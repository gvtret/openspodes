/**
 * yb_helpers.c — Shared helpers for Yellow Book conformance tests.
 */

#include "yb_helpers.h"
#include <string.h>

/* Global server reference for loopback transport */
static osp_server_t *g_yb_server = NULL;

static osp_err_t yb_loopback_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_loopback_send(p, g_yb_server, data, len);
}

static osp_err_t yb_loopback_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout);
}

void yb_setup_loopback(mock_transport_pair_t *pair, osp_server_t *server) {
	mock_transport_pair_init(pair);
	pair->client_transport.send = yb_loopback_send;
	pair->client_transport.recv = yb_loopback_recv;
	pair->client_transport.ctx = pair;
	g_yb_server = server;
}

void yb_setup_server(osp_server_t *server, mock_transport_pair_t *pair, uint32_t initial_value) {
	osp_server_init(server, &pair->server_transport, OSP_FRAMING_NONE);
	static osp_ic_data_t data_obj;
	osp_ic_data_init(&data_obj, YB_TEST_OBIS);
	data_obj.value = osp_val_u32(initial_value);
	osp_server_register(server, osp_ic_data_class(), &data_obj);
	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(server, &sec);
}

void yb_make_pair(mock_transport_pair_t *pair, osp_server_t *server, osp_client_t *client, uint32_t initial_value) {
	yb_setup_server(server, pair, initial_value);
	yb_setup_loopback(pair, server);
	osp_client_init(client, &pair->client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(client, &csec);
}
