/* ber.c -- shared length codec, BER TLV, and APDU dispatch. */
#include "dlms_codec.h"

const char *dlms_strerror(dlms_status s) {
    switch (s) {
        case DLMS_OK:              return "ok";
        case DLMS_ERR_TRUNCATED:   return "truncated input";
        case DLMS_ERR_OVERFLOW:    return "output overflow";
        case DLMS_ERR_BAD_LENGTH:  return "bad length field";
        case DLMS_ERR_UNSUPPORTED: return "unsupported encoding";
        case DLMS_ERR_BAD_TAG:     return "bad/unexpected tag";
        case DLMS_ERR_RANGE:       return "value out of range";
        default:                   return "?";
    }
}

/* --- low-level byte I/O ------------------------------------------------- */
static dlms_status w_u8(dlms_writer *w, uint8_t b) {
    if (w->pos >= w->cap) return DLMS_ERR_OVERFLOW;
    w->buf[w->pos++] = b;
    return DLMS_OK;
}
static dlms_status w_bytes(dlms_writer *w, const uint8_t *p, size_t n) {
    if (n > w->cap - w->pos) return DLMS_ERR_OVERFLOW;
    for (size_t i = 0; i < n; i++) w->buf[w->pos++] = p[i];
    return DLMS_OK;
}
static dlms_status r_u8(dlms_reader *r, uint8_t *b) {
    if (dlms_remaining(r) < 1) return DLMS_ERR_TRUNCATED;
    *b = r->buf[r->pos++];
    return DLMS_OK;
}

/* Minimal number of octets to represent a 32-bit value big-endian. */
static uint8_t min_octets_u32(uint32_t v) {
    if (v <= 0xFF)     return 1;
    if (v <= 0xFFFF)   return 2;
    if (v <= 0xFFFFFF) return 3;
    return 4;
}

/* ============================ length codec ============================= */
dlms_status dlms_write_len(dlms_writer *w, uint32_t len) {
    if (len < 0x80) return w_u8(w, (uint8_t)len);
    uint8_t n = min_octets_u32(len);
    dlms_status s = w_u8(w, (uint8_t)(0x80 | n));
    for (int i = n - 1; i >= 0 && s == DLMS_OK; i--)
        s = w_u8(w, (uint8_t)(len >> (8 * i)));
    return s;
}

dlms_status ber_read_len(dlms_reader *r, uint32_t *len_out, bool *indefinite) {
    uint8_t b;
    dlms_status s = r_u8(r, &b);
    if (s) return s;
    *indefinite = false;
    if (b < 0x80) { *len_out = b; return DLMS_OK; }
    uint8_t n = b & 0x7F;
    if (n == 0) { *indefinite = true; *len_out = 0; return DLMS_OK; } /* 0x80 */
    if (n > 4)  return DLMS_ERR_BAD_LENGTH; /* > 2^32 not supported here */
    if (dlms_remaining(r) < n) return DLMS_ERR_TRUNCATED;
    uint32_t v = 0;
    for (uint8_t i = 0; i < n; i++) v = (v << 8) | r->buf[r->pos++];
    *len_out = v;
    return DLMS_OK;
}

dlms_status dlms_read_len(dlms_reader *r, uint32_t *len_out) {
    bool indef;
    dlms_status s = ber_read_len(r, len_out, &indef);
    if (s) return s;
    if (indef) return DLMS_ERR_UNSUPPORTED; /* A-XDR forbids indefinite */
    return DLMS_OK;
}

/* ============================ BER tag ================================== */
dlms_status ber_write_tag(dlms_writer *w, ber_tag tag) {
    uint8_t lead = (uint8_t)(tag.cls & 0xC0);
    if (tag.constructed) lead |= 0x20;
    if (tag.number < 31) {
        return w_u8(w, (uint8_t)(lead | tag.number));
    }
    /* high-tag-number form: lead|0x1F, then base-128 big-endian, MSB=1 except last */
    dlms_status s = w_u8(w, (uint8_t)(lead | 0x1F));
    if (s) return s;
    uint8_t tmp[5]; int n = 0;
    uint32_t v = tag.number;
    do { tmp[n++] = (uint8_t)(v & 0x7F); v >>= 7; } while (v);
    for (int i = n - 1; i >= 0 && s == DLMS_OK; i--)
        s = w_u8(w, (uint8_t)(tmp[i] | (i ? 0x80 : 0x00)));
    return s;
}

dlms_status ber_read_tag(dlms_reader *r, ber_tag *tag) {
    uint8_t b;
    dlms_status s = r_u8(r, &b);
    if (s) return s;
    tag->cls = (uint8_t)(b & 0xC0);
    tag->constructed = (b & 0x20) != 0;
    if ((b & 0x1F) != 0x1F) { tag->number = b & 0x1F; return DLMS_OK; }
    uint32_t v = 0; int cnt = 0;
    do {
        s = r_u8(r, &b);
        if (s) return s;
        if (++cnt > 5) return DLMS_ERR_BAD_TAG; /* > 32-bit tag number */
        v = (v << 7) | (b & 0x7F);
    } while (b & 0x80);
    tag->number = v;
    return DLMS_OK;
}

dlms_status ber_read_tlv(dlms_reader *r, ber_tag *tag,
                         const uint8_t **val, size_t *val_len,
                         bool *indefinite) {
    dlms_status s = ber_read_tag(r, tag);
    if (s) return s;
    uint32_t len;
    s = ber_read_len(r, &len, indefinite);
    if (s) return s;
    if (*indefinite) {
        /* Caller must locate the matching end-of-contents. We expose the
         * remaining span; nesting is the caller's responsibility. */
        *val = r->buf + r->pos;
        *val_len = dlms_remaining(r);
        return DLMS_OK;
    }
    if (dlms_remaining(r) < len) return DLMS_ERR_TRUNCATED;
    *val = r->buf + r->pos;
    *val_len = len;
    r->pos += len;
    return DLMS_OK;
}

/* ------------------------- constructed helpers ------------------------- */
dlms_status ber_begin(dlms_writer *w, ber_tag tag, uint8_t width, ber_frame *hdr) {
    if (width < 1 || width > 4) return DLMS_ERR_RANGE;
    dlms_status s = ber_write_tag(w, tag);
    if (s) return s;
    if (w_u8(w, (uint8_t)(0x80 | width))) return DLMS_ERR_OVERFLOW;
    hdr->len_pos = w->pos;
    hdr->width = width;
    /* reserve `width` placeholder octets */
    for (uint8_t i = 0; i < width; i++)
        if (w_u8(w, 0x00)) return DLMS_ERR_OVERFLOW;
    return DLMS_OK;
}

dlms_status ber_end(dlms_writer *w, const ber_frame *hdr) {
    size_t content = w->pos - (hdr->len_pos + hdr->width);
    /* ensure it fits the reserved width */
    if (hdr->width < 4 && content >= ((size_t)1 << (8 * hdr->width)))
        return DLMS_ERR_RANGE;
    for (uint8_t i = 0; i < hdr->width; i++)
        w->buf[hdr->len_pos + i] =
            (uint8_t)(content >> (8 * (hdr->width - 1 - i)));
    return DLMS_OK;
}

dlms_status ber_write_primitive(dlms_writer *w, ber_tag tag,
                                const uint8_t *val, size_t val_len) {
    if (val_len > 0xFFFFFFFFu) return DLMS_ERR_RANGE;
    dlms_status s = ber_write_tag(w, tag);
    if (s) return s;
    s = dlms_write_len(w, (uint32_t)val_len);
    if (s) return s;
    return w_bytes(w, val, val_len);
}

/* ============================ dispatch ================================= */
dlms_encoding dlms_encoding_for_apdu(uint8_t first_tag) {
    switch (first_tag) {
        case ACSE_AARQ: case ACSE_AARE:
        case ACSE_RLRQ: case ACSE_RLRE:
            return DLMS_ENC_BER_ACSE;
        /* xDLMS service APDUs are A-XDR. Common tags: */
        case 0x01: /* initiate-request              */
        case 0x08: /* initiate-response             */
        case 0x0F: /* data-notification             */
        case 0xC0: /* get-request                   */
        case 0xC1: /* get-response                  */
        case 0xC2: /* set-request                   */
        case 0xC3: /* set-response                  */
        case 0xC4: /* action-request... etc.        */
        case 0xC5:
        case 0xC6:
        case 0xC7:
        case 0xC8:
            return DLMS_ENC_AXDR_XDLMS;
        default:
            return DLMS_ENC_UNKNOWN;
    }
}
