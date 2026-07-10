/**
 * test_phase2.c — Phase 2: WithList, block transfer, notifications, GBT
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "openspodes.h"
#include "service/service.h"
#include "service/notification.h"
#include "service/gbt.h"
#include "server/server.h"
#include "server/dispatcher.h"
#include "ic/register.h"

static void test_get_with_list_golden(void **state) {
	(void)state;
	const uint8_t golden_req[] = {0xC0, 0x03, 0x41, 0x01, 0x00, 0x03, 0x01, 0x00, 0x01, 0x08, 0x00, 0xFF, 0x02, 0x00};
	const uint8_t golden_resp[] = {0xC4, 0x03, 0x41, 0x01, 0x00, 0x11, 0x05};

	osp_get_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = OSP_GET_WITH_LIST;
	req.invoke_id_priority = 0x41;
	req.as.with_list.count = 1;
	req.as.with_list.items[0].attr.class_id = 3;
	req.as.with_list.items[0].attr.instance_id = (osp_obis_t){1, 0, 1, 8, 0, 255};
	req.as.with_list.items[0].attr.attribute_id = 2;

	uint8_t mem[64];
	osp_buf_t w;
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_get_request_encode(&w, &req), 0);
	assert_int_equal(w.wr, sizeof(golden_req));
	assert_memory_equal(mem, golden_req, sizeof(golden_req));

	osp_buf_t r;
	osp_buf_init(&r, (uint8_t *)golden_req, sizeof(golden_req));
	r.wr = sizeof(golden_req);
	osp_get_request_t decoded;
	assert_int_equal(osp_get_request_decode(&r, &decoded), 0);
	assert_int_equal(decoded.type, OSP_GET_WITH_LIST);
	assert_int_equal(decoded.as.with_list.count, 1);

	osp_get_response_t resp;
	memset(&resp, 0, sizeof(resp));
	resp.type = OSP_GET_RESP_WITH_LIST;
	resp.invoke_id_priority = 0x41;
	resp.with_list.count = 1;
	resp.with_list.items[0].is_data = 1;
	resp.with_list.items[0].data = osp_val_u8(5);
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_get_response_encode(&w, &resp), 0);
	assert_int_equal(w.wr, sizeof(golden_resp));
	assert_memory_equal(mem, golden_resp, sizeof(golden_resp));

	osp_buf_init(&r, (uint8_t *)golden_resp, sizeof(golden_resp));
	r.wr = sizeof(golden_resp);
	assert_int_equal(osp_get_response_decode(&r, &resp), 0);
	assert_int_equal(resp.type, OSP_GET_RESP_WITH_LIST);
	assert_int_equal(osp_get_u8(&resp.with_list.items[0].data), 5);
}

static void test_get_datablock_golden(void **state) {
	(void)state;
	const uint8_t golden[] = {0xC4, 0x02, 0x41, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0xAA, 0xBB, 0xCC};
	osp_get_response_t resp;
	memset(&resp, 0, sizeof(resp));
	resp.type = OSP_GET_RESP_BLOCK_LAST;
	resp.invoke_id_priority = 0x41;
	resp.data_block.block_number = 1;
	resp.data_block.last_block = true;
	resp.data_block.raw_data_len = 3;
	resp.data_block.raw_data[0] = 0xAA;
	resp.data_block.raw_data[1] = 0xBB;
	resp.data_block.raw_data[2] = 0xCC;

	uint8_t mem[32];
	osp_buf_t w;
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_get_response_encode(&w, &resp), 0);
	assert_int_equal(w.wr, sizeof(golden));
	assert_memory_equal(mem, golden, sizeof(golden));

	osp_buf_t r;
	osp_buf_init(&r, (uint8_t *)golden, sizeof(golden));
	r.wr = sizeof(golden);
	osp_get_response_t decoded;
	assert_int_equal(osp_get_response_decode(&r, &decoded), 0);
	assert_int_equal(decoded.type, OSP_GET_RESP_BLOCK_LAST);
	assert_int_equal(decoded.data_block.raw_data_len, 3);
}

static void test_data_notification_roundtrip(void **state) {
	(void)state;
	osp_data_notification_t dn = {0};
	dn.long_invoke_id_and_priority = 1;
	dn.notification_body = osp_val_u8(7);

	uint8_t mem[32];
	osp_buf_t w;
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_data_notification_encode(&w, &dn), 0);
	const uint8_t golden[] = {0x0F, 0x00, 0x00, 0x00, 0x01, 0x00, 0x11, 0x07};
	assert_int_equal(w.wr, sizeof(golden));
	assert_memory_equal(mem, golden, sizeof(golden));

	osp_buf_t r;
	osp_buf_init(&r, mem, w.wr);
	r.wr = w.wr;
	osp_data_notification_t out;
	assert_int_equal(osp_data_notification_decode(&r, &out), 0);
	assert_int_equal(out.long_invoke_id_and_priority, 1);
	assert_int_equal(osp_get_u8(&out.notification_body), 7);
}

static void test_event_notification_roundtrip(void **state) {
	(void)state;
	osp_event_notification_t ev = {0};
	ev.attribute.class_id = 3;
	ev.attribute.instance_id = (osp_obis_t){1, 0, 1, 8, 0, 255};
	ev.attribute.attribute_id = 2;
	ev.value = osp_val_u8(7);

	uint8_t mem[32];
	osp_buf_t w;
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_event_notification_encode(&w, &ev), 0);
	const uint8_t golden[] = {0xC2, 0x00, 0x00, 0x03, 0x01, 0x00, 0x01, 0x08, 0x00, 0xFF, 0x02, 0x11, 0x07};
	assert_int_equal(w.wr, sizeof(golden));
	assert_memory_equal(mem, golden, sizeof(golden));
}

static void test_gbt_roundtrip(void **state) {
	(void)state;
	osp_general_block_transfer_t gbt = {0};
	gbt.window = 1;
	gbt.block_number = 1;
	gbt.block_data_len = 5;
	gbt.block_data[0] = 0xC0;
	gbt.block_data[1] = 0x01;
	gbt.block_data[2] = 0xC1;
	gbt.block_data[3] = 0x00;
	gbt.block_data[4] = 0x08;

	uint8_t mem[32];
	osp_buf_t w;
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_gbt_encode(&w, &gbt), 0);
	const uint8_t golden[] = {0xE0, 0x01, 0x00, 0x01, 0x00, 0x00, 0x05, 0xC0, 0x01, 0xC1, 0x00, 0x08};
	assert_int_equal(w.wr, sizeof(golden));
	assert_memory_equal(mem, golden, sizeof(golden));

	osp_buf_t r;
	osp_buf_init(&r, mem, w.wr);
	r.wr = w.wr;
	osp_general_block_transfer_t out;
	assert_int_equal(osp_gbt_decode(&r, &out), 0);
	assert_int_equal(out.block_number, 1);
	assert_int_equal(out.block_data_len, 5);
}

static void test_action_pblock_golden(void **state) {
	(void)state;
	const uint8_t golden_next_req[] = {0xC3, 0x02, 0xC1, 0x00, 0x00, 0x00, 0x02};
	const uint8_t golden_next_resp[] = {0xC7, 0x04, 0xC1, 0x00, 0x00, 0x00, 0x01};

	osp_action_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = OSP_ACTION_NEXT_PBLOCK;
	req.invoke_id_priority = 0xC1;
	req.as.next_pblock.block_number = 2;

	uint8_t mem[32];
	osp_buf_t w;
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_action_request_encode(&w, &req), 0);
	assert_int_equal(w.wr, sizeof(golden_next_req));
	assert_memory_equal(mem, golden_next_req, sizeof(golden_next_req));

	osp_action_response_t resp;
	memset(&resp, 0, sizeof(resp));
	resp.type = OSP_ACTION_RESP_NEXT_PBLOCK;
	resp.invoke_id_priority = 0xC1;
	resp.as.next_pblock.block_number = 1;
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_action_response_encode(&w, &resp), 0);
	assert_int_equal(w.wr, sizeof(golden_next_resp));
	assert_memory_equal(mem, golden_next_resp, sizeof(golden_next_resp));
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_get_with_list_golden),
	    cmocka_unit_test(test_get_datablock_golden),
	    cmocka_unit_test(test_data_notification_roundtrip),
	    cmocka_unit_test(test_event_notification_roundtrip),
	    cmocka_unit_test(test_gbt_roundtrip),
	    cmocka_unit_test(test_action_pblock_golden),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
