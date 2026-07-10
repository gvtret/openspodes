/* demo.c -- round-trips a COSEM Data structure through A-XDR, builds a BER
 * ACSE frame, walks both, and shows encoding dispatch. */
#include <stdio.h>
#include <string.h>
#include "dlms_codec.h"

static void hex(const char *label, const uint8_t *b, size_t n) {
    printf("%-14s", label);
    for (size_t i = 0; i < n; i++) printf("%02X ", b[i]);
    printf(" (%zu bytes)\n", n);
}

/* ---- A-XDR demo: a typical GET-Response payload ----------------------- */
static const char *type_name(axdr_type t) {
    switch (t) {
        case AXDR_NULL: return "null"; case AXDR_ARRAY: return "array";
        case AXDR_STRUCTURE: return "structure"; case AXDR_BOOLEAN: return "boolean";
        case AXDR_OCTET_STRING: return "octet-string"; case AXDR_VISIBLE_STRING: return "visible-string";
        case AXDR_UTF8_STRING: return "utf8-string"; case AXDR_DOUBLE_LONG_U: return "double-long-unsigned";
        case AXDR_DOUBLE_LONG: return "double-long"; case AXDR_LONG_U: return "long-unsigned";
        case AXDR_LONG: return "long"; case AXDR_UNSIGNED: return "unsigned";
        case AXDR_INTEGER: return "integer"; case AXDR_ENUM: return "enum";
        case AXDR_FLOAT32: return "float32"; case AXDR_FLOAT64: return "float64";
        case AXDR_DATE_TIME: return "date-time"; case AXDR_LONG64_U: return "long64-unsigned";
        case AXDR_BIT_STRING: return "bit-string";
        default: return "?";
    }
}

static dlms_status visit(const axdr_header *h, int depth, void *ctx) {
    (void)ctx;
    for (int i = 0; i < depth; i++) printf("  ");
    printf("- %s", type_name(h->type));
    switch (h->type) {
        case AXDR_ARRAY: case AXDR_STRUCTURE:
            printf(" [%zu]", h->count); break;
        case AXDR_UNSIGNED: case AXDR_ENUM:
            printf(" = %u", h->value.u.u8); break;
        case AXDR_LONG_U:   printf(" = %u", h->value.u.u16); break;
        case AXDR_DOUBLE_LONG_U: printf(" = %u", h->value.u.u32); break;
        case AXDR_LONG64_U: printf(" = %llu", (unsigned long long)h->value.u.u64); break;
        case AXDR_INTEGER:  printf(" = %d", h->value.u.i8); break;
        case AXDR_FLOAT32:  printf(" = %g", (double)h->value.u.f32); break;
        case AXDR_BOOLEAN:  printf(" = %s", h->value.u.boolean ? "true" : "false"); break;
        case AXDR_OCTET_STRING: {
            printf(" = ");
            for (size_t i = 0; i < h->value.u.bytes.len; i++)
                printf("%02X", h->value.u.bytes.ptr[i]);
            break;
        }
        case AXDR_DATE_TIME: {
            printf(" = ");
            for (size_t i = 0; i < h->value.u.bytes.len; i++)
                printf("%02X", h->value.u.bytes.ptr[i]);
            break;
        }
        default: break;
    }
    printf("\n");
    return DLMS_OK;
}

static int axdr_demo(void) {
    /* structure {
     *   unsigned         2,
     *   octet-string     1.0.1.8.0.255   (OBIS),
     *   double-long-uns  123456,
     *   enum             30
     * } */
    static const uint8_t obis[6] = {1,0,1,8,0,255};
    axdr_data items[4];
    items[0] = (axdr_data){ .type = AXDR_UNSIGNED };        items[0].u.u8 = 2;
    items[1] = (axdr_data){ .type = AXDR_OCTET_STRING };    items[1].u.bytes.ptr = obis; items[1].u.bytes.len = 6;
    items[2] = (axdr_data){ .type = AXDR_DOUBLE_LONG_U };   items[2].u.u32 = 123456;
    items[3] = (axdr_data){ .type = AXDR_ENUM };            items[3].u.u8 = 30;

    axdr_data root = { .type = AXDR_STRUCTURE };
    root.u.list.items = items; root.u.list.count = 4;

    uint8_t buf[128];
    dlms_writer w; dlms_writer_init(&w, buf, sizeof buf);
    dlms_status s = axdr_encode_data(&w, &root);
    if (s) { printf("encode failed: %s\n", dlms_strerror(s)); return 1; }

    puts("=== A-XDR (COSEM Data) ===");
    hex("encoded:", buf, w.pos);

    dlms_reader r; dlms_reader_init(&r, buf, w.pos);
    puts("decoded tree:");
    s = axdr_walk(&r, visit, NULL);
    if (s) { printf("walk failed: %s\n", dlms_strerror(s)); return 1; }
    printf("consumed %zu/%zu bytes\n\n", r.pos, w.pos);
    return 0;
}

/* ---- BER demo: minimal AARQ skeleton ---------------------------------- */
static int ber_demo(void) {
    uint8_t buf[128];
    dlms_writer w; dlms_writer_init(&w, buf, sizeof buf);

    /* AARQ [APPLICATION 0] { application-context-name [1] OID(...) } (skeleton) */
    ber_tag aarq = { BER_CLASS_APPLICATION, true, 0 };  /* 0x60 */
    ber_frame f;
    dlms_status s = ber_begin(&w, aarq, 1, &f);          /* 1-octet length slot */
    if (s) goto fail;

    /* application-context-name [1] IMPLICIT -- carry a dummy 7-byte OID */
    static const uint8_t app_ctx[7] = {0x60,0x85,0x74,0x05,0x08,0x01,0x01};
    ber_tag ctx1 = { BER_CLASS_CONTEXT, true, 1 };       /* 0xA1 */
    ber_frame f1;
    s = ber_begin(&w, ctx1, 1, &f1); if (s) goto fail;
    ber_tag oid = { BER_CLASS_UNIVERSAL, false, 6 };     /* 0x06 OBJECT IDENTIFIER */
    s = ber_write_primitive(&w, oid, app_ctx, sizeof app_ctx); if (s) goto fail;
    s = ber_end(&w, &f1); if (s) goto fail;

    s = ber_end(&w, &f); if (s) goto fail;

    puts("=== BER (ACSE AARQ skeleton) ===");
    hex("encoded:", buf, w.pos);

    /* structural walk of the top-level TLV */
    dlms_reader r; dlms_reader_init(&r, buf, w.pos);
    ber_tag t; const uint8_t *val; size_t vlen; bool indef;
    s = ber_read_tlv(&r, &t, &val, &vlen, &indef); if (s) goto fail;
    printf("top TLV: class=0x%02X constructed=%d number=%u len=%zu\n",
           t.cls, t.constructed, t.number, vlen);

    dlms_reader cr; dlms_reader_init(&cr, val, vlen);
    while (dlms_remaining(&cr)) {
        s = ber_read_tlv(&cr, &t, &val, &vlen, &indef); if (s) goto fail;
        printf("  child : class=0x%02X constructed=%d number=%u len=%zu\n",
               t.cls, t.constructed, t.number, vlen);
    }
    printf("\n");
    return 0;
fail:
    printf("BER demo failed: %s\n", dlms_strerror(s));
    return 1;
}

/* ---- dispatch demo ---------------------------------------------------- */
static const char *enc_name(dlms_encoding e) {
    switch (e) {
        case DLMS_ENC_BER_ACSE:   return "BER (ACSE)";
        case DLMS_ENC_AXDR_XDLMS: return "A-XDR (xDLMS)";
        default:                  return "unknown";
    }
}
static void dispatch_demo(void) {
    puts("=== encoding dispatch by leading APDU tag ===");
    uint8_t tags[] = {0x60, 0x61, 0xC1, 0x0F, 0x2A};
    for (size_t i = 0; i < sizeof tags; i++)
        printf("  0x%02X -> %s\n", tags[i], enc_name(dlms_encoding_for_apdu(tags[i])));
    printf("\n");
}

int main(void) {
    int rc = 0;
    rc |= axdr_demo();
    rc |= ber_demo();
    dispatch_demo();
    return rc;
}
