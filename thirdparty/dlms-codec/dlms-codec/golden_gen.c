/* golden_gen.c -- emits the canonical golden-vector listing (labels + hex)
 * straight from the codec, so the reference file can never drift from the
 * bytes the code actually produces. Output is captured into golden_vectors.txt.
 */
#include <stdio.h>
#include <string.h>
#include "xdlms.h"

static void emit(const char *label, const uint8_t *b, size_t n) {
    printf("%s\n   ", label);
    for (size_t i = 0; i < n; i++) printf(" %02X", b[i]);
    printf("\n");
}
static void section(const char *s) { printf("\n# %s\n", s); }

/* encode one Data value into a fresh buffer and emit it */
static void emit_data(const char *label, const axdr_data *d) {
    uint8_t buf[64]; dlms_writer w; dlms_writer_init(&w, buf, sizeof buf);
    if (axdr_encode_data(&w, d) != DLMS_OK) { printf("%s\n    <encode error>\n", label); return; }
    emit(label, buf, w.pos);
}

int main(void) {
    const uint8_t iap = dlms_iap_make(1, true, false); /* 0x41 */
    const cosem_attr_desc REG = { 3, {1,0,1,8,0,255}, 2 };
    uint8_t buf[64]; dlms_writer w;

    printf("DLMS/COSEM codec -- golden vectors\n");
    printf("Generated from the codec; identical to the assertions in test.c.\n");
    printf("Format: <label> <hex bytes>. All multi-byte integers big-endian.\n");

    /* --------------------------------------------------------- length */
    section("A-XDR / BER length encoding (short & long form)");
    struct { const char *l; uint32_t v; } lens[] = {
        {"len 0",0},{"len 127",127},{"len 128",128},{"len 255",255},
        {"len 256",256},{"len 65535",65535},{"len 65536",65536},
        {"len 0x01020304",0x01020304}
    };
    for (size_t i = 0; i < sizeof lens/sizeof lens[0]; i++) {
        dlms_writer_init(&w, buf, sizeof buf);
        dlms_write_len(&w, lens[i].v);
        emit(lens[i].l, buf, w.pos);
    }
    emit("len indefinite (BER only)", (uint8_t[]){0x80}, 1);

    /* ---------------------------------------------------- Data scalars */
    section("A-XDR Data -- scalar types");
    static const uint8_t oct4[4] = {0xDE,0xAD,0xBE,0xEF};
    static const uint8_t ab[2]   = {'A','B'};
    static const uint8_t bits1[1]= {0xA0};
    static const uint8_t dt12[12]= {0x07,0xE6,0x01,0x0A,0x02,0x0C,0x1E,0x00,0xFF,0x88,0x80,0x00};
    axdr_data d;
    d=(axdr_data){.type=AXDR_NULL};                         emit_data("null-data",&d);
    d=(axdr_data){.type=AXDR_BOOLEAN}; d.u.boolean=true;    emit_data("boolean true",&d);
    d=(axdr_data){.type=AXDR_BOOLEAN}; d.u.boolean=false;   emit_data("boolean false",&d);
    d=(axdr_data){.type=AXDR_BIT_STRING}; d.u.bytes.ptr=bits1; d.u.bytes.len=4;
                                                            emit_data("bit-string (4 bits)",&d);
    d=(axdr_data){.type=AXDR_DOUBLE_LONG}; d.u.i32=-2;      emit_data("double-long -2",&d);
    d=(axdr_data){.type=AXDR_DOUBLE_LONG_U}; d.u.u32=123456;emit_data("double-long-unsigned 123456",&d);
    d=(axdr_data){.type=AXDR_OCTET_STRING}; d.u.bytes.ptr=oct4; d.u.bytes.len=4;
                                                            emit_data("octet-string DEADBEEF",&d);
    d=(axdr_data){.type=AXDR_VISIBLE_STRING}; d.u.bytes.ptr=ab; d.u.bytes.len=2;
                                                            emit_data("visible-string \"AB\"",&d);
    d=(axdr_data){.type=AXDR_UTF8_STRING}; d.u.bytes.ptr=ab; d.u.bytes.len=2;
                                                            emit_data("utf8-string \"AB\"",&d);
    d=(axdr_data){.type=AXDR_BCD}; d.u.i8=0x12;             emit_data("bcd 0x12",&d);
    d=(axdr_data){.type=AXDR_INTEGER}; d.u.i8=-1;           emit_data("integer -1",&d);
    d=(axdr_data){.type=AXDR_LONG}; d.u.i16=-2;             emit_data("long -2",&d);
    d=(axdr_data){.type=AXDR_UNSIGNED}; d.u.u8=200;         emit_data("unsigned 200",&d);
    d=(axdr_data){.type=AXDR_LONG_U}; d.u.u16=1000;         emit_data("long-unsigned 1000",&d);
    d=(axdr_data){.type=AXDR_LONG64}; d.u.i64=-2;           emit_data("long64 -2",&d);
    d=(axdr_data){.type=AXDR_LONG64_U}; d.u.u64=1;          emit_data("long64-unsigned 1",&d);
    d=(axdr_data){.type=AXDR_ENUM}; d.u.u8=30;              emit_data("enum 30",&d);
    d=(axdr_data){.type=AXDR_FLOAT32}; d.u.f32=1.0f;        emit_data("float32 1.0",&d);
    d=(axdr_data){.type=AXDR_FLOAT64}; d.u.f64=1.0;         emit_data("float64 1.0",&d);
    d=(axdr_data){.type=AXDR_DATE_TIME}; d.u.bytes.ptr=dt12; d.u.bytes.len=12;
                                                            emit_data("date-time (12 octets)",&d);

    /* ------------------------------------------------- Data containers */
    section("A-XDR Data -- containers");
    axdr_data ua[2];
    ua[0]=(axdr_data){.type=AXDR_UNSIGNED}; ua[0].u.u8=1;
    ua[1]=(axdr_data){.type=AXDR_UNSIGNED}; ua[1].u.u8=2;
    d=(axdr_data){.type=AXDR_ARRAY}; d.u.list.items=ua; d.u.list.count=2;
    emit_data("array[2]{ unsigned 1, unsigned 2 }",&d);

    axdr_data la[2];
    la[0]=(axdr_data){.type=AXDR_LONG_U}; la[0].u.u16=10;
    la[1]=(axdr_data){.type=AXDR_LONG_U}; la[1].u.u16=20;
    axdr_data inner={.type=AXDR_ARRAY}; inner.u.list.items=la; inner.u.list.count=2;
    axdr_data si[2];
    si[0]=(axdr_data){.type=AXDR_UNSIGNED}; si[0].u.u8=1; si[1]=inner;
    d=(axdr_data){.type=AXDR_STRUCTURE}; d.u.list.items=si; d.u.list.count=2;
    emit_data("structure{ unsigned 1, array[2]{ lu 10, lu 20 } }",&d);

    axdr_data pair[2];
    pair[0]=(axdr_data){.type=AXDR_UNSIGNED}; pair[0].u.u8=1;
    pair[1]=(axdr_data){.type=AXDR_LONG_U};   pair[1].u.u16=0x0102;
    d=(axdr_data){.type=AXDR_STRUCTURE}; d.u.list.items=pair; d.u.list.count=2;
    emit_data("structure{ unsigned 1, long-unsigned 0x0102 }",&d);

    /* ------------------------------------------------ compact-array */
    section("A-XDR Data -- compact-array");
    axdr_data u3[3];
    for (int i=0;i<3;i++){ u3[i]=(axdr_data){.type=AXDR_UNSIGNED}; u3[i].u.u8=(uint8_t)(i+1); }
    d=(axdr_data){.type=AXDR_ARRAY}; d.u.list.items=u3; d.u.list.count=3;
    { dlms_writer_init(&w,buf,sizeof buf); axdr_encode_compact_array(&w,&d);
      emit("compact-array of unsigned { 1, 2, 3 }", buf, w.pos); }

    axdr_data s0[2], s1[2];
    s0[0]=(axdr_data){.type=AXDR_UNSIGNED}; s0[0].u.u8=1;
    s0[1]=(axdr_data){.type=AXDR_LONG_U};   s0[1].u.u16=0x0102;
    s1[0]=(axdr_data){.type=AXDR_UNSIGNED}; s1[0].u.u8=2;
    s1[1]=(axdr_data){.type=AXDR_LONG_U};   s1[1].u.u16=0x0304;
    axdr_data e0={.type=AXDR_STRUCTURE}; e0.u.list.items=s0; e0.u.list.count=2;
    axdr_data e1={.type=AXDR_STRUCTURE}; e1.u.list.items=s1; e1.u.list.count=2;
    axdr_data ce[2]={e0,e1};
    d=(axdr_data){.type=AXDR_ARRAY}; d.u.list.items=ce; d.u.list.count=2;
    { dlms_writer_init(&w,buf,sizeof buf); axdr_encode_compact_array(&w,&d);
      emit("compact-array of structure{ u8, u16 } x2", buf, w.pos); }

    /* ------------------------------------------------------------- BER */
    section("BER TLV (ACSE style; ber_begin emits long-form length)");
    dlms_writer_init(&w, buf, sizeof buf);
    { ber_frame f; ber_tag app0={BER_CLASS_APPLICATION,true,0};
      ber_begin(&w,app0,1,&f);
      ber_tag os={BER_CLASS_UNIVERSAL,false,4};
      ber_write_primitive(&w,os,(const uint8_t*)"AB",2);
      ber_end(&w,&f); }
    emit("[APPLICATION 0]{ octet-string AB }", buf, w.pos);
    dlms_writer_init(&w, buf, sizeof buf);
    { ber_tag t={BER_CLASS_CONTEXT,true,0x1234}; ber_write_tag(&w,t); }
    emit("high-tag-number tag [CONTEXT 0x1234]", buf, w.pos);

    /* -------------------------------------------------- xDLMS services */
    section("xDLMS LN services");

    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_get_request_normal(&w, iap, &REG, false, 0);
    emit("get-request-normal (no sel)", buf, w.pos);

    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_get_request_normal(&w, iap, &REG, true, 1);
    { axdr_data n={.type=AXDR_NULL}; axdr_encode_data(&w,&n); }
    emit("get-request-normal (sel=1, params=null)", buf, w.pos);

    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_get_request_next(&w, iap, 1);
    emit("get-request-next (block 1)", buf, w.pos);

    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_get_response_normal_data(&w, iap);
    { axdr_data it[3];
      it[0]=(axdr_data){.type=AXDR_LONG_U}; it[0].u.u16=1000;
      it[1]=(axdr_data){.type=AXDR_DOUBLE_LONG_U}; it[1].u.u32=123456;
      it[2]=(axdr_data){.type=AXDR_ENUM}; it[2].u.u8=30;
      axdr_data v={.type=AXDR_STRUCTURE}; v.u.list.items=it; v.u.list.count=3;
      axdr_encode_data(&w,&v); }
    emit("get-response-normal (data=struct{lu,dlu,enum})", buf, w.pos);

    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_get_response_normal_result(&w, iap, DAR_OBJECT_UNAVAILABLE);
    emit("get-response-normal (result=object-unavail)", buf, w.pos);

    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_get_response_datablock(&w, iap, true, 1, true, 3, DAR_SUCCESS);
    { uint8_t raw[3]={0xAA,0xBB,0xCC}; axdr_put_fixed(&w,raw,3); }
    emit("get-response-with-datablock (raw, last)", buf, w.pos);

    const cosem_attr_desc DATA_OBJ = { 1, {0,0,40,0,0,255}, 2 };
    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_set_request_normal(&w, iap, &DATA_OBJ, false, 0);
    { axdr_data v5={.type=AXDR_UNSIGNED}; v5.u.u8=5; axdr_encode_data(&w,&v5); }
    emit("set-request-normal (value=unsigned 5)", buf, w.pos);

    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_set_response_normal(&w, iap, DAR_SUCCESS);
    emit("set-response-normal (success)", buf, w.pos);

    const cosem_method_desc MTH = { 1, {0,0,40,0,0,255}, 1 };
    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_action_request_normal(&w, iap, &MTH, false);
    emit("action-request-normal (no params)", buf, w.pos);

    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_action_response_normal(&w, iap, ACTR_SUCCESS, false, false, DAR_SUCCESS);
    emit("action-response-normal (success, no ret)", buf, w.pos);

    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_data_notification(&w, 1, NULL, 0);
    { axdr_data v7={.type=AXDR_UNSIGNED}; v7.u.u8=7; axdr_encode_data(&w,&v7); }
    emit("data-notification (dt empty, data=unsigned 7)", buf, w.pos);

    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_get_request_with_list(&w, iap, 1);
    dlms_put_attr_desc_with_selection(&w, &REG, false, 0);
    emit("get-request-with-list (1 entry, no sel)", buf, w.pos);

    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_get_response_with_list(&w, iap, 1);
    dlms_put_get_data_result_data(&w);
    { axdr_data v5={.type=AXDR_UNSIGNED}; v5.u.u8=5; axdr_encode_data(&w,&v5); }
    emit("get-response-with-list (1 result: data=unsigned 5)", buf, w.pos);

    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_event_notification(&w, false, NULL, 0, &REG);
    { axdr_data v7={.type=AXDR_UNSIGNED}; v7.u.u8=7; axdr_encode_data(&w,&v7); }
    emit("event-notification (no time, data=unsigned 7)", buf, w.pos);

    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_exception_response(&w, EXC_STATE_SERVICE_UNKNOWN, EXC_SVC_PDU_TOO_LONG, 0);
    emit("exception-response (service-unknown, pdu-too-long)", buf, w.pos);

    dlms_writer_init(&w, buf, sizeof buf);
    dlms_enc_exception_response(&w, EXC_STATE_SERVICE_NOT_ALLOWED,
                                EXC_SVC_INVOCATION_COUNTER_ERROR, 0x2A);
    emit("exception-response (invocation-counter-error=0x2A)", buf, w.pos);

    printf("\n# end\n");
    return 0;
}
