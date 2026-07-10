#include <stdio.h>
#include <string.h>
#include "../src/openspodes.h"
#include "../src/client/client.h"
#include "../src/server/server.h"
#include "../src/ic/data.h"
#include "../src/security/security.h"
#include "mock_transport.h"
#include "mock_crypto.h"

static osp_server_t *g_server = NULL;

static osp_err_t loopback_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	printf("  [client→server] %u bytes\n", len);
	osp_err_t r = mock_loopback_send(p, g_server, data, len);
	if (g_server) {
		printf("  [server_accept] result: %d, client_rx: %u bytes\n", r, p->client_rx.len);
	}
	return r;
}

static osp_err_t loopback_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	osp_err_t r = mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout);
	printf("  [server→client] result: %d, got %u bytes\n", r, out_len ? *out_len : 0);
	return r;
}

int main(void) {
	mock_crypto_init();
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);
	pair.client_transport.send = loopback_send;
	pair.client_transport.recv = loopback_recv;
	pair.client_transport.ctx = &pair;

	osp_server_t server;
	osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE);
	g_server = &server;
	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &sec);

	osp_client_t client;
	osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &csec);

	printf("=== connect ===\n");
	osp_err_t r = osp_client_connect(&client, 5000);
	printf("=== result: %d ===\n", r);
	return (r == 0) ? 0 : 1;
}
