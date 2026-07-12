/**
 * test_phase2.c — Phase 2: WithList, block transfer, notifications, GBT
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <cmocka.h>

#include "openspodes.h"
#include "service/service.h"
#include "service/notification.h"
#include "service/gbt.h"
#include "mock_transport.h"
#include "server/server.h"
#include "server/dispatcher.h"
#include "ic/register.h"
#include "ic/compact_data.h"
#include "codec/serialize.h"

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

static void test_gbt_transport_mock_loopback(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	uint8_t apdu[256];
	for (uint32_t i = 0; i < 187; i++) {
		apdu[i] = (uint8_t)(i & 0xFF);
	}

	uint8_t tx_scratch[256];
	uint8_t rx_scratch[256];
	assert_int_equal(osp_gbt_transport_send(&pair.server_transport, OSP_FRAMING_NONE, apdu, 187, 40, 0, tx_scratch, sizeof(tx_scratch),
	                                          rx_scratch, sizeof(rx_scratch), 5000),
	                 OSP_OK);
	assert_true(pair.client_rx.msg_count >= 5);

	uint8_t out[256];
	uint8_t recv_scratch[256];
	uint8_t ack_tx[64];
	uint32_t out_len = 0;
	uint32_t first_len = 0;
	assert_int_equal(mock_recv_from_peer(&pair.client_rx, recv_scratch, sizeof(recv_scratch), &first_len, 0), OSP_OK);
	assert_int_equal(recv_scratch[0], OSP_TAG_GENERAL_BLOCK_TRANSFER);

	assert_int_equal(osp_gbt_transport_recv(&pair.client_transport, OSP_FRAMING_NONE, recv_scratch, sizeof(recv_scratch), out, sizeof(out),
	                                          &out_len, ack_tx, sizeof(ack_tx), 5000, recv_scratch, first_len),
	                 OSP_OK);
	assert_int_equal(out_len, 187);
	assert_memory_equal(out, apdu, 187);
}

static void test_gbt_streaming_transport_sets_str(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	const uint8_t apdu[] = {0xC4, 0x01, 0x41, 0x00, 0x11, 0x01, 0xC4, 0x01, 0x41, 0x00, 0x11, 0x02};
	uint8_t tx_scratch[64];
	uint8_t rx_scratch[64];
	assert_int_equal(osp_gbt_transport_send_streaming(&pair.server_transport, OSP_FRAMING_NONE, apdu, sizeof(apdu), 5, 0, tx_scratch,
	                                                   sizeof(tx_scratch), rx_scratch, sizeof(rx_scratch), 5000),
	                 OSP_OK);
	assert_true(pair.client_rx.msg_count >= 3);

	for (uint32_t i = 0; i < pair.client_rx.msg_count; i++) {
		uint32_t start = pair.client_rx.msg_starts[i];
		uint32_t end = (i + 1 < pair.client_rx.msg_count) ? pair.client_rx.msg_starts[i + 1] : pair.client_rx.len;
		osp_buf_t buf;
		osp_buf_init(&buf, &pair.client_rx.data[start], end - start);
		buf.wr = end - start;
		osp_general_block_transfer_t block;
		assert_int_equal(osp_gbt_decode(&buf, &block), 0);
		assert_true(block.streaming);
		assert_true(block.block_data_len > 0);
	}

	uint8_t out[sizeof(apdu)];
	uint8_t recv_scratch[64];
	uint8_t ack_tx[64];
	uint32_t out_len = 0;
	uint32_t first_len = 0;
	assert_int_equal(mock_recv_from_peer(&pair.client_rx, recv_scratch, sizeof(recv_scratch), &first_len, 0), OSP_OK);
	assert_int_equal(osp_gbt_transport_recv(&pair.client_transport, OSP_FRAMING_NONE, recv_scratch, sizeof(recv_scratch), out, sizeof(out),
	                                          &out_len, ack_tx, sizeof(ack_tx), 5000, recv_scratch, first_len),
	                 OSP_OK);
	assert_int_equal(out_len, sizeof(apdu));
	assert_memory_equal(out, apdu, sizeof(apdu));
}

typedef struct {
	osp_transport_t transport;
	const uint8_t *const *msgs;
	const uint32_t *lens;
	uint32_t count;
	uint32_t idx;
	uint8_t rx_buf[512];
} gbt_scripted_rx_t;

static osp_err_t gbt_scripted_open(void *ctx) {
	(void)ctx;
	return OSP_OK;
}

static void gbt_scripted_close(void *ctx) {
	(void)ctx;
}

static bool gbt_scripted_connected(void *ctx) {
	(void)ctx;
	return true;
}

static osp_err_t gbt_scripted_send(void *ctx, const uint8_t *data, uint32_t len) {
	(void)ctx;
	(void)data;
	(void)len;
	return OSP_OK;
}

static osp_err_t gbt_scripted_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms) {
	(void)timeout_ms;
	gbt_scripted_rx_t *s = (gbt_scripted_rx_t *)ctx;
	if (s->idx >= s->count) {
		return OSP_ERR_TIMEOUT;
	}
	uint32_t n = s->lens[s->idx];
	if (n > size) {
		return OSP_ERR_NOMEM;
	}
	memcpy(buf, s->msgs[s->idx], n);
	*out_len = n;
	s->idx++;
	return OSP_OK;
}

static void gbt_scripted_init(gbt_scripted_rx_t *s, const uint8_t *const *msgs, const uint32_t *lens, uint32_t count) {
	memset(s, 0, sizeof(*s));
	s->msgs = msgs;
	s->lens = lens;
	s->count = count;
	s->transport.open = gbt_scripted_open;
	s->transport.close = gbt_scripted_close;
	s->transport.send = gbt_scripted_send;
	s->transport.recv = gbt_scripted_recv;
	s->transport.is_connected = gbt_scripted_connected;
	s->transport.ctx = s;
}

static void test_gbt_send_rejects_payload_as_ack(void **state) {
	(void)state;
	uint8_t payload_ack[16];
	osp_general_block_transfer_t block = {0};
	block.last_block = true;
	block.block_number = 1;
	block.block_number_ack = 1;
	block.block_data_len = 1;
	block.block_data[0] = 0xC4;
	osp_buf_t w;
	osp_buf_init(&w, payload_ack, sizeof(payload_ack));
	assert_int_equal(osp_gbt_encode(&w, &block), 0);

	const uint8_t *msgs[] = {payload_ack};
	const uint32_t lens[] = {w.wr};
	gbt_scripted_rx_t scripted;
	gbt_scripted_init(&scripted, msgs, lens, 1);

	const uint8_t apdu[] = {0xC0, 0x01};
	uint8_t tx_scratch[32];
	assert_int_equal(osp_gbt_transport_send(&scripted.transport, OSP_FRAMING_NONE, apdu, sizeof(apdu), 1, 1, tx_scratch,
	                                          sizeof(tx_scratch), scripted.rx_buf, sizeof(scripted.rx_buf), 5000),
	                 OSP_ERR_INVALID);
}

static void test_gbt_recv_gap_recovery(void **state) {
	(void)state;
	uint8_t payload[120];
	for (uint32_t i = 0; i < sizeof(payload); i++) {
		payload[i] = (uint8_t)(i & 0xFF);
	}

	uint8_t b1[64], b2[64], b3[64], b2r[64];
	uint32_t b1_len = 0, b2_len = 0, b3_len = 0, b2r_len = 0;
	osp_general_block_transfer_t gbt = {0};
	gbt.window = 1;
	gbt.block_number = 1;
	gbt.block_data_len = 40;
	memcpy(gbt.block_data, payload, 40);
	osp_buf_t w;
	osp_buf_init(&w, b1, sizeof(b1));
	assert_int_equal(osp_gbt_encode(&w, &gbt), 0);
	b1_len = w.wr;

	gbt.block_number = 2;
	memcpy(gbt.block_data, &payload[40], 40);
	osp_buf_init(&w, b2, sizeof(b2));
	assert_int_equal(osp_gbt_encode(&w, &gbt), 0);
	b2_len = w.wr;

	gbt.block_number = 3;
	gbt.last_block = true;
	gbt.block_data_len = 40;
	memcpy(gbt.block_data, &payload[80], 40);
	osp_buf_init(&w, b3, sizeof(b3));
	assert_int_equal(osp_gbt_encode(&w, &gbt), 0);
	b3_len = w.wr;

	/* Retransmitted block 2 after gap nack */
	gbt.last_block = false;
	gbt.block_number = 2;
	gbt.block_data_len = 40;
	memcpy(gbt.block_data, &payload[40], 40);
	osp_buf_init(&w, b2r, sizeof(b2r));
	assert_int_equal(osp_gbt_encode(&w, &gbt), 0);
	b2r_len = w.wr;

	const uint8_t *msgs[] = {b3, b2r, b3};
	const uint32_t lens[] = {b3_len, b2r_len, b3_len};
	gbt_scripted_rx_t scripted;
	gbt_scripted_init(&scripted, msgs, lens, 3);

	uint8_t out[256];
	uint8_t ack_tx[64];
	uint32_t out_len = 0;
	assert_int_equal(osp_gbt_transport_recv(&scripted.transport, OSP_FRAMING_NONE, scripted.rx_buf, sizeof(scripted.rx_buf), out,
	                                          sizeof(out), &out_len, ack_tx, sizeof(ack_tx), 5000, b1, b1_len),
	                 OSP_OK);
	assert_int_equal(out_len, sizeof(payload));
	assert_memory_equal(out, payload, sizeof(payload));
}

static void test_confirmed_service_error_roundtrip(void **state) {
	(void)state;
	uint8_t mem[8];
	osp_buf_t w;
	osp_buf_init(&w, mem, sizeof(mem));

	osp_confirmed_service_error_t err = {OSP_CSE_SERVICE_INITIATE_ERROR, OSP_CSE_CATEGORY_INITIATE, 2};
	assert_int_equal(osp_confirmed_service_error_encode(&w, &err), 0);
	assert_int_equal(mem[0], OSP_TAG_CONFIRMED_SERVICE_ERROR);
	assert_int_equal(mem[1], 0x01);
	assert_int_equal(mem[2], 0x06);
	assert_int_equal(mem[3], 0x02);

	osp_buf_t r;
	osp_buf_init(&r, mem, w.wr);
	r.wr = w.wr;
	osp_confirmed_service_error_t decoded;
	assert_int_equal(osp_confirmed_service_error_decode(&r, &decoded), 0);
	assert_int_equal(decoded.service, err.service);
	assert_int_equal(decoded.category, err.category);
	assert_int_equal(decoded.value, err.value);
}

static void test_gbt_transport_confirmed(void **state) {
	(void)state;
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);
	mock_transport_enable_gbt_ack_pump(&pair, true);

	uint8_t apdu[256];
	for (uint32_t i = 0; i < 187; i++) {
		apdu[i] = (uint8_t)(i & 0xFF);
	}

	uint8_t send_tx[256];
	uint8_t send_rx[256];
	assert_int_equal(osp_gbt_transport_send(&pair.server_transport, OSP_FRAMING_NONE, apdu, 187, 40, 1, send_tx, sizeof(send_tx), send_rx,
	                                          sizeof(send_rx), 5000),
	                 OSP_OK);
	assert_true(pair.client_rx.msg_count >= 5);
	assert_true(pair.server_rx.msg_count >= 4);

	uint8_t out[256];
	uint8_t recv_scratch[256];
	uint8_t ack_tx[64];
	uint32_t out_len = 0;
	uint32_t first_len = 0;
	assert_int_equal(mock_recv_from_peer(&pair.client_rx, recv_scratch, sizeof(recv_scratch), &first_len, 0), OSP_OK);

	assert_int_equal(osp_gbt_transport_recv(&pair.client_transport, OSP_FRAMING_NONE, recv_scratch, sizeof(recv_scratch), out, sizeof(out),
	                                          &out_len, ack_tx, sizeof(ack_tx), 5000, recv_scratch, first_len),
	                 OSP_OK);
	assert_int_equal(out_len, 187);
	assert_memory_equal(out, apdu, 187);
}

static void test_action_param_block_golden(void **state) {
	(void)state;
	const uint8_t golden_next_req[] = {0xC3, 0x02, 0xC1, 0x00, 0x00, 0x00, 0x02};
	const uint8_t golden_next_resp[] = {0xC7, 0x04, 0xC1, 0x00, 0x00, 0x00, 0x01};

	osp_action_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = OSP_ACTION_NEXT_PARAM_BLOCK;
	req.invoke_id_priority = 0xC1;
	req.as.next_param_block.block_number = 2;

	uint8_t mem[32];
	osp_buf_t w;
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_action_request_encode(&w, &req), 0);
	assert_int_equal(w.wr, sizeof(golden_next_req));
	assert_memory_equal(mem, golden_next_req, sizeof(golden_next_req));

	osp_action_response_t resp;
	memset(&resp, 0, sizeof(resp));
	resp.type = OSP_ACTION_RESP_NEXT_PARAM_BLOCK;
	resp.invoke_id_priority = 0xC1;
	resp.as.next_param_block.block_number = 1;
	osp_buf_init(&w, mem, sizeof(mem));
	assert_int_equal(osp_action_response_encode(&w, &resp), 0);
	assert_int_equal(w.wr, sizeof(golden_next_resp));
	assert_memory_equal(mem, golden_next_resp, sizeof(golden_next_resp));
}

static void test_compact_data_capture(void **state) {
	(void)state;
	osp_ic_compact_data_t cd;
	osp_ic_compact_data_init(&cd, (osp_obis_t){0, 0, 99, 0, 0, 255});

	osp_value_t fields[2] = {osp_val_u8(1), osp_val_u16(0x0102)};
	osp_value_t row;
	row.tag = OSP_TAG_STRUCTURE;
	row.as.structure.elements.items = fields;
	row.as.structure.elements.count = 2;
	row.as.structure.elements.capacity = 2;
	osp_ic_compact_data_set_capture_values(&cd, &row, 1);

	const osp_ic_class_t *cls = osp_ic_compact_data_class();
	osp_value_t result = osp_val_null();
	assert_int_equal(cls->invoke(&cd, 2, NULL, &result), OSP_OK);

	osp_value_t buf_val;
	assert_int_equal(cls->get_attr(&cd, 2, &buf_val), OSP_OK);
	assert_int_equal(buf_val.tag, OSP_TAG_OCTETSTRING);

	const uint8_t expected[] = {0x13, 0x02, 0x02, 0x11, 0x12, 0x03, 0x01, 0x01, 0x02};
	assert_int_equal(buf_val.as.octetstring.len, sizeof(expected));
	assert_memory_equal(buf_val.as.octetstring.data, expected, sizeof(expected));

	osp_buf_t r;
	osp_buf_init(&r, buf_val.as.octetstring.data, buf_val.as.octetstring.len);
	r.wr = buf_val.as.octetstring.len;
	osp_value_t decoded;
	assert_int_equal(osp_value_read(&r, &decoded), OSP_OK);
	assert_int_equal(decoded.tag, OSP_TAG_ARRAY);
	assert_int_equal(decoded.as.array.elements.count, 1);
	assert_int_equal(decoded.as.array.elements.items[0].tag, OSP_TAG_STRUCTURE);
	assert_int_equal(decoded.as.array.elements.items[0].as.structure.elements.items[0].as.uint8.value, 1);
	assert_int_equal(decoded.as.array.elements.items[0].as.structure.elements.items[1].as.uint16.value, 0x0102);

	assert_int_equal(cls->invoke(&cd, 1, NULL, &result), OSP_OK);
	assert_int_equal(cls->get_attr(&cd, 2, &buf_val), OSP_OK);
	assert_int_equal(buf_val.as.octetstring.len, 0);
}

/* ── Bidirectional GBT streaming test ──────────────────────────────── */

static uint8_t g_bidir_received[64];
static uint32_t g_bidir_received_len = 0;

static void test_bidir_recv_cb(const uint8_t *data, uint32_t data_len, void *user_ctx) {
	(void)user_ctx;
	if (data_len <= sizeof(g_bidir_received)) {
		memcpy(g_bidir_received, data, data_len);
		g_bidir_received_len = data_len;
	}
}

static void test_gbt_bidir_streaming(void **state) {
	(void)state;
	/* Direct mock: server writes/reads from same buffer (client_rx).
	 * No loopback auto-process — avoids osp_server_accept interference. */
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);

	const uint8_t server_data[] = {0xC4, 0x01, 0x41, 0x00, 0x11, 0x01, 0x02, 0x03};
	uint8_t tx_scratch[128];
	uint8_t rx_scratch[128];

	/* Pre-populate client_rx with an ack that carries piggybacked data */
	osp_general_block_transfer_t client_ack = {0};
	client_ack.last_block = true;
	client_ack.streaming = false;
	client_ack.window = 1;
	client_ack.block_number = 1;
	client_ack.block_number_ack = 1;
	uint8_t client_response[] = {0xC4, 0x01, 0x42, 0x00, 0x11, 0x02};
	memcpy(client_ack.block_data, client_response, sizeof(client_response));
	client_ack.block_data_len = sizeof(client_response);

	osp_buf_t ack_buf;
	osp_buf_init(&ack_buf, tx_scratch, sizeof(tx_scratch));
	assert_int_equal(osp_gbt_encode(&ack_buf, &client_ack), 0);
	/* Server reads from server_rx — put ack there */
	assert_int_equal(mock_send_to_peer(&pair.server_rx, tx_scratch, ack_buf.wr), OSP_OK);

	g_bidir_received_len = 0;

	/* Server sends with block_payload_max=5 (2 blocks for 8 bytes), window=1.
	 * Server reads from client_rx (direct mock, no loopback). */
	osp_err_t r = osp_gbt_transport_send_streaming_bidir(&pair.server_transport, OSP_FRAMING_NONE, server_data, sizeof(server_data), 5, 1,
	                                            tx_scratch, sizeof(tx_scratch), rx_scratch, sizeof(rx_scratch), 5000,
	                                            test_bidir_recv_cb, NULL);
	assert_int_equal(r, OSP_OK);

	/* Server sent data blocks to client_rx */
	assert_true(pair.client_rx.msg_count >= 2);

	/* Callback was invoked with piggybacked data from ack */
	assert_true(g_bidir_received_len > 0);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_get_with_list_golden),
	    cmocka_unit_test(test_get_datablock_golden),
	    cmocka_unit_test(test_data_notification_roundtrip),
	    cmocka_unit_test(test_event_notification_roundtrip),
	    cmocka_unit_test(test_gbt_roundtrip),
	    cmocka_unit_test(test_gbt_transport_mock_loopback),
	    cmocka_unit_test(test_gbt_streaming_transport_sets_str),
	    cmocka_unit_test(test_gbt_send_rejects_payload_as_ack),
	    cmocka_unit_test(test_gbt_recv_gap_recovery),
	    cmocka_unit_test(test_confirmed_service_error_roundtrip),
	    cmocka_unit_test(test_gbt_transport_confirmed),
	    cmocka_unit_test(test_gbt_bidir_streaming),
	    cmocka_unit_test(test_action_param_block_golden),
	    cmocka_unit_test(test_compact_data_capture),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
