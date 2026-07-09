/**
 * test_core.c — Smoke test for openspodes architecture
 *
 * Verifies: codec round-trip, IC vtable, dispatcher routing.
 */

#include "openspodes.h"
#include "codec/codec.h"
#include "server/dispatcher.h"
#include "ic/data.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define PASS(name) printf("  PASS: %s\n", name)
#define FAIL(name, msg) do { printf("  FAIL: %s — %s\n", name, msg); failures++; } while(0)

static int failures = 0;

static void test_buf_init(void)
{
    uint8_t mem[128];
    osp_buf_t buf;
    osp_buf_init(&buf, mem, sizeof(mem));

    assert(osp_buf_written(&buf) == 0);
    assert(osp_buf_unread(&buf) == 0);
    assert(osp_buf_free(&buf) == 128);
    PASS("buf_init");
}

static void test_axdr_round_trip(void)
{
    uint8_t wbuf[64], rbuf[64];
    osp_buf_t w, r;
    osp_buf_init(&w, wbuf, sizeof(wbuf));
    osp_buf_init(&r, rbuf, sizeof(rbuf));

    /* Write some AXDR data */
    osp_axdr_write_u8(&w, 0x42);
    osp_axdr_write_u16(&w, 0x1234);
    osp_axdr_write_u32(&w, 0xDEADBEEF);
    osp_axdr_write_bool(&w, true);

    /* Set up read buffer with same data */
    osp_buf_init(&r, wbuf, w.wr);
    r.wr = w.wr;

    uint8_t  v8;
    uint16_t v16;
    uint32_t v32;
    bool     vb;

    assert(osp_axdr_read_u8(&r, &v8) == OSP_OK);
    assert(v8 == 0x42);

    assert(osp_axdr_read_u16(&r, &v16) == OSP_OK);
    assert(v16 == 0x1234);

    assert(osp_axdr_read_u32(&r, &v32) == OSP_OK);
    assert(v32 == 0xDEADBEEF);

    assert(osp_axdr_read_bool(&r, &vb) == OSP_OK);
    assert(vb == true);

    PASS("axdr_round_trip");
}

static void test_ic_data_class(void)
{
    const osp_ic_class_t *cls = osp_ic_data_class();
    assert(cls != NULL);
    assert(cls->class_id == 1);
    assert(cls->version == 0);
    assert(strcmp(cls->name, "Data") == 0);
    assert(cls->attr_count == 1);
    assert(cls->get_attr != NULL);
    assert(cls->set_attr != NULL);
    PASS("ic_data_class");
}

static void test_ic_data_getset(void)
{
    osp_ic_data_t data;
    osp_obis_t ln = {0, 0, 0x80, 0, 0, 0xFF};
    osp_ic_data_init(&data, ln);

    /* Set a uint32 value */
    uint8_t set_buf[8];
    osp_buf_t sb;
    osp_buf_init(&sb, set_buf, sizeof(set_buf));
    osp_axdr_write_u8(&sb, OSP_TYPE_UINT32);
    osp_axdr_write_u32(&sb, 42);

    const osp_ic_class_t *cls = osp_ic_data_class();
    assert(cls->set_attr(&data, 1, &sb) == OSP_OK);

    /* Get the value back */
    uint8_t get_buf[64];
    osp_buf_t gb;
    osp_buf_init(&gb, get_buf, sizeof(get_buf));
    assert(cls->get_attr(&data, 1, &gb) == OSP_OK);
    assert(gb.wr == 5); /* tag(1) + uint32(4) */

    /* Verify */
    uint8_t rtag;
    uint32_t rval;
    gb.rd = 0;
    osp_axdr_read_u8(&gb, &rtag);
    osp_axdr_read_u32(&gb, &rval);
    assert(rtag == OSP_TYPE_UINT32);
    assert(rval == 42);

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
    uint8_t sbuf[8];
    osp_buf_t sb;
    osp_buf_init(&sb, sbuf, sizeof(sbuf));
    osp_axdr_write_u8(&sb, OSP_TYPE_UINT32);
    osp_axdr_write_u32(&sb, 100);

    osp_obis_t ln1 = {0, 0, 1, 0, 0, 255};
    assert(osp_dispatcher_set(&disp, 1, &ln1, 1, &sb) == OSP_OK);

    /* Get value from first object */
    uint8_t gbuf[64];
    osp_buf_t gb;
    osp_buf_init(&gb, gbuf, sizeof(gbuf));
    assert(osp_dispatcher_get(&disp, 1, &ln1, 1, &gb) == OSP_OK);

    uint32_t val = 0;
    gb.rd = 1; /* skip type tag */
    osp_axdr_read_u32(&gb, &val);
    assert(val == 100);

    /* Unknown object returns NOT_FOUND */
    osp_obis_t ln_unknown = {0, 0, 99, 0, 0, 255};
    assert(osp_dispatcher_get(&disp, 99, &ln_unknown, 1, &gb) == OSP_ERR_NOT_FOUND);

    PASS("dispatcher");
}

int main(void)
{
    printf("openspodes v%d.%d.%d — smoke tests\n",
           OPENSPODES_VERSION_MAJOR,
           OPENSPODES_VERSION_MINOR,
           OPENSPODES_VERSION_PATCH);
    printf("\n");

    test_buf_init();
    test_axdr_round_trip();
    test_ic_data_class();
    test_ic_data_getset();
    test_dispatcher();

    printf("\n");
    if (failures == 0) {
        printf("All tests passed!\n");
    } else {
        printf("%d test(s) failed.\n", failures);
    }
    return failures;
}
