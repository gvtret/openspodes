/* demo_xdlms.c -- full GET round trip across both roles, streaming only.
 *
 * Client -> get-request-normal(class=3, OBIS 1.0.1.8.0.255, attr=2)
 * Server -> get-response-normal(data = structure{u16, double-long-unsigned, enum})
 * Both header framing and the Data payload move by streaming; nothing is
 * decoded into an intermediate tree.
 */
#include <stdio.h>
#include "xdlms.h"

static void hex(const char *label, const uint8_t *b, size_t n) {
    printf("%-16s", label);
    for (size_t i = 0; i < n; i++) printf("%02X ", b[i]);
    printf(" (%zu bytes)\n", n);
}

static const char *dar_name(dlms_dar d) {
    switch (d) {
        case DAR_SUCCESS: return "success";
        case DAR_OBJECT_UNAVAILABLE: return "object-unavailable";
        case DAR_READ_WRITE_DENIED: return "read-write-denied";
        default: return "other";
    }
}

/* visitor to print a decoded Data payload */
static dlms_status visit(const axdr_header *h, int depth, void *ctx) {
    (void)ctx;
    for (int i = 0; i < depth; i++) printf("    ");
    switch (h->type) {
        case AXDR_STRUCTURE: printf("structure[%zu]\n", h->count); break;
        case AXDR_ARRAY:     printf("array[%zu]\n", h->count); break;
        case AXDR_LONG_U:    printf("long-unsigned = %u\n", h->value.u.u16); break;
        case AXDR_DOUBLE_LONG_U: printf("double-long-unsigned = %u\n", h->value.u.u32); break;
        case AXDR_ENUM:      printf("enum = %u\n", h->value.u.u8); break;
        default:             printf("type %d\n", (int)h->type); break;
    }
    return DLMS_OK;
}

int main(void) {
    uint8_t iap = dlms_iap_make(1, /*confirmed*/true, /*high*/false);
    cosem_attr_desc reg = {
        .class_id = 3,                                   /* Register */
        .instance_id = {1,0,1,8,0,255},                  /* 1.0.1.8.0.255 */
        .attribute_id = 2                                /* value */
    };

    /* ---------------- CLIENT: build get-request-normal --------------- */
    uint8_t req[64];
    dlms_writer cw; dlms_writer_init(&cw, req, sizeof req);
    dlms_status s = dlms_enc_get_request_normal(&cw, iap, &reg, /*sel*/false, 0);
    if (s) { printf("req encode: %s\n", dlms_strerror(s)); return 1; }
    puts("=== CLIENT -> request ===");
    hex("get-request:", req, cw.pos);

    /* ---------------- SERVER: decode request ------------------------- */
    dlms_reader sr; dlms_reader_init(&sr, req, cw.pos);
    uint8_t apdu, variant;
    s = dlms_read_apdu_header(&sr, &apdu, &variant);
    if (s) { printf("hdr: %s\n", dlms_strerror(s)); return 1; }
    printf("\n=== SERVER <- request ===\napdu=0x%02X variant=%u\n", apdu, variant);
    if (apdu != APDU_GET_REQUEST || variant != GET_REQUEST_NORMAL) {
        puts("unexpected PDU"); return 1;
    }
    uint8_t r_iap; cosem_attr_desc r_desc; bool has_sel; uint8_t sel;
    s = dlms_read_get_request_normal(&sr, &r_iap, &r_desc, &has_sel, &sel);
    if (s) { printf("req decode: %s\n", dlms_strerror(s)); return 1; }
    printf("iid=%u confirmed=%d class=%u obis=%u.%u.%u.%u.%u.%u attr=%d sel=%d\n",
           dlms_iap_invoke_id(r_iap), dlms_iap_confirmed(r_iap), r_desc.class_id,
           r_desc.instance_id[0], r_desc.instance_id[1], r_desc.instance_id[2],
           r_desc.instance_id[3], r_desc.instance_id[4], r_desc.instance_id[5],
           r_desc.attribute_id, has_sel);

    /* ---------------- SERVER: build get-response-normal(data) -------- */
    /* value = structure { long-unsigned scaler_class?, dbl-long-uns value, enum unit } */
    axdr_data items[3];
    items[0] = (axdr_data){ .type = AXDR_LONG_U };          items[0].u.u16 = 1000;
    items[1] = (axdr_data){ .type = AXDR_DOUBLE_LONG_U };   items[1].u.u32 = 123456;
    items[2] = (axdr_data){ .type = AXDR_ENUM };            items[2].u.u8  = 30; /* Wh */
    axdr_data value = { .type = AXDR_STRUCTURE };
    value.u.list.items = items; value.u.list.count = 3;

    uint8_t resp[64];
    dlms_writer rw; dlms_writer_init(&rw, resp, sizeof resp);
    s = dlms_enc_get_response_normal_data(&rw, r_iap);           /* header */
    if (!s) s = axdr_encode_data(&rw, &value);                  /* streamed payload */
    if (s) { printf("resp encode: %s\n", dlms_strerror(s)); return 1; }
    puts("\n=== SERVER -> response ===");
    hex("get-response:", resp, rw.pos);

    /* ---------------- CLIENT: decode response ------------------------ */
    dlms_reader cr; dlms_reader_init(&cr, resp, rw.pos);
    s = dlms_read_apdu_header(&cr, &apdu, &variant);
    if (s || apdu != APDU_GET_RESPONSE || variant != GET_RESPONSE_NORMAL) {
        printf("resp hdr: %s\n", dlms_strerror(s)); return 1;
    }
    uint8_t c_iap; bool is_data; dlms_dar dar = DAR_SUCCESS;
    s = dlms_read_get_response_normal(&cr, &c_iap, &is_data, &dar);
    if (s) { printf("resp decode: %s\n", dlms_strerror(s)); return 1; }
    printf("\n=== CLIENT <- response ===\niid=%u ", dlms_iap_invoke_id(c_iap));
    if (!is_data) { printf("error = %s\n", dar_name(dar)); return 0; }
    puts("data:");
    s = axdr_walk(&cr, visit, NULL);   /* stream the Data payload */
    if (s) { printf("walk: %s\n", dlms_strerror(s)); return 1; }
    printf("consumed %zu/%zu bytes\n", cr.pos, rw.pos);
    return 0;
}
