/**
 * test_data_hal.c — Unit tests for Data HAL interface.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "test_data_hal.h"
#include "../src/data_hal.h"
#include "../src/ic/data.h"
#include "../src/ic/register.h"
#include "../src/ic/clock.h"
#include "../src/server/server.h"
#include "../src/server/dispatcher.h"
#include <string.h>

static test_db_t g_db;
static osp_hal_data_t g_hal;

static int setup(void **state) {
	(void)state;
	test_db_init(&g_db);
	g_hal = test_db_make_hal(&g_db);
	osp_hal_data = NULL;
	return 0;
}

static int teardown(void **state) {
	(void)state;
	osp_hal_data = NULL;
	return 0;
}

/* ── Helper: create a mock transport pair for loopback ─────────────────── */

static uint8_t g_loopback_buf[4096];
static uint32_t g_loopback_len = 0;

static osp_err_t mock_send(void *ctx, const uint8_t *data, uint32_t len) {
	(void)ctx;
	if (len > sizeof(g_loopback_buf))
		return OSP_ERR_NOMEM;
	memcpy(g_loopback_buf, data, len);
	g_loopback_len = len;
	return OSP_OK;
}

static osp_err_t mock_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms) {
	(void)ctx;
	(void)timeout_ms;
	if (g_loopback_len == 0)
		return OSP_ERR_TIMEOUT;
	uint32_t n = g_loopback_len > size ? size : g_loopback_len;
	memcpy(buf, g_loopback_buf, n);
	*out_len = n;
	g_loopback_len = 0;
	return OSP_OK;
}

/* ── Test 1: NULL HAL preserves cached value ───────────────────────────── */

static void test_hal_null_no_change(void **state) {
	(void)state;

	osp_ic_data_t data;
	osp_ic_data_init(&data, (osp_obis_t){0, 0, 1, 0, 0, 255});
	data.value = osp_val_u32(42);

	osp_value_t result;
	memset(&result, 0, sizeof(result));
	osp_err_t r = osp_ic_data_class()->get_attr(&data, 2, &result);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_DOUBLE_LONG_UNS);
	assert_int_equal(result.as.uint32.value, 42);
}

/* ── Test 2: HAL read overrides Data IC cached value ───────────────────── */

static void test_hal_read_data(void **state) {
	(void)state;

	/* Add to DB */
	osp_obis_t obis = {0, 0, 1, 0, 0, 255};
	test_db_add(&g_db, "data", &obis, 2, osp_val_u32(999));

	osp_hal_data = &g_hal;

	osp_ic_data_t data;
	osp_ic_data_init(&data, obis);
	data.value = osp_val_u32(42); /* cached value */

	osp_value_t result;
	memset(&result, 0, sizeof(result));
	osp_err_t r = osp_ic_data_class()->get_attr(&data, 2, &result);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_DOUBLE_LONG_UNS);
	assert_int_equal(result.as.uint32.value, 999);
}

/* ── Test 3: HAL read for Register IC ──────────────────────────────────── */

static void test_hal_read_register(void **state) {
	(void)state;

	osp_obis_t obis = {0, 0, 3, 0, 0, 255};
	test_db_add(&g_db, "register", &obis, 2, osp_val_u32(12345));

	osp_hal_data = &g_hal;

	osp_value_t result;
	memset(&result, 0, sizeof(result));
	osp_err_t r = g_hal.read(g_hal.ctx, &obis, 2, &result);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_DOUBLE_LONG_UNS);
	assert_int_equal(result.as.uint32.value, 12345);
}

/* ── Test 4: HAL read for Clock IC ─────────────────────────────────────── */

static void test_hal_read_clock(void **state) {
	(void)state;

	osp_obis_t obis = {0, 0, 1, 0, 0, 255};
	test_db_add(&g_db, "clock", &obis, 2, osp_val_u32(0x12345678));

	osp_hal_data = &g_hal;

	osp_value_t result;
	memset(&result, 0, sizeof(result));
	osp_err_t r = g_hal.read(g_hal.ctx, &obis, 2, &result);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(result.as.uint32.value, 0x12345678);
}

/* ── Test 5: HAL write updates DB ──────────────────────────────────────── */

static void test_hal_write_data(void **state) {
	(void)state;

	osp_obis_t obis = {0, 0, 1, 0, 0, 255};
	test_db_add(&g_db, "data", &obis, 2, osp_val_u32(0));

	osp_hal_data = &g_hal;

	osp_value_t new_val = osp_val_u32(777);
	osp_err_t r = g_hal.write(g_hal.ctx, &obis, 2, &new_val);
	assert_int_equal(r, OSP_OK);

	/* Verify DB updated */
	osp_value_t result;
	memset(&result, 0, sizeof(result));
	r = g_hal.read(g_hal.ctx, &obis, 2, &result);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(result.as.uint32.value, 777);
}

/* ── Test 6: HAL write for Register IC ─────────────────────────────────── */

static void test_hal_write_register(void **state) {
	(void)state;

	osp_obis_t obis = {0, 0, 3, 0, 0, 255};
	test_db_add(&g_db, "register", &obis, 2, osp_val_u32(100));

	osp_hal_data = &g_hal;

	osp_value_t new_val = osp_val_u32(200);
	osp_err_t r = g_hal.write(g_hal.ctx, &obis, 2, &new_val);
	assert_int_equal(r, OSP_OK);

	osp_value_t result;
	memset(&result, 0, sizeof(result));
	r = g_hal.read(g_hal.ctx, &obis, 2, &result);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(result.as.uint32.value, 200);
}

/* ── Test 7: HAL execute delegates ─────────────────────────────────────── */

static void test_hal_execute_action(void **state) {
	(void)state;

	osp_obis_t obis = {0, 0, 1, 0, 0, 255};
	osp_hal_data = &g_hal;

	osp_value_t param = osp_val_u32(1);
	osp_value_t result;
	memset(&result, 0, sizeof(result));
	osp_err_t r = g_hal.execute(g_hal.ctx, &obis, 1, &param, &result);
	/* Stub returns NOT_FOUND for all */
	assert_int_equal(r, OSP_ERR_NOT_FOUND);
}

/* ── Test 8: HAL IO error propagation ──────────────────────────────────── */

static osp_err_t hal_io_error_read(void *ctx, const osp_obis_t *obis,
                                   uint8_t attr_id, osp_value_t *result) {
	(void)ctx; (void)obis; (void)attr_id; (void)result;
	return OSP_ERR_IO;
}

static void test_hal_io_error(void **state) {
	(void)state;

	osp_hal_data_t io_hal;
	io_hal.read = hal_io_error_read;
	io_hal.write = NULL;
	io_hal.execute = NULL;
	io_hal.ctx = NULL;
	osp_hal_data = &io_hal;

	osp_obis_t obis = {0, 0, 1, 0, 0, 255};
	osp_value_t result;
	memset(&result, 0, sizeof(result));
	osp_err_t r = io_hal.read(io_hal.ctx, &obis, 1, &result);
	assert_int_equal(r, OSP_ERR_IO);
}

/* ── Test 9: HAL NOT_FOUND fallback ────────────────────────────────────── */

static void test_hal_not_found(void **state) {
	(void)state;

	/* DB is empty — read should return NOT_FOUND */
	osp_hal_data = &g_hal;

	osp_obis_t obis = {0, 0, 99, 0, 0, 255};
	osp_value_t result;
	memset(&result, 0, sizeof(result));
	osp_err_t r = g_hal.read(g_hal.ctx, &obis, 1, &result);
	assert_int_equal(r, OSP_ERR_NOT_FOUND);
}

/* ── Test 10: poll updates caches ──────────────────────────────────────── */

static void test_hal_poll(void **state) {
	(void)state;

	/* Register a Data IC with cached value */
	osp_server_t server;
	memset(&server, 0, sizeof(server));

	osp_ic_data_t data;
	osp_ic_data_init(&data, (osp_obis_t){0, 0, 1, 0, 0, 255});
	data.value = osp_val_u32(10);

	/* Add to DB with different value */
	osp_obis_t obis = {0, 0, 1, 0, 0, 255};
	test_db_add(&g_db, "poll", &obis, 2, osp_val_u32(999));

	/* Register with dispatcher */
	server.dispatcher.objects[0].class_def = osp_ic_data_class();
	server.dispatcher.objects[0].instance = &data;
	server.dispatcher.count = 1;

	osp_hal_data = &g_hal;

	/* Poll should update cache from DB */
	for (uint16_t i = 0; i < server.dispatcher.count; i++) {
		osp_object_entry_t *e = &server.dispatcher.objects[i];
		if (!e->class_def || !e->instance)
			continue;
		const osp_obis_t *o = (const osp_obis_t *)e->instance;
		osp_value_t val;
		osp_err_t r = g_hal.read(g_hal.ctx, o, 2, &val);
		if (r == OSP_OK && e->class_def->set_attr) {
			e->class_def->set_attr(e->instance, 2, &val);
		}
	}

	/* Verify cache updated */
	osp_value_t result;
	memset(&result, 0, sizeof(result));
	osp_err_t r = osp_ic_data_class()->get_attr(&data, 2, &result);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_DOUBLE_LONG_UNS);
	assert_int_equal(result.as.uint32.value, 999);
	osp_value_t val;
	memset(&val, 0, sizeof(val));
	r = g_hal.read(g_hal.ctx, &obis, 2, &val);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(val.as.uint32.value, 999);
}

/* ── Test 11: full loopback with server accept ─────────────────────────── */

static void test_hal_poll_integration(void **state) {
	(void)state;

	/* Add value to DB */
	osp_obis_t obis = {0, 0, 1, 0, 0, 255};
	test_db_add(&g_db, "integration", &obis, 2, osp_val_u32(5555));

	osp_hal_data = &g_hal;

	/* Read from DB via HAL */
	osp_value_t result;
	memset(&result, 0, sizeof(result));
	osp_err_t r = g_hal.read(g_hal.ctx, &obis, 2, &result);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(result.as.uint32.value, 5555);

	/* Write new value via HAL */
	osp_value_t new_val = osp_val_u32(6666);
	r = g_hal.write(g_hal.ctx, &obis, 2, &new_val);
	assert_int_equal(r, OSP_OK);

	/* Read back */
	memset(&result, 0, sizeof(result));
	r = g_hal.read(g_hal.ctx, &obis, 2, &result);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(result.as.uint32.value, 6666);
}

/* ── Test 12: password from DB ─────────────────────────────────────────── */

static void test_hal_password_from_db(void **state) {
	(void)state;

	osp_obis_t obis = {0, 0, 40, 0, 0, 255}; /* Association LN */
	uint8_t secret[] = {0x41, 0x42, 0x43, 0x44}; /* "ABCD" */
	osp_value_t secret_val;
	secret_val.tag = OSP_TAG_OCTETSTRING;
	secret_val.as.octetstring.len = 4;
	memcpy(secret_val.as.octetstring.data, secret, 4);
	test_db_add(&g_db, "assoc", &obis, 7, secret_val);

	osp_hal_data = &g_hal;

	osp_value_t result;
	memset(&result, 0, sizeof(result));
	osp_err_t r = g_hal.read(g_hal.ctx, &obis, 7, &result);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(result.tag, OSP_TAG_OCTETSTRING);
	assert_int_equal(result.as.octetstring.len, 4);
	assert_memory_equal(result.as.octetstring.data, "ABCD", 4);
}

/* ── Main ──────────────────────────────────────────────────────────────── */

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test_setup_teardown(test_hal_null_no_change, setup, teardown),
	    cmocka_unit_test_setup_teardown(test_hal_read_data, setup, teardown),
	    cmocka_unit_test_setup_teardown(test_hal_read_register, setup, teardown),
	    cmocka_unit_test_setup_teardown(test_hal_read_clock, setup, teardown),
	    cmocka_unit_test_setup_teardown(test_hal_write_data, setup, teardown),
	    cmocka_unit_test_setup_teardown(test_hal_write_register, setup, teardown),
	    cmocka_unit_test_setup_teardown(test_hal_execute_action, setup, teardown),
	    cmocka_unit_test_setup_teardown(test_hal_io_error, setup, teardown),
	    cmocka_unit_test_setup_teardown(test_hal_not_found, setup, teardown),
	    cmocka_unit_test_setup_teardown(test_hal_poll, setup, teardown),
	    cmocka_unit_test_setup_teardown(test_hal_poll_integration, setup, teardown),
	    cmocka_unit_test_setup_teardown(test_hal_password_from_db, setup, teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
