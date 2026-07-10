#include "mock_transport.h"
#include "../src/server/server.h"
#include <string.h>

osp_err_t mock_loopback_send(mock_transport_pair_t *pair, osp_server_t *server, const uint8_t *data, uint32_t len) {
	if (!pair) {
		return OSP_ERR_INVALID;
	}

	osp_err_t r = mock_send_to_peer(&pair->server_rx, data, len);
	if (r != OSP_OK) {
		return r;
	}

	if (server) {
		r = osp_server_accept(server, 0);
		if (r != OSP_OK) {
			return r;
		}
	}

	return OSP_OK;
}

osp_err_t mock_send_to_peer(mock_buf_t *dst, const uint8_t *data, uint32_t len) {
	if (dst->len + len > MOCK_BUF_SIZE) {
		return OSP_ERR_NOMEM;
	}
	if (dst->msg_count < (sizeof(dst->msg_starts) / sizeof(dst->msg_starts[0]))) {
		dst->msg_starts[dst->msg_count++] = dst->len;
	}
	memcpy(&dst->data[dst->len], data, len);
	dst->len += len;
	return OSP_OK;
}

osp_err_t mock_recv_from_peer(mock_buf_t *src, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms) {
	(void)timeout_ms;
	if (src->msg_index >= src->msg_count) {
		return OSP_ERR_TIMEOUT;
	}
	uint32_t start = src->msg_starts[src->msg_index];
	uint32_t end = (src->msg_index + 1 < src->msg_count) ? src->msg_starts[src->msg_index + 1] : src->len;
	uint32_t avail = end - start;
	if (avail == 0) {
		return OSP_ERR_TIMEOUT;
	}
	uint32_t n = avail < size ? avail : size;
	memcpy(buf, &src->data[start], n);
	src->msg_index++;
	src->rpos = end;
	*out_len = n;
	return OSP_OK;
}

/* Client send → write to server_rx */
static osp_err_t client_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_send_to_peer(&p->server_rx, data, len);
}

/* Server recv ← read from server_rx (what client sent) */
static osp_err_t server_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_recv_from_peer(&p->server_rx, buf, size, out_len, timeout);
}

/* Server send → write to client_rx */
static osp_err_t server_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_send_to_peer(&p->client_rx, data, len);
}

/* Client recv ← read from client_rx (what server sent) */
static osp_err_t client_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout);
}

static bool mock_is_connected(void *ctx) {
	(void)ctx;
	return true;
}

static osp_err_t mock_open(void *ctx) {
	(void)ctx;
	return OSP_OK;
}

static void mock_close(void *ctx) {
	(void)ctx;
}

void mock_transport_pair_init(mock_transport_pair_t *p) {
	memset(p, 0, sizeof(*p));

	p->client_transport.open = mock_open;
	p->client_transport.send = client_send;
	p->client_transport.recv = client_recv;
	p->client_transport.close = mock_close;
	p->client_transport.is_connected = mock_is_connected;
	p->client_transport.ctx = p;

	p->server_transport.open = mock_open;
	p->server_transport.send = server_send;
	p->server_transport.recv = server_recv;
	p->server_transport.close = mock_close;
	p->server_transport.is_connected = mock_is_connected;
	p->server_transport.ctx = p;
}
