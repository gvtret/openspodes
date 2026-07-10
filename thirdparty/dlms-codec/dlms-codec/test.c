/* test.c -- correctness tests for the DLMS BER + A-XDR + xDLMS codecs.
 *
 * Three layers of checking, because each catches what the others miss:
 *   1. GOLDEN vectors  -- encode and compare to hand-verified bytes. Catches
 *      symmetric bugs that round-trip tests hide.
 *   2. ROUND-TRIP      -- decode the golden bytes and re-encode; must match.
 *      Catches decoder bugs.
 *   3. ERROR paths     -- truncation, overflow, bad tags, unsupported types.
 *
 * Exit status is non-zero if any check fails (CI-friendly).
 */
#include <stdio.h>
#include <string.h>
#include "xdlms.h"

static int g_pass = 0, g_fail = 0;

static void fail(const char *what, const char *file, int line) {
    printf("  FAIL: %-40s (%s:%d)\n", what, file, line);
    g_fail++;
}
#define OK(cond, what) do { if (cond) g_pass++; else fail(what, __FILE__, __LINE__); } while (0)

static void dump(const char *tag, const uint8_t *b, size_t n) {
    printf("    %s", tag);
    for (size_t i = 0; i < n; i++) printf(" %02X", b[i]);
    printf("\n");
}

static int bytes_eq(const char *what, const uint8_t *got, size_t got_n,
                    const uint8_t *exp, size_t exp_n) {
    if (got_n == exp_n && memcmp(got, exp, exp_n) == 0) { g_pass++; return 1; }
    fail(what, __FILE__, __LINE__);
    dump("got:", got, got_n);
    dump("exp:", exp, exp_n);
    return 0;
}

/* ============================================================ length codec */
static void t_length(void) {
    puts("length codec:");
    struct { uint32_t v; uint8_t exp[5]; size_t n; } cases[] = {
        { 0,          {0x00},                   1 },
        { 127,        {0x7F},                   1 },
        { 128,        {0x81,0x80},              2 },
        { 255,        {0x81,0xFF},              2 },
        { 256,        {0x82,0x01,0x00},         3 },
        { 65535,      {0x82,0xFF,0xFF},         3 },
        { 65536,      {0x83,0x01,0x00,0x00},    4 },
        { 0x01020304, {0x84,0x01,0x02,0x03,0x04},5 },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        uint8_t buf[8]; dlms_writer w; dlms_writer_init(&w, buf, sizeof buf);
        OK(dlms_write_len(&w, cases[i].v) == DLMS_OK, "write_len");
        bytes_eq("write_len golden", buf, w.pos, cases[i].exp, cases[i].n);
        dlms_reader r; dlms_reader_init(&r, buf, w.pos);
        uint32_t back;
        OK(dlms_read_len(&r, &back) == DLMS_OK && back == cases[i].v && r.pos == w.pos,
           "read_len round-trip");
    }
    /* indefinite form is BER-only */
    uint8_t indef[] = {0x80};
    dlms_reader r; dlms_reader_init(&r, indef, 1); uint32_t len; bool ind;
    OK(ber_read_len(&r, &len, &ind) == DLMS_OK && ind, "ber indefinite ok");
    dlms_reader_init(&r, indef, 1);
    OK(dlms_read_len(&r, &len) == DLMS_ERR_UNSUPPORTED, "axdr rejects indefinite");
}

/* ===================================================== A-XDR Data scalars */
/* Encode d, compare to golden; then decode golden and re-encode -> must match. */
static void t_scalar(const char *name, axdr_data d, const uint8_t *exp, size_t n) {
    uint8_t buf[32]; dlms_writer w; dlms_writer_init(&w, buf, sizeof buf);
    OK(axdr_encode_data(&w, &d) == DLMS_OK, name);
    bytes_eq(name, buf, w.pos, exp, n);

    dlms_reader r; dlms_reader_init(&r, buf, w.pos);
    axdr_header h;
    OK(axdr_decode_data(&r, &h) == DLMS_OK && r.pos == w.pos && h.type == d.type,
       name);
    uint8_t buf2[32]; dlms_writer w2; dlms_writer_init(&w2, buf2, sizeof buf2);
    OK(axdr_encode_data(&w2, &h.value) == DLMS_OK, name);
    bytes_eq(name, buf2, w2.pos, exp, n);
}

static void t_data_scalars(void) {
    puts("A-XDR Data scalars:");
    static const uint8_t octets[4] = {0xDE,0xAD,0xBE,0xEF};
    static const uint8_t str[2]    = {'A','B'};
    static const uint8_t bits[1]   = {0xA0};      /* "1010...." */
    static const uint8_t dt12[12]  = {0x07,0xE6,0x01,0x0A,0x02,0x0C,0x1E,0x00,0xFF,0x88,0x80,0x00};
    static const uint8_t d5[5]     = {0x07,0xE6,0x01,0x0A};      /* 4 shown; filled below */
    (void)d5;

    axdr_data d;

    d = (axdr_data){ .type = AXDR_NULL };
    t_scalar("null", d, (uint8_t[]){0x00}, 1);

    d = (axdr_data){ .type = AXDR_BOOLEAN }; d.u.boolean = true;
    t_scalar("boolean true", d, (uint8_t[]){0x03,0x01}, 2);
    d.u.boolean = false;
    t_scalar("boolean false", d, (uint8_t[]){0x03,0x00}, 2);

    d = (axdr_data){ .type = AXDR_BIT_STRING };
    d.u.bytes.ptr = bits; d.u.bytes.len = 4; /* 4 bits */
    t_scalar("bit-string", d, (uint8_t[]){0x04,0x04,0xA0}, 3);

    d = (axdr_data){ .type = AXDR_DOUBLE_LONG }; d.u.i32 = -2;
    t_scalar("double-long", d, (uint8_t[]){0x05,0xFF,0xFF,0xFF,0xFE}, 5);

    d = (axdr_data){ .type = AXDR_DOUBLE_LONG_U }; d.u.u32 = 123456;
    t_scalar("double-long-unsigned", d, (uint8_t[]){0x06,0x00,0x01,0xE2,0x40}, 5);

    d = (axdr_data){ .type = AXDR_OCTET_STRING };
    d.u.bytes.ptr = octets; d.u.bytes.len = 4;
    t_scalar("octet-string", d, (uint8_t[]){0x09,0x04,0xDE,0xAD,0xBE,0xEF}, 6);

    d = (axdr_data){ .type = AXDR_VISIBLE_STRING };
    d.u.bytes.ptr = str; d.u.bytes.len = 2;
    t_scalar("visible-string", d, (uint8_t[]){0x0A,0x02,0x41,0x42}, 4);

    d = (axdr_data){ .type = AXDR_UTF8_STRING };
    d.u.bytes.ptr = str; d.u.bytes.len = 2;
    t_scalar("utf8-string", d, (uint8_t[]){0x0C,0x02,0x41,0x42}, 4);

    d = (axdr_data){ .type = AXDR_BCD }; d.u.i8 = 0x12;
    t_scalar("bcd", d, (uint8_t[]){0x0D,0x12}, 2);

    d = (axdr_data){ .type = AXDR_INTEGER }; d.u.i8 = -1;
    t_scalar("integer", d, (uint8_t[]){0x0F,0xFF}, 2);

    d = (axdr_data){ .type = AXDR_LONG }; d.u.i16 = -2;
    t_scalar("long", d, (uint8_t[]){0x10,0xFF,0xFE}, 3);

    d = (axdr_data){ .type = AXDR_UNSIGNED }; d.u.u8 = 200;
    t_scalar("unsigned", d, (uint8_t[]){0x11,0xC8}, 2);

    d = (axdr_data){ .type = AXDR_LONG_U }; d.u.u16 = 1000;
    t_scalar("long-unsigned", d, (uint8_t[]){0x12,0x03,0xE8}, 3);

    d = (axdr_data){ .type = AXDR_LONG64 }; d.u.i64 = -2;
    t_scalar("long64", d, (uint8_t[]){0x14,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE}, 9);

    d = (axdr_data){ .type = AXDR_LONG64_U }; d.u.u64 = 1;
    t_scalar("long64-unsigned", d, (uint8_t[]){0x15,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01}, 9);

    d = (axdr_data){ .type = AXDR_ENUM }; d.u.u8 = 30;
    t_scalar("enum", d, (uint8_t[]){0x16,0x1E}, 2);

    d = (axdr_data){ .type = AXDR_FLOAT32 }; d.u.f32 = 1.0f;
    t_scalar("float32 1.0", d, (uint8_t[]){0x17,0x3F,0x80,0x00,0x00}, 5);

    d = (axdr_data){ .type = AXDR_FLOAT64 }; d.u.f64 = 1.0;
    t_scalar("float64 1.0", d, (uint8_t[]){0x18,0x3F,0xF0,0x00,0x00,0x00,0x00,0x00,0x00}, 9);

    d = (axdr_data){ .type = AXDR_DATE_TIME }; d.u.bytes.ptr = dt12; d.u.bytes.len = 12;
    {
        uint8_t exp[13]; exp[0] = 0x19; memcpy(exp+1, dt12, 12);
        t_scalar("date-time", d, exp, 13);
    }

    /* non-bit-preserving float sanity: round-trip an awkward value */
    d = (axdr_data){ .type = AXDR_FLOAT32 }; d.u.f32 = 3.14159f;
    uint8_t fb[8]; dlms_writer fw; dlms_writer_init(&fw, fb, sizeof fb);
    axdr_encode_data(&fw, &d);
    dlms_reader fr; dlms_reader_init(&fr, fb, fw.pos); axdr_header fh;
    axdr_decode_data(&fr, &fh);
    OK(fh.value.u.f32 == 3.14159f, "float32 exact round-trip");
}

/* ================================================== A-XDR containers/trace */
typedef struct { char *p; size_t cap; size_t len; } trace_buf;

static dlms_status trace_visit(const axdr_header *h, int depth, void *ctx) {
    trace_buf *tb = ctx;
    int w;
    switch (h->type) {
        case AXDR_ARRAY: case AXDR_STRUCTURE:
            w = snprintf(tb->p + tb->len, tb->cap - tb->len, "%d:%d#%zu;",
                         depth, (int)h->type, h->count); break;
        case AXDR_COMPACT_ARRAY:
            w = snprintf(tb->p + tb->len, tb->cap - tb->len, "%d:19*;", depth); break;
        case AXDR_UNSIGNED: case AXDR_ENUM:
            w = snprintf(tb->p + tb->len, tb->cap - tb->len, "%d:%d=%u;",
                         depth, (int)h->type, h->value.u.u8); break;
        case AXDR_LONG_U:
            w = snprintf(tb->p + tb->len, tb->cap - tb->len, "%d:%d=%u;",
                         depth, (int)h->type, h->value.u.u16); break;
        case AXDR_DOUBLE_LONG_U:
            w = snprintf(tb->p + tb->len, tb->cap - tb->len, "%d:%d=%u;",
                         depth, (int)h->type, h->value.u.u32); break;
        case AXDR_BOOLEAN:
            w = snprintf(tb->p + tb->len, tb->cap - tb->len, "%d:%d=%d;",
                         depth, (int)h->type, h->value.u.boolean); break;
        default:
            w = snprintf(tb->p + tb->len, tb->cap - tb->len, "%d:%d?;",
                         depth, (int)h->type); break;
    }
    if (w < 0 || (size_t)w >= tb->cap - tb->len) return DLMS_ERR_OVERFLOW;
    tb->len += (size_t)w;
    return DLMS_OK;
}

static void t_data_containers(void) {
    puts("A-XDR Data containers:");

    /* array of 2 unsigned {1,2} */
    axdr_data ua[2];
    ua[0] = (axdr_data){ .type = AXDR_UNSIGNED }; ua[0].u.u8 = 1;
    ua[1] = (axdr_data){ .type = AXDR_UNSIGNED }; ua[1].u.u8 = 2;
    axdr_data arr = { .type = AXDR_ARRAY }; arr.u.list.items = ua; arr.u.list.count = 2;
    uint8_t b1[16]; dlms_writer w1; dlms_writer_init(&w1, b1, sizeof b1);
    OK(axdr_encode_data(&w1, &arr) == DLMS_OK, "array encode");
    bytes_eq("array golden", b1, w1.pos,
             (uint8_t[]){0x01,0x02,0x11,0x01,0x11,0x02}, 6);

    /* nested: structure { unsigned 1, array[2]{ long-unsigned 10, 20 } } */
    axdr_data la[2];
    la[0] = (axdr_data){ .type = AXDR_LONG_U }; la[0].u.u16 = 10;
    la[1] = (axdr_data){ .type = AXDR_LONG_U }; la[1].u.u16 = 20;
    axdr_data inner = { .type = AXDR_ARRAY }; inner.u.list.items = la; inner.u.list.count = 2;
    axdr_data si[2];
    si[0] = (axdr_data){ .type = AXDR_UNSIGNED }; si[0].u.u8 = 1;
    si[1] = inner;
    axdr_data st = { .type = AXDR_STRUCTURE }; st.u.list.items = si; st.u.list.count = 2;

    uint8_t b2[32]; dlms_writer w2; dlms_writer_init(&w2, b2, sizeof b2);
    OK(axdr_encode_data(&w2, &st) == DLMS_OK, "nested encode");
    bytes_eq("nested golden", b2, w2.pos,
             (uint8_t[]){0x02,0x02, 0x11,0x01, 0x01,0x02, 0x12,0x00,0x0A, 0x12,0x00,0x14}, 12);

    /* decode the nested structure via streaming walk, compare traversal trace */
    char tr[128]; trace_buf tb = { tr, sizeof tr, 0 };
    dlms_reader r; dlms_reader_init(&r, b2, w2.pos);
    OK(axdr_walk(&r, trace_visit, &tb) == DLMS_OK && r.pos == w2.pos, "nested walk");
    OK(strcmp(tr, "0:2#2;1:17=1;1:1#2;2:18=10;2:18=20;") == 0, "nested trace");

    /* standalone structure{ unsigned 1, long-unsigned 0x0102 } */
    axdr_data pair[2];
    pair[0] = (axdr_data){ .type = AXDR_UNSIGNED }; pair[0].u.u8 = 1;
    pair[1] = (axdr_data){ .type = AXDR_LONG_U };   pair[1].u.u16 = 0x0102;
    axdr_data ps = { .type = AXDR_STRUCTURE }; ps.u.list.items = pair; ps.u.list.count = 2;
    uint8_t b3[16]; dlms_writer w3; dlms_writer_init(&w3, b3, sizeof b3);
    OK(axdr_encode_data(&w3, &ps) == DLMS_OK, "structure encode");
    bytes_eq("structure golden", b3, w3.pos,
             (uint8_t[]){0x02,0x02,0x11,0x01,0x12,0x01,0x02}, 7);
}

/* ===================================================== compact-array */
static void t_compact_array(void) {
    puts("A-XDR compact-array:");

    /* compact-array of unsigned { 1, 2, 3 } */
    axdr_data u3[3];
    for (int i = 0; i < 3; i++) { u3[i] = (axdr_data){ .type = AXDR_UNSIGNED }; u3[i].u.u8 = (uint8_t)(i+1); }
    axdr_data ca = { .type = AXDR_ARRAY }; ca.u.list.items = u3; ca.u.list.count = 3;
    uint8_t b[32]; dlms_writer w; dlms_writer_init(&w, b, sizeof b);
    OK(axdr_encode_compact_array(&w, &ca) == DLMS_OK, "compact scalar encode");
    bytes_eq("compact-array(unsigned) golden", b, w.pos,
             (uint8_t[]){0x13,0x11,0x03,0x01,0x02,0x03}, 6);
    {
        dlms_reader r; dlms_reader_init(&r, b, w.pos);
        char tr[96]; trace_buf tb = { tr, sizeof tr, 0 };
        OK(axdr_walk(&r, trace_visit, &tb) == DLMS_OK && r.pos == w.pos, "compact scalar walk");
        OK(strcmp(tr, "0:19*;1:17=1;1:17=2;1:17=3;") == 0, "compact scalar trace");
    }

    /* compact-array of structure{ unsigned, long-unsigned }, 2 elements */
    axdr_data s0[2], s1[2];
    s0[0] = (axdr_data){ .type = AXDR_UNSIGNED }; s0[0].u.u8 = 1;
    s0[1] = (axdr_data){ .type = AXDR_LONG_U };   s0[1].u.u16 = 0x0102;
    s1[0] = (axdr_data){ .type = AXDR_UNSIGNED }; s1[0].u.u8 = 2;
    s1[1] = (axdr_data){ .type = AXDR_LONG_U };   s1[1].u.u16 = 0x0304;
    axdr_data e0 = { .type = AXDR_STRUCTURE }; e0.u.list.items = s0; e0.u.list.count = 2;
    axdr_data e1 = { .type = AXDR_STRUCTURE }; e1.u.list.items = s1; e1.u.list.count = 2;
    axdr_data elems[2] = { e0, e1 };
    axdr_data cas = { .type = AXDR_ARRAY }; cas.u.list.items = elems; cas.u.list.count = 2;
    dlms_writer_init(&w, b, sizeof b);
    OK(axdr_encode_compact_array(&w, &cas) == DLMS_OK, "compact struct encode");
    bytes_eq("compact-array(struct) golden", b, w.pos,
             (uint8_t[]){0x13,0x02,0x02,0x11,0x12,0x06,
                         0x01,0x01,0x02, 0x02,0x03,0x04}, 12);
    {
        dlms_reader r; dlms_reader_init(&r, b, w.pos);
        char tr[128]; trace_buf tb = { tr, sizeof tr, 0 };
        OK(axdr_walk(&r, trace_visit, &tb) == DLMS_OK && r.pos == w.pos, "compact struct walk");
        OK(strcmp(tr, "0:19*;1:2#2;2:17=1;2:18=258;1:2#2;2:17=2;2:18=772;") == 0,
           "compact struct trace");
    }
}

/* ========================================================== BER structural */
static void t_ber(void) {
    puts("BER TLV:");
    /* high-tag-number round-trip (number >= 31) */
    uint8_t tb[8]; dlms_writer w; dlms_writer_init(&w, tb, sizeof tb);
    ber_tag t = { BER_CLASS_CONTEXT, true, 0x1234 };
    OK(ber_write_tag(&w, t) == DLMS_OK, "write high tag");
    dlms_reader r; dlms_reader_init(&r, tb, w.pos);
    ber_tag t2;
    OK(ber_read_tag(&r, &t2) == DLMS_OK, "read high tag");
    OK(t2.cls == t.cls && t2.constructed && t2.number == 0x1234, "high tag round-trip");

    /* constructed frame: [APPLICATION 0] { OCTET STRING "AB" } */
    dlms_writer_init(&w, tb, sizeof tb);
    ber_frame f;
    ber_tag app0 = { BER_CLASS_APPLICATION, true, 0 };
    OK(ber_begin(&w, app0, 1, &f) == DLMS_OK, "ber_begin");
    ber_tag os = { BER_CLASS_UNIVERSAL, false, 4 };
    OK(ber_write_primitive(&w, os, (const uint8_t*)"AB", 2) == DLMS_OK, "primitive");
    OK(ber_end(&w, &f) == DLMS_OK, "ber_end");
    bytes_eq("ber frame golden", tb, w.pos,
             (uint8_t[]){0x60,0x81,0x04,0x04,0x02,0x41,0x42}, 7);

    /* structural read-back */
    dlms_reader_init(&r, tb, w.pos);
    ber_tag rt; const uint8_t *val; size_t vlen; bool ind;
    OK(ber_read_tlv(&r, &rt, &val, &vlen, &ind) == DLMS_OK, "read top tlv");
    OK(rt.cls == BER_CLASS_APPLICATION && rt.constructed && rt.number == 0 && vlen == 4,
       "top tlv fields");
    dlms_reader cr; dlms_reader_init(&cr, val, vlen);
    OK(ber_read_tlv(&cr, &rt, &val, &vlen, &ind) == DLMS_OK, "read child tlv");
    OK(rt.number == 4 && vlen == 2 && val[0]=='A' && val[1]=='B', "child tlv fields");
}

/* ============================================================ xDLMS services */
static const cosem_attr_desc REG = { 3, {1,0,1,8,0,255}, 2 };

static void t_services(void) {
    puts("xDLMS services:");
    uint8_t iap = dlms_iap_make(1, true, false); /* 0x41 */
    uint8_t buf[64]; dlms_writer w; dlms_reader r;

    /* ---- get-request-normal (no selective access) ---- */
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_get_request_normal(&w, iap, &REG, false, 0) == DLMS_OK, "get-req enc");
    bytes_eq("get-req-normal golden", buf, w.pos,
        (uint8_t[]){0xC0,0x01,0x41,0x00,0x03,0x01,0x00,0x01,0x08,0x00,0xFF,0x02,0x00}, 13);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; OK(dlms_read_apdu_header(&r, &a, &v) == DLMS_OK
                         && a == APDU_GET_REQUEST && v == GET_REQUEST_NORMAL, "get-req hdr");
        uint8_t ri; cosem_attr_desc rd; bool hs; uint8_t sel;
        OK(dlms_read_get_request_normal(&r, &ri, &rd, &hs, &sel) == DLMS_OK && r.pos == w.pos,
           "get-req dec");
        OK(ri == iap && rd.class_id == 3 && rd.attribute_id == 2 && !hs
           && memcmp(rd.instance_id, REG.instance_id, 6) == 0, "get-req fields");
    }

    /* ---- get-request-normal WITH selective access + params Data(null) ---- */
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_get_request_normal(&w, iap, &REG, true, 1) == DLMS_OK, "get-req(sel) enc");
    { axdr_data nul = { .type = AXDR_NULL };
      OK(axdr_encode_data(&w, &nul) == DLMS_OK, "get-req(sel) params"); }
    bytes_eq("get-req(sel) golden", buf, w.pos,
        (uint8_t[]){0xC0,0x01,0x41,0x00,0x03,0x01,0x00,0x01,0x08,0x00,0xFF,0x02,0x01,0x01,0x00}, 15);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
        uint8_t ri; cosem_attr_desc rd; bool hs; uint8_t sel;
        OK(dlms_read_get_request_normal(&r, &ri, &rd, &hs, &sel) == DLMS_OK
           && hs && sel == 1, "get-req(sel) dec");
        axdr_header h; OK(axdr_decode_data(&r, &h) == DLMS_OK && h.type == AXDR_NULL
           && r.pos == w.pos, "get-req(sel) params dec");
    }

    /* ---- get-request-next ---- */
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_get_request_next(&w, iap, 1) == DLMS_OK, "get-next enc");
    bytes_eq("get-next golden", buf, w.pos,
        (uint8_t[]){0xC0,0x02,0x41,0x00,0x00,0x00,0x01}, 7);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
        uint8_t ri; uint32_t bn;
        OK(dlms_read_get_request_next(&r, &ri, &bn) == DLMS_OK && bn == 1, "get-next dec");
    }

    /* ---- get-response-normal (data = structure{u16,u32,enum}) ---- */
    axdr_data it[3];
    it[0] = (axdr_data){ .type = AXDR_LONG_U };        it[0].u.u16 = 1000;
    it[1] = (axdr_data){ .type = AXDR_DOUBLE_LONG_U }; it[1].u.u32 = 123456;
    it[2] = (axdr_data){ .type = AXDR_ENUM };          it[2].u.u8  = 30;
    axdr_data val = { .type = AXDR_STRUCTURE }; val.u.list.items = it; val.u.list.count = 3;
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_get_response_normal_data(&w, iap) == DLMS_OK, "get-resp enc");
    OK(axdr_encode_data(&w, &val) == DLMS_OK, "get-resp payload");
    bytes_eq("get-resp golden", buf, w.pos,
        (uint8_t[]){0xC4,0x01,0x41,0x00, 0x02,0x03, 0x12,0x03,0xE8,
                    0x06,0x00,0x01,0xE2,0x40, 0x16,0x1E}, 16);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
        uint8_t ri; bool isd; dlms_dar dar = DAR_SUCCESS;
        OK(dlms_read_get_response_normal(&r, &ri, &isd, &dar) == DLMS_OK && isd, "get-resp dec");
        char tr[96]; trace_buf tb = { tr, sizeof tr, 0 };
        OK(axdr_walk(&r, trace_visit, &tb) == DLMS_OK && r.pos == w.pos, "get-resp walk");
        OK(strcmp(tr, "0:2#3;1:18=1000;1:6=123456;1:22=30;") == 0, "get-resp trace");
    }

    /* ---- get-response-normal (result = object-unavailable) ---- */
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_get_response_normal_result(&w, iap, DAR_OBJECT_UNAVAILABLE) == DLMS_OK,
       "get-resp(err) enc");
    bytes_eq("get-resp(err) golden", buf, w.pos,
        (uint8_t[]){0xC4,0x01,0x41,0x01,0x0B}, 5);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
        uint8_t ri; bool isd; dlms_dar dar;
        OK(dlms_read_get_response_normal(&r, &ri, &isd, &dar) == DLMS_OK
           && !isd && dar == DAR_OBJECT_UNAVAILABLE, "get-resp(err) dec");
    }

    /* ---- get-response-with-datablock (raw, last, block 1, 3 bytes) ---- */
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_get_response_datablock(&w, iap, true, 1, true, 3, DAR_SUCCESS) == DLMS_OK,
       "get-resp-db enc");
    { uint8_t raw[3] = {0xAA,0xBB,0xCC}; OK(axdr_put_fixed(&w, raw, 3) == DLMS_OK, "db raw"); }
    bytes_eq("get-resp-db golden", buf, w.pos,
        (uint8_t[]){0xC4,0x02,0x41,0x01,0x00,0x00,0x00,0x01,0x00,0x03,0xAA,0xBB,0xCC}, 13);

    /* ---- set-request-normal + value Data(unsigned 5) ---- */
    cosem_attr_desc data_obj = { 1, {0,0,40,0,0,255}, 2 };
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_set_request_normal(&w, iap, &data_obj, false, 0) == DLMS_OK, "set-req enc");
    { axdr_data v5 = { .type = AXDR_UNSIGNED }; v5.u.u8 = 5;
      OK(axdr_encode_data(&w, &v5) == DLMS_OK, "set-req value"); }
    bytes_eq("set-req golden", buf, w.pos,
        (uint8_t[]){0xC1,0x01,0x41,0x00,0x01,0x00,0x00,0x28,0x00,0x00,0xFF,0x02,0x00,0x11,0x05}, 15);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
        uint8_t ri; cosem_attr_desc rd; bool hs; uint8_t sel;
        OK(dlms_read_set_request_normal(&r, &ri, &rd, &hs, &sel) == DLMS_OK && !hs, "set-req dec");
        axdr_header h; OK(axdr_decode_data(&r, &h) == DLMS_OK && h.type == AXDR_UNSIGNED
           && h.value.u.u8 == 5 && r.pos == w.pos, "set-req value dec");
    }

    /* ---- set-response-normal (success) ---- */
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_set_response_normal(&w, iap, DAR_SUCCESS) == DLMS_OK, "set-resp enc");
    bytes_eq("set-resp golden", buf, w.pos, (uint8_t[]){0xC5,0x01,0x41,0x00}, 4);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
        uint8_t ri; dlms_dar dar;
        OK(dlms_read_set_response_normal(&r, &ri, &dar) == DLMS_OK && dar == DAR_SUCCESS,
           "set-resp dec");
    }

    /* ---- action-request-normal (no params) ---- */
    cosem_method_desc mth = { 1, {0,0,40,0,0,255}, 1 };
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_action_request_normal(&w, iap, &mth, false) == DLMS_OK, "act-req enc");
    bytes_eq("act-req golden", buf, w.pos,
        (uint8_t[]){0xC3,0x01,0x41,0x00,0x01,0x00,0x00,0x28,0x00,0x00,0xFF,0x01,0x00}, 13);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
        uint8_t ri; cosem_method_desc rm; bool hp;
        OK(dlms_read_action_request_normal(&r, &ri, &rm, &hp) == DLMS_OK
           && !hp && rm.method_id == 1 && r.pos == w.pos, "act-req dec");
    }

    /* ---- action-response-normal (success, no return) ---- */
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_action_response_normal(&w, iap, ACTR_SUCCESS, false, false, DAR_SUCCESS)
       == DLMS_OK, "act-resp enc");
    bytes_eq("act-resp golden", buf, w.pos, (uint8_t[]){0xC7,0x01,0x41,0x00,0x00}, 5);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
        uint8_t ri; dlms_action_result res; bool hr, isd; dlms_dar dr;
        OK(dlms_read_action_response_normal(&r, &ri, &res, &hr, &isd, &dr) == DLMS_OK
           && res == ACTR_SUCCESS && !hr, "act-resp dec");
    }

    /* ---- data-notification (empty date-time, Data = unsigned 7) ---- */
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_data_notification(&w, 1, NULL, 0) == DLMS_OK, "dnot enc");
    { axdr_data v7 = { .type = AXDR_UNSIGNED }; v7.u.u8 = 7;
      OK(axdr_encode_data(&w, &v7) == DLMS_OK, "dnot payload"); }
    bytes_eq("dnot golden", buf, w.pos,
        (uint8_t[]){0x0F,0x00,0x00,0x00,0x01,0x00,0x11,0x07}, 8);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
        OK(a == APDU_DATA_NOTIFICATION, "dnot hdr no-variant");
        uint32_t liap; const uint8_t *dt; size_t dtl;
        OK(dlms_read_data_notification(&r, &liap, &dt, &dtl) == DLMS_OK
           && liap == 1 && dtl == 0, "dnot dec");
        axdr_header h; OK(axdr_decode_data(&r, &h) == DLMS_OK && h.type == AXDR_UNSIGNED
           && h.value.u.u8 == 7 && r.pos == w.pos, "dnot payload dec");
    }

    /* ---- get-request-with-list (1 entry, no sel) ---- */
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_get_request_with_list(&w, iap, 1) == DLMS_OK, "get-req-list enc");
    OK(dlms_put_attr_desc_with_selection(&w, &REG, false, 0) == DLMS_OK, "get-req-list entry");
    bytes_eq("get-req-with-list golden", buf, w.pos,
        (uint8_t[]){0xC0,0x03,0x41,0x01, 0x00,0x03,0x01,0x00,0x01,0x08,0x00,0xFF,0x02,0x00}, 14);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
        uint8_t ri; uint16_t cnt;
        OK(dlms_read_get_request_with_list(&r, &ri, &cnt) == DLMS_OK && cnt == 1, "get-req-list dec");
        cosem_attr_desc rd; bool hs; uint8_t sel;
        OK(dlms_read_attr_desc_with_selection(&r, &rd, &hs, &sel) == DLMS_OK
           && !hs && rd.class_id == 3 && r.pos == w.pos, "get-req-list entry dec");
    }

    /* ---- get-response-with-list (1 result: data=unsigned 5) ---- */
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_get_response_with_list(&w, iap, 1) == DLMS_OK, "get-resp-list enc");
    OK(dlms_put_get_data_result_data(&w) == DLMS_OK, "gdr data tag");
    { axdr_data v5 = { .type = AXDR_UNSIGNED }; v5.u.u8 = 5;
      OK(axdr_encode_data(&w, &v5) == DLMS_OK, "gdr data"); }
    bytes_eq("get-resp-with-list golden", buf, w.pos,
        (uint8_t[]){0xC4,0x03,0x41,0x01, 0x00, 0x11,0x05}, 7);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
        uint8_t ri; uint16_t cnt;
        OK(dlms_read_get_response_with_list(&r, &ri, &cnt) == DLMS_OK && cnt == 1, "get-resp-list dec");
        bool isd; dlms_dar dar;
        OK(dlms_read_get_data_result(&r, &isd, &dar) == DLMS_OK && isd, "gdr dec");
        axdr_header h; OK(axdr_decode_data(&r, &h) == DLMS_OK && h.value.u.u8 == 5
           && r.pos == w.pos, "gdr data dec");
    }

    /* ---- event-notification (no time, attr REG, data=unsigned 7) ---- */
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_event_notification(&w, false, NULL, 0, &REG) == DLMS_OK, "event enc");
    { axdr_data v7 = { .type = AXDR_UNSIGNED }; v7.u.u8 = 7;
      OK(axdr_encode_data(&w, &v7) == DLMS_OK, "event payload"); }
    bytes_eq("event-notification golden", buf, w.pos,
        (uint8_t[]){0xC2,0x00, 0x00,0x03,0x01,0x00,0x01,0x08,0x00,0xFF,0x02, 0x11,0x07}, 13);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
        OK(a == APDU_EVENT_NOTIFICATION && v == 0, "event hdr no-variant");
        bool ht; const uint8_t *tp; size_t tl; cosem_attr_desc rd;
        OK(dlms_read_event_notification(&r, &ht, &tp, &tl, &rd) == DLMS_OK
           && !ht && rd.class_id == 3, "event dec");
        axdr_header h; OK(axdr_decode_data(&r, &h) == DLMS_OK && h.value.u.u8 == 7
           && r.pos == w.pos, "event payload dec");
    }

    /* ---- exception-response (no invocation counter) ---- */
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_exception_response(&w, EXC_STATE_SERVICE_UNKNOWN, EXC_SVC_PDU_TOO_LONG, 0)
       == DLMS_OK, "exc enc");
    bytes_eq("exception-response golden", buf, w.pos, (uint8_t[]){0xD8,0x02,0x04}, 3);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
        OK(a == APDU_EXCEPTION_RESPONSE && v == 0, "exc hdr no-variant");
        dlms_exc_state st; dlms_exc_service sv; bool hic; uint32_t ic;
        OK(dlms_read_exception_response(&r, &st, &sv, &hic, &ic) == DLMS_OK
           && st == EXC_STATE_SERVICE_UNKNOWN && sv == EXC_SVC_PDU_TOO_LONG && !hic
           && r.pos == w.pos, "exc dec");
    }

    /* ---- exception-response with invocation-counter-error ---- */
    dlms_writer_init(&w, buf, sizeof buf);
    OK(dlms_enc_exception_response(&w, EXC_STATE_SERVICE_NOT_ALLOWED,
       EXC_SVC_INVOCATION_COUNTER_ERROR, 0x2A) == DLMS_OK, "exc-ic enc");
    bytes_eq("exception-response(ic) golden", buf, w.pos,
        (uint8_t[]){0xD8,0x01,0x06,0x00,0x00,0x00,0x2A}, 7);
    {
        dlms_reader_init(&r, buf, w.pos);
        uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
        dlms_exc_state st; dlms_exc_service sv; bool hic; uint32_t ic;
        OK(dlms_read_exception_response(&r, &st, &sv, &hic, &ic) == DLMS_OK
           && hic && ic == 0x2A && r.pos == w.pos, "exc-ic dec");
    }
}

/* ================================================================ error paths */
static void t_errors(void) {
    puts("error paths:");

    /* truncated get-request-normal (cut mid-descriptor) */
    uint8_t trunc[] = {0xC0,0x01,0x41,0x00,0x03,0x01,0x00};
    dlms_reader r; dlms_reader_init(&r, trunc, sizeof trunc);
    uint8_t a, v; dlms_read_apdu_header(&r, &a, &v);
    uint8_t ri; cosem_attr_desc rd; bool hs; uint8_t sel;
    OK(dlms_read_get_request_normal(&r, &ri, &rd, &hs, &sel) == DLMS_ERR_TRUNCATED,
       "truncated -> TRUNCATED");

    /* writer overflow: 4-byte buffer, encode a 5-byte double-long */
    uint8_t small[4]; dlms_writer w; dlms_writer_init(&w, small, sizeof small);
    axdr_data d = { .type = AXDR_DOUBLE_LONG }; d.u.i32 = 1;
    OK(axdr_encode_data(&w, &d) == DLMS_ERR_OVERFLOW, "overflow -> OVERFLOW");

    /* compact-array encode unsupported */
    uint8_t b[8]; dlms_writer_init(&w, b, sizeof b);
    axdr_data ca = { .type = AXDR_COMPACT_ARRAY };
    OK(axdr_encode_data(&w, &ca) == DLMS_ERR_UNSUPPORTED, "compact-array -> UNSUPPORTED");

    /* decode unknown Data tag (8 is a reserved gap) */
    uint8_t unk[] = {0x08, 0x00};
    dlms_reader_init(&r, unk, sizeof unk);
    axdr_header h;
    OK(axdr_decode_data(&r, &h) == DLMS_ERR_UNSUPPORTED, "unknown tag -> UNSUPPORTED");

    /* bad optional flag (0x02) in a get-request-normal */
    uint8_t badopt[] = {0x41,0x00,0x03,0x01,0x00,0x01,0x08,0x00,0xFF,0x02,0x02};
    dlms_reader_init(&r, badopt, sizeof badopt);
    OK(dlms_read_get_request_normal(&r, &ri, &rd, &hs, &sel) == DLMS_ERR_BAD_TAG,
       "bad optional -> BAD_TAG");
}

int main(void) {
    t_length();
    t_data_scalars();
    t_data_containers();
    t_compact_array();
    t_ber();
    t_services();
    t_errors();
    printf("\n==== %d passed, %d failed ====\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
