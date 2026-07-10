/* axdr.c -- A-XDR codec for the COSEM `Data` type. */
#include <string.h>
#include "dlms_codec.h"

/* ---- byte helpers (duplicated small statics keep the file standalone) --- */
static dlms_status w8(dlms_writer *w, uint8_t b) {
    if (w->pos >= w->cap) return DLMS_ERR_OVERFLOW;
    w->buf[w->pos++] = b; return DLMS_OK;
}
static dlms_status wN(dlms_writer *w, const uint8_t *p, size_t n) {
    if (n > w->cap - w->pos) return DLMS_ERR_OVERFLOW;
    memcpy(w->buf + w->pos, p, n); w->pos += n; return DLMS_OK;
}
static dlms_status r8(dlms_reader *r, uint8_t *b) {
    if (dlms_remaining(r) < 1) return DLMS_ERR_TRUNCATED;
    *b = r->buf[r->pos++]; return DLMS_OK;
}
static dlms_status take(dlms_reader *r, size_t n, const uint8_t **p) {
    if (dlms_remaining(r) < n) return DLMS_ERR_TRUNCATED;
    *p = r->buf + r->pos; r->pos += n; return DLMS_OK;
}

/* big-endian integer writers/readers */
static dlms_status wbe(dlms_writer *w, uint64_t v, int n) {
    for (int i = n - 1; i >= 0; i--) { dlms_status s = w8(w, (uint8_t)(v >> (8*i))); if (s) return s; }
    return DLMS_OK;
}
static dlms_status rbe(dlms_reader *r, uint64_t *v, int n) {
    if (dlms_remaining(r) < (size_t)n) return DLMS_ERR_TRUNCATED;
    uint64_t x = 0;
    for (int i = 0; i < n; i++) x = (x << 8) | r->buf[r->pos++];
    *v = x; return DLMS_OK;
}

/* IEEE-754 <-> big-endian bytes via bit-pattern copy. */
static dlms_status w_f32(dlms_writer *w, float f) {
    uint32_t u; memcpy(&u, &f, 4); return wbe(w, u, 4);
}
static dlms_status w_f64(dlms_writer *w, double f) {
    uint64_t u; memcpy(&u, &f, 8); return wbe(w, u, 8);
}
static dlms_status r_f32(dlms_reader *r, float *f) {
    uint64_t u; dlms_status s = rbe(r, &u, 4); if (s) return s;
    uint32_t u32 = (uint32_t)u; memcpy(f, &u32, 4); return DLMS_OK;
}
static dlms_status r_f64(dlms_reader *r, double *f) {
    uint64_t u; dlms_status s = rbe(r, &u, 8); if (s) return s;
    memcpy(f, &u, 8); return DLMS_OK;
}

/* ============================ ENCODE ================================== */
dlms_status axdr_encode_data(dlms_writer *w, const axdr_data *d) {
    dlms_status s = w8(w, (uint8_t)d->type);
    if (s) return s;
    switch (d->type) {
        case AXDR_NULL:
            return DLMS_OK;

        case AXDR_BOOLEAN:
            return w8(w, d->u.boolean ? 0x01 : 0x00);

        case AXDR_BCD:      case AXDR_INTEGER:
            return w8(w, (uint8_t)d->u.i8);
        case AXDR_UNSIGNED: case AXDR_ENUM:
            return w8(w, d->u.u8);

        case AXDR_LONG:     return wbe(w, (uint16_t)d->u.i16, 2);
        case AXDR_LONG_U:   return wbe(w, d->u.u16, 2);
        case AXDR_DOUBLE_LONG:   return wbe(w, (uint32_t)d->u.i32, 4);
        case AXDR_DOUBLE_LONG_U: return wbe(w, d->u.u32, 4);
        case AXDR_LONG64:   return wbe(w, (uint64_t)d->u.i64, 8);
        case AXDR_LONG64_U: return wbe(w, d->u.u64, 8);

        case AXDR_FLOAT32:  return w_f32(w, d->u.f32);
        case AXDR_FLOAT64:  return w_f64(w, d->u.f64);

        case AXDR_OCTET_STRING:
        case AXDR_VISIBLE_STRING:
        case AXDR_UTF8_STRING:
            s = dlms_write_len(w, (uint32_t)d->u.bytes.len);
            if (s) return s;
            return wN(w, d->u.bytes.ptr, d->u.bytes.len);

        case AXDR_BIT_STRING: {              /* len is a BIT count */
            size_t bits = d->u.bytes.len;
            size_t octs = (bits + 7) / 8;
            s = dlms_write_len(w, (uint32_t)bits);
            if (s) return s;
            return wN(w, d->u.bytes.ptr, octs);
        }

        case AXDR_DATE_TIME:  return wN(w, d->u.bytes.ptr, AXDR_DATE_TIME_LEN);
        case AXDR_DATE:       return wN(w, d->u.bytes.ptr, AXDR_DATE_LEN);
        case AXDR_TIME:       return wN(w, d->u.bytes.ptr, AXDR_TIME_LEN);

        case AXDR_ARRAY:
        case AXDR_STRUCTURE:
            s = dlms_write_len(w, (uint32_t)d->u.list.count);
            if (s) return s;
            for (size_t i = 0; i < d->u.list.count; i++) {
                s = axdr_encode_data(w, &d->u.list.items[i]);
                if (s) return s;
            }
            return DLMS_OK;

        case AXDR_COMPACT_ARRAY:
        default:
            return DLMS_ERR_UNSUPPORTED;
    }
}

/* ============================ DECODE ================================== */
/* Decode a non-container value (the type tag is already known / consumed).
 * Fills `out` (an axdr_data). Null and dont-care read no bytes. */
static dlms_status decode_leaf(dlms_reader *r, axdr_type t, axdr_data *out) {
    uint64_t v = 0; uint32_t len; const uint8_t *p; dlms_status s;
    out->type = t;
    switch (t) {
        case AXDR_NULL: return DLMS_OK;
        case AXDR_BOOLEAN: { uint8_t b; s = r8(r,&b); if(s) return s;
            out->u.boolean = (b != 0); return DLMS_OK; }
        case AXDR_BCD: case AXDR_INTEGER: { uint8_t b; s = r8(r,&b); if(s) return s;
            out->u.i8 = (int8_t)b; return DLMS_OK; }
        case AXDR_UNSIGNED: case AXDR_ENUM: { uint8_t b; s = r8(r,&b); if(s) return s;
            out->u.u8 = b; return DLMS_OK; }
        case AXDR_LONG:   s = rbe(r,&v,2); out->u.i16 = (int16_t)v; return s;
        case AXDR_LONG_U: s = rbe(r,&v,2); out->u.u16 = (uint16_t)v; return s;
        case AXDR_DOUBLE_LONG:   s = rbe(r,&v,4); out->u.i32 = (int32_t)v; return s;
        case AXDR_DOUBLE_LONG_U: s = rbe(r,&v,4); out->u.u32 = (uint32_t)v; return s;
        case AXDR_LONG64:   s = rbe(r,&v,8); out->u.i64 = (int64_t)v; return s;
        case AXDR_LONG64_U: s = rbe(r,&v,8); out->u.u64 = v; return s;
        case AXDR_FLOAT32: return r_f32(r, &out->u.f32);
        case AXDR_FLOAT64: return r_f64(r, &out->u.f64);
        case AXDR_OCTET_STRING: case AXDR_VISIBLE_STRING: case AXDR_UTF8_STRING:
            s = dlms_read_len(r,&len); if(s) return s;
            s = take(r,len,&p); if(s) return s;
            out->u.bytes.ptr = p; out->u.bytes.len = len; return DLMS_OK;
        case AXDR_BIT_STRING:
            s = dlms_read_len(r,&len); if(s) return s;
            s = take(r,(len+7)/8,&p); if(s) return s;
            out->u.bytes.ptr = p; out->u.bytes.len = len; return DLMS_OK;
        case AXDR_DATE_TIME: s = take(r,AXDR_DATE_TIME_LEN,&p); if(s) return s;
            out->u.bytes.ptr = p; out->u.bytes.len = AXDR_DATE_TIME_LEN; return DLMS_OK;
        case AXDR_DATE: s = take(r,AXDR_DATE_LEN,&p); if(s) return s;
            out->u.bytes.ptr = p; out->u.bytes.len = AXDR_DATE_LEN; return DLMS_OK;
        case AXDR_TIME: s = take(r,AXDR_TIME_LEN,&p); if(s) return s;
            out->u.bytes.ptr = p; out->u.bytes.len = AXDR_TIME_LEN; return DLMS_OK;
        default: return DLMS_ERR_UNSUPPORTED;
    }
}

/* Advance a reader past one TypeDescription (used to bound the description). */
static dlms_status skip_type_desc(dlms_reader *r) {
    uint8_t t; dlms_status s = r8(r, &t); if (s) return s;
    if (t == AXDR_STRUCTURE) {
        uint32_t n; s = dlms_read_len(r, &n); if (s) return s;
        for (uint32_t i = 0; i < n; i++) { s = skip_type_desc(r); if (s) return s; }
    } else if (t == AXDR_ARRAY) {
        uint64_t dummy; s = rbe(r, &dummy, 2); if (s) return s; /* number-of-elements */
        s = skip_type_desc(r);
    }
    return s;
}

dlms_status axdr_decode_data(dlms_reader *r, axdr_header *out) {
    uint8_t tag;
    dlms_status s = r8(r, &tag);
    if (s) return s;
    memset(out, 0, sizeof(*out));
    out->type = (axdr_type)tag;
    out->value.type = (axdr_type)tag;

    if (tag == AXDR_ARRAY || tag == AXDR_STRUCTURE) {
        uint32_t len; s = dlms_read_len(r, &len); if (s) return s;
        out->count = len; return DLMS_OK; /* reader at first child */
    }
    if (tag == AXDR_COMPACT_ARRAY) {
        size_t desc_start = r->pos;
        s = skip_type_desc(r); if (s) return s;
        out->compact.desc = r->buf + desc_start;
        out->compact.desc_len = r->pos - desc_start;
        uint32_t clen; s = dlms_read_len(r, &clen); if (s) return s;
        const uint8_t *cp; s = take(r, clen, &cp); if (s) return s;
        out->compact.contents = cp; out->compact.contents_len = clen;
        return DLMS_OK;
    }
    return decode_leaf(r, (axdr_type)tag, &out->value);
}

/* ---- compact-array element walk (guided by the TypeDescription) -------- */
static dlms_status walk_desc(dlms_reader *dr, dlms_reader *cr,
                             axdr_visit_fn fn, void *ctx, int depth) {
    uint8_t t; dlms_status s = r8(dr, &t); if (s) return s;
    axdr_header h; memset(&h, 0, sizeof h);
    if (t == AXDR_STRUCTURE) {
        uint32_t n; s = dlms_read_len(dr, &n); if (s) return s;
        h.type = AXDR_STRUCTURE; h.count = n;
        s = fn(&h, depth, ctx); if (s) return s;
        for (uint32_t i = 0; i < n; i++) { s = walk_desc(dr, cr, fn, ctx, depth+1); if (s) return s; }
        return DLMS_OK;
    }
    if (t == AXDR_ARRAY) {
        uint64_t n; s = rbe(dr, &n, 2); if (s) return s;   /* number-of-elements */
        size_t elem_desc = dr->pos;                         /* one element description */
        h.type = AXDR_ARRAY; h.count = (size_t)n;
        s = fn(&h, depth, ctx); if (s) return s;
        for (uint64_t i = 0; i < n; i++) {
            dr->pos = elem_desc;                            /* re-read per element */
            s = walk_desc(dr, cr, fn, ctx, depth+1); if (s) return s;
        }
        return DLMS_OK;
    }
    /* leaf: pull the value from the packed contents */
    h.type = (axdr_type)t;
    s = decode_leaf(cr, (axdr_type)t, &h.value); if (s) return s;
    return fn(&h, depth, ctx);
}

static dlms_status compact_walk(const axdr_header *h, axdr_visit_fn fn, void *ctx, int base) {
    dlms_reader dr, cr;
    dlms_reader_init(&dr, h->compact.desc, h->compact.desc_len);
    dlms_reader_init(&cr, h->compact.contents, h->compact.contents_len);
    while (dlms_remaining(&cr) > 0) {
        dr.pos = 0;                                         /* re-read description per element */
        dlms_status s = walk_desc(&dr, &cr, fn, ctx, base);
        if (s) return s;
    }
    return DLMS_OK;
}

dlms_status axdr_compact_array_walk(const axdr_header *h, axdr_visit_fn fn, void *ctx) {
    if (h->type != AXDR_COMPACT_ARRAY) return DLMS_ERR_BAD_TAG;
    return compact_walk(h, fn, ctx, 0);
}

static dlms_status walk_rec(dlms_reader *r, axdr_visit_fn fn, void *ctx, int depth) {
    axdr_header h;
    dlms_status s = axdr_decode_data(r, &h);
    if (s) return s;
    s = fn(&h, depth, ctx);
    if (s) return s;
    if (h.type == AXDR_ARRAY || h.type == AXDR_STRUCTURE) {
        for (size_t i = 0; i < h.count; i++) {
            s = walk_rec(r, fn, ctx, depth + 1);
            if (s) return s;
        }
    } else if (h.type == AXDR_COMPACT_ARRAY) {
        s = compact_walk(&h, fn, ctx, depth + 1);
        if (s) return s;
    }
    return DLMS_OK;
}

dlms_status axdr_walk(dlms_reader *r, axdr_visit_fn fn, void *ctx) {
    return walk_rec(r, fn, ctx, 0);
}

/* ===================== compact-array ENCODE ========================== */
/* minimal octets to hold a length value (mirrors ber.c) */
static size_t len_of_len(uint32_t v) {
    if (v < 0x80) return 1;
    if (v <= 0xFF) return 2;
    if (v <= 0xFFFF) return 3;
    if (v <= 0xFFFFFF) return 4;
    return 5;
}

/* byte size of one value packed tagless (per compact-array rules) */
static size_t packed_size(const axdr_data *d) {
    switch (d->type) {
        case AXDR_NULL: return 0;
        case AXDR_BOOLEAN: case AXDR_BCD: case AXDR_INTEGER:
        case AXDR_UNSIGNED: case AXDR_ENUM: return 1;
        case AXDR_LONG: case AXDR_LONG_U: return 2;
        case AXDR_DOUBLE_LONG: case AXDR_DOUBLE_LONG_U: case AXDR_FLOAT32: return 4;
        case AXDR_LONG64: case AXDR_LONG64_U: case AXDR_FLOAT64: return 8;
        case AXDR_DATE_TIME: return AXDR_DATE_TIME_LEN;
        case AXDR_DATE: return AXDR_DATE_LEN;
        case AXDR_TIME: return AXDR_TIME_LEN;
        case AXDR_OCTET_STRING: case AXDR_VISIBLE_STRING: case AXDR_UTF8_STRING:
            return len_of_len((uint32_t)d->u.bytes.len) + d->u.bytes.len;
        case AXDR_BIT_STRING:
            return len_of_len((uint32_t)d->u.bytes.len) + (d->u.bytes.len + 7) / 8;
        case AXDR_ARRAY: case AXDR_STRUCTURE: {
            size_t sum = 0;
            for (size_t i = 0; i < d->u.list.count; i++) sum += packed_size(&d->u.list.items[i]);
            return sum;
        }
        default: return 0;
    }
}

/* write one value tagless */
static dlms_status pack_value(dlms_writer *w, const axdr_data *d) {
    switch (d->type) {
        case AXDR_NULL: return DLMS_OK;
        case AXDR_BOOLEAN: return w8(w, d->u.boolean ? 1 : 0);
        case AXDR_BCD: case AXDR_INTEGER: return w8(w, (uint8_t)d->u.i8);
        case AXDR_UNSIGNED: case AXDR_ENUM: return w8(w, d->u.u8);
        case AXDR_LONG:   return wbe(w, (uint16_t)d->u.i16, 2);
        case AXDR_LONG_U: return wbe(w, d->u.u16, 2);
        case AXDR_DOUBLE_LONG:   return wbe(w, (uint32_t)d->u.i32, 4);
        case AXDR_DOUBLE_LONG_U: return wbe(w, d->u.u32, 4);
        case AXDR_LONG64:   return wbe(w, (uint64_t)d->u.i64, 8);
        case AXDR_LONG64_U: return wbe(w, d->u.u64, 8);
        case AXDR_FLOAT32:  return w_f32(w, d->u.f32);
        case AXDR_FLOAT64:  return w_f64(w, d->u.f64);
        case AXDR_OCTET_STRING: case AXDR_VISIBLE_STRING: case AXDR_UTF8_STRING: {
            dlms_status s = dlms_write_len(w, (uint32_t)d->u.bytes.len); if (s) return s;
            return wN(w, d->u.bytes.ptr, d->u.bytes.len);
        }
        case AXDR_BIT_STRING: {
            dlms_status s = dlms_write_len(w, (uint32_t)d->u.bytes.len); if (s) return s;
            return wN(w, d->u.bytes.ptr, (d->u.bytes.len + 7) / 8);
        }
        case AXDR_DATE_TIME: return wN(w, d->u.bytes.ptr, AXDR_DATE_TIME_LEN);
        case AXDR_DATE:      return wN(w, d->u.bytes.ptr, AXDR_DATE_LEN);
        case AXDR_TIME:      return wN(w, d->u.bytes.ptr, AXDR_TIME_LEN);
        case AXDR_ARRAY: case AXDR_STRUCTURE: {
            for (size_t i = 0; i < d->u.list.count; i++) {
                dlms_status s = pack_value(w, &d->u.list.items[i]); if (s) return s;
            }
            return DLMS_OK;
        }
        default: return DLMS_ERR_UNSUPPORTED;
    }
}

/* write the TypeDescription for the shape of `d` */
static dlms_status write_type_desc(dlms_writer *w, const axdr_data *d) {
    dlms_status s;
    switch (d->type) {
        case AXDR_STRUCTURE:
            s = w8(w, AXDR_STRUCTURE); if (s) return s;
            s = dlms_write_len(w, (uint32_t)d->u.list.count); if (s) return s;
            for (size_t i = 0; i < d->u.list.count; i++) {
                s = write_type_desc(w, &d->u.list.items[i]); if (s) return s;
            }
            return DLMS_OK;
        case AXDR_ARRAY:
            if (d->u.list.count == 0) return DLMS_ERR_RANGE; /* need a template element */
            s = w8(w, AXDR_ARRAY); if (s) return s;
            s = wbe(w, (uint16_t)d->u.list.count, 2); if (s) return s; /* number-of-elements */
            return write_type_desc(w, &d->u.list.items[0]);
        default:
            return w8(w, (uint8_t)d->type); /* scalar / string / date: tag only */
    }
}

dlms_status axdr_encode_compact_array(dlms_writer *w, const axdr_data *arr) {
    if (arr->type != AXDR_ARRAY || arr->u.list.count == 0) return DLMS_ERR_RANGE;
    dlms_status s = w8(w, AXDR_COMPACT_ARRAY); if (s) return s;
    s = write_type_desc(w, &arr->u.list.items[0]); if (s) return s;
    size_t total = 0;
    for (size_t i = 0; i < arr->u.list.count; i++) total += packed_size(&arr->u.list.items[i]);
    if (total > 0xFFFFFFFFu) return DLMS_ERR_RANGE;
    s = dlms_write_len(w, (uint32_t)total); if (s) return s;
    for (size_t i = 0; i < arr->u.list.count; i++) {
        s = pack_value(w, &arr->u.list.items[i]); if (s) return s;
    }
    return DLMS_OK;
}
