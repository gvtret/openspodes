/**
 * test_core.c — Smoke test for openspodes architecture
 *
 * Verifies: type system, codec round-trip, IC vtable, dispatcher routing.
 */

#include "openspodes.h"
#include "codec/codec.h"
#include "codec/types.h"
#include "codec/serialize.h"
#include "codec/structures.h"
#include "server/dispatcher.h"
#include "ic/data.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#define PASS(name) printf("  PASS: %s\n", name)
#define FAIL(name, msg) do { printf("  FAIL: %s — %s\n", name, msg); failures++; } while(0)

static int failures = 0;

static void test_types(void)
{
    /* Constructors */
    osp_value_t v;
    v = osp_val_null();
    assert(v.tag == OSP_TAG_NULL);

    v = osp_val_bool(true);
    assert(v.tag == OSP_TAG_BOOLEAN && v.as.boolean.value == true);

    v = osp_val_i8(-42);
    assert(v.tag == OSP_TAG_INTEGER && v.as.int8.value == -42);

    v = osp_val_u8(255);
    assert(v.tag == OSP_TAG_UNSIGNED && v.as.uint8.value == 255);

    v = osp_val_i16(-12345);
    assert(v.tag == OSP_TAG_LONG && v.as.int16.value == -12345);

    v = osp_val_u16(60000);
    assert(v.tag == OSP_TAG_LONG_UNSIGNED && v.as.uint16.value == 60000);

    v = osp_val_i32(-100000);
    assert(v.tag == OSP_TAG_DOUBLE_LONG && v.as.int32.value == -100000);

    v = osp_val_u32(0xDEADBEEF);
    assert(v.tag == OSP_TAG_DOUBLE_LONG_UNS && v.as.uint32.value == 0xDEADBEEF);

    v = osp_val_i64(1234567890123LL);
    assert(v.tag == OSP_TAG_LONG64 && v.as.int64.value == 1234567890123LL);

    v = osp_val_u64(0xDEADBEEFCAFEBABEULL);
    assert(v.tag == OSP_TAG_LONG64_UNSIGNED && v.as.uint64.value == 0xDEADBEEFCAFEBABEULL);

    v = osp_val_f32(3.14f);
    assert(v.tag == OSP_TAG_FLOAT32 && fabsf(v.as.float32.value - 3.14f) < 0.001f);

    v = osp_val_f64(2.718281828);
    assert(v.tag == OSP_TAG_FLOAT64 && fabs(v.as.float64.value - 2.718281828) < 1e-9);

    v = osp_val_enum(5);
    assert(v.tag == OSP_TAG_ENUM && v.as.enum_val.value == 5);

    v = osp_val_date(2026, 7, 9, 3);
    assert(v.tag == OSP_TAG_DATE && v.as.date.year == 2026 && v.as.date.month == 7 && v.as.date.day == 9);

    v = osp_val_time(14, 30, 45, 500);
    assert(v.tag == OSP_TAG_TIME && v.as.time.hour == 14 && v.as.time.ms == 0);

    v = osp_val_datetime(2026, 7, 9, 3, 14, 30, 45, 500);
    assert(v.tag == OSP_TAG_DATETIME && v.as.datetime.date.year == 2026);

    /* Extractors */
    v = osp_val_i32(-999);
    assert(osp_get_i32(&v) == -999);
    v = osp_val_u32(42);
    assert(osp_get_u32(&v) == 42);
    v = osp_val_bool(true);
    assert(osp_get_bool(&v) == true);
    v = osp_val_enum(7);
    assert(osp_get_enum(&v) == 7);

    /* Size table */
    assert(osp_axdr_type_size(OSP_TAG_BOOLEAN) == 1);
    assert(osp_axdr_type_size(OSP_TAG_INTEGER) == 1);
    assert(osp_axdr_type_size(OSP_TAG_LONG) == 2);
    assert(osp_axdr_type_size(OSP_TAG_DOUBLE_LONG) == 4);
    assert(osp_axdr_type_size(OSP_TAG_LONG64) == 8);
    assert(osp_axdr_type_size(OSP_TAG_DATE) == 5);
    assert(osp_axdr_type_size(OSP_TAG_TIME) == 4);
    assert(osp_axdr_type_size(OSP_TAG_DATETIME) == 12);
    assert(osp_axdr_type_size(OSP_TAG_OCTETSTRING) == 0); /* variable */

    PASS("types");
}

static void test_axdr_round_trip(void)
{
    uint8_t buf[128];
    osp_buf_t b;
    osp_buf_init(&b, buf, sizeof(buf));

    /* Write various types */
    osp_axdr_write_u8(&b, 0x42);
    osp_axdr_write_u16(&b, 0x1234);
    osp_axdr_write_u32(&b, 0xDEADBEEF);
    osp_axdr_write_bool(&b, true);

    /* Read back */
    osp_buf_t r;
    osp_buf_init(&r, buf, b.wr);
    r.wr = b.wr;

    uint8_t  v8;  uint16_t v16;  uint32_t v32;  bool vb;
    osp_axdr_read_u8(&r, &v8);   assert(v8 == 0x42);
    osp_axdr_read_u16(&r, &v16); assert(v16 == 0x1234);
    osp_axdr_read_u32(&r, &v32); assert(v32 == 0xDEADBEEF);
    osp_axdr_read_bool(&r, &vb); assert(vb == true);

    PASS("axdr_round_trip");
}

static void test_ic_data_class(void)
{
    const osp_ic_class_t *cls = osp_ic_data_class();
    assert(cls != NULL);
    assert(cls->class_id == 1);
    assert(cls->version == 0);
    assert(strcmp(cls->name, "Data") == 0);
    assert(cls->get_attr != NULL);
    assert(cls->set_attr != NULL);
    PASS("ic_data_class");
}

static void test_ic_data_getset(void)
{
    osp_ic_data_t data;
    osp_ic_data_init(&data, (osp_obis_t){0, 0, 0x80, 0, 0, 0xFF});

    /* Set uint32 value */
    osp_value_t set_val = osp_val_u32(42);
    const osp_ic_class_t *cls = osp_ic_data_class();
    assert(cls->set_attr(&data, 1, &set_val) == OSP_OK);

    /* Get it back */
    osp_value_t get_val = osp_val_null();
    assert(cls->get_attr(&data, 1, &get_val) == OSP_OK);
    assert(get_val.tag == OSP_TAG_DOUBLE_LONG_UNS);
    assert(get_val.as.uint32.value == 42);

    /* Set bool */
    set_val = osp_val_bool(true);
    assert(cls->set_attr(&data, 1, &set_val) == OSP_OK);
    assert(cls->get_attr(&data, 1, &get_val) == OSP_OK);
    assert(get_val.tag == OSP_TAG_BOOLEAN);
    assert(get_val.as.boolean.value == true);

    PASS("ic_data_getset");
}

static void test_dispatcher(void)
{
    osp_dispatcher_t disp;
    osp_dispatcher_init(&disp);

    osp_ic_data_t data1, data2;
    osp_ic_data_init(&data1, (osp_obis_t){0, 0, 1, 0, 0, 255});
    osp_ic_data_init(&data2, (osp_obis_t){0, 0, 8, 0, 0, 255});

    const osp_ic_class_t *cls = osp_ic_data_class();
    assert(osp_dispatcher_register(&disp, cls, &data1) == OSP_OK);
    assert(osp_dispatcher_register(&disp, cls, &data2) == OSP_OK);
    assert(disp.count == 2);

    /* Set value on first object */
    osp_value_t val = osp_val_u32(100);
    osp_obis_t ln1 = {0, 0, 1, 0, 0, 255};
    assert(osp_dispatcher_set(&disp, 1, &ln1, 1, &val) == OSP_OK);

    /* Get value from first object */
    osp_value_t result = osp_val_null();
    assert(osp_dispatcher_get(&disp, 1, &ln1, 1, &result) == OSP_OK);
    assert(result.tag == OSP_TAG_DOUBLE_LONG_UNS);
    assert(result.as.uint32.value == 100);

    /* Unknown object returns NOT_FOUND */
    osp_obis_t ln_unknown = {0, 0, 99, 0, 0, 255};
    assert(osp_dispatcher_get(&disp, 99, &ln_unknown, 1, &result) == OSP_ERR_NOT_FOUND);

    PASS("dispatcher");
}

static void test_obis_eq(void)
{
    osp_obis_t a = {0, 0, 1, 0, 0, 255};
    osp_obis_t b = {0, 0, 1, 0, 0, 255};
    osp_obis_t c = {0, 0, 8, 0, 0, 255};
    assert(osp_obis_eq(&a, &b) == true);
    assert(osp_obis_eq(&a, &c) == false);
    PASS("obis_eq");
}

static void test_serialize_value(void)
{
    /* Round-trip osp_value_t through generic read/write */
    uint8_t buf[256];
    osp_buf_t b;
    osp_buf_init(&b, buf, sizeof(buf));

    /* Write a bool */
    osp_value_t v = osp_val_bool(true);
    assert(osp_value_write(&b, &v) == OSP_OK);

    /* Write a u32 */
    v = osp_val_u32(0xDEADBEEF);
    assert(osp_value_write(&b, &v) == OSP_OK);

    /* Write a date */
    v = osp_val_date(2026, 7, 9, 3);
    assert(osp_value_write(&b, &v) == OSP_OK);

    /* Read back */
    osp_buf_t r;
    osp_buf_init(&r, buf, b.wr);
    r.wr = b.wr;

    assert(osp_value_read(&r, &v) == OSP_OK);
    assert(v.tag == OSP_TAG_BOOLEAN && v.as.boolean.value == true);

    assert(osp_value_read(&r, &v) == OSP_OK);
    assert(v.tag == OSP_TAG_DOUBLE_LONG_UNS && v.as.uint32.value == 0xDEADBEEF);

    assert(osp_value_read(&r, &v) == OSP_OK);
    assert(v.tag == OSP_TAG_DATE && v.as.date.year == 2026 && v.as.date.day == 9);

    /* Test date/time/datetime round-trip */
    osp_buf_init(&b, buf, sizeof(buf));
    osp_date_t d = {2026, 7, 9, 3};
    osp_date_write(&b, &d);
    osp_time_t t = {14, 30, 45, 0};
    osp_time_write(&b, &t);

    osp_buf_init(&r, buf, b.wr);
    r.wr = b.wr;
    osp_date_t d2; osp_time_t t2;
    assert(osp_date_read(&r, &d2) == OSP_OK);
    assert(osp_time_read(&r, &t2) == OSP_OK);
    assert(d2.year == 2026 && d2.day == 9);
    assert(t2.hour == 14 && t2.second == 45);

    /* Test structure begin/read */
    osp_buf_init(&b, buf, sizeof(buf));
    osp_struct_begin(&b, 3);
    osp_axdr_write_u8(&b, 0xAA);
    osp_axdr_write_u16(&b, 0x1234);
    osp_axdr_write_u8(&b, 0xBB);

    osp_buf_init(&r, buf, b.wr);
    r.wr = b.wr;
    uint8_t nf;
    assert(osp_struct_begin_read(&r, &nf) == OSP_OK);
    assert(nf == 3);
    uint8_t v8; uint16_t v16;
    osp_axdr_read_u8(&r, &v8); assert(v8 == 0xAA);
    osp_axdr_read_u16(&r, &v16); assert(v16 == 0x1234);
    osp_axdr_read_u8(&r, &v8); assert(v8 == 0xBB);

    /* Test array begin/read */
    osp_buf_init(&b, buf, sizeof(buf));
    osp_array_begin(&b, 5);
    for (int i = 0; i < 5; i++) osp_axdr_write_u8(&b, (uint8_t)i);

    osp_buf_init(&r, buf, b.wr);
    r.wr = b.wr;
    uint8_t ac;
    assert(osp_array_begin_read(&r, &ac) == OSP_OK);
    assert(ac == 5);
    for (int i = 0; i < 5; i++) {
        osp_axdr_read_u8(&r, &v8);
        assert(v8 == (uint8_t)i);
    }

    /* Test OBIS round-trip */
    osp_buf_init(&b, buf, sizeof(buf));
    osp_obis_t obis = {1, 2, 3, 4, 5, 6};
    assert(osp_obis_write(&b, &obis) == OSP_OK);

    osp_buf_init(&r, buf, b.wr);
    r.wr = b.wr;
    osp_obis_t obis2;
    assert(osp_obis_read(&r, &obis2) == OSP_OK);
    assert(osp_obis_eq(&obis, &obis2));

    /* Test scaler unit round-trip */
    osp_buf_init(&b, buf, sizeof(buf));
    osp_scaler_unit_t su = {-2, 30}; /* 10^-2 V */
    assert(osp_scaler_unit_write(&b, &su) == OSP_OK);

    osp_buf_init(&r, buf, b.wr);
    r.wr = b.wr;
    osp_scaler_unit_t su2;
    assert(osp_scaler_unit_read(&r, &su2) == OSP_OK);
    assert(su2.scaler == -2 && su2.unit == 30);

    PASS("serialize_value");
}

int main(void)
{
    printf("openspodes v%d.%d.%d — smoke tests\n",
           OPENSPODES_VERSION_MAJOR,
           OPENSPODES_VERSION_MINOR,
           OPENSPODES_VERSION_PATCH);
    printf("\n");

    test_types();
    test_axdr_round_trip();
    test_ic_data_class();
    test_ic_data_getset();
    test_dispatcher();
    test_obis_eq();
    test_serialize_value();

    printf("\n");
    if (failures == 0) {
        printf("All tests passed!\n");
    } else {
        printf("%d test(s) failed.\n", failures);
    }
    return failures;
}
