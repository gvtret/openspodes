#include "mock_transport.h"
#include "../src/server/server.h"
#include "../src/service/gbt.h"
#include "../src/codec/codec.h"
#include <string.h>
#include <stdio.h>

/* ── Trace dump ──────────────────────────────────────────────────────── */

void mock_buf_trace_dump(const mock_buf_t *buf, const char *label) {
	if (!buf || !label) return;
	if (buf->msg_count == 0) {
		printf("  [%s] (no messages)\n", label);
		return;
	}
	for (uint32_t i = 0; i < buf->msg_count; i++) {
		uint32_t start = buf->msg_starts[i];
		uint32_t end = (i + 1 < buf->msg_count) ? buf->msg_starts[i + 1] : buf->len;
		uint32_t msg_len = end - start;
		if (msg_len == 0) continue;
		printf("  [%s] msg %u (%u bytes): ", label, i, msg_len);
		for (uint32_t j = 0; j < msg_len && j < 64; j++) {
			printf("%02X", buf->data[start + j]);
			if (j + 1 < msg_len && j + 1 < 64) printf("-");
		}
		if (msg_len > 64) printf("... (%u more)", msg_len - 64);
		printf("\n");
	}
}

void mock_transport_trace_dump(const mock_transport_pair_t *p) {
	if (!p) return;
	printf("=== Transport trace ===\n");
	mock_buf_trace_dump(&p->server_rx, "TX(client->server)");
	mock_buf_trace_dump(&p->client_rx, "RX(server->client)");
	printf("=== End trace ===\n");
}

osp_err_t mock_loopback_send(mock_transport_pair_t *pair, osp_server_t *server, const uint8_t *data, uint32_t len) {
	if (!pair) {
		return OSP_ERR_INVALID;
	}

	osp_err_t r = mock_send_to_peer(&pair->server_rx, data, len);
	if (r != OSP_OK) {
		return r;
	}

	/* GBT ack-only APDUs (empty block-data) must not re-enter the server dispatcher */
	if (len > 0 && data[0] == OSP_TAG_GENERAL_BLOCK_TRANSFER) {
		osp_general_block_transfer_t gbt;
		osp_buf_t buf;
		osp_buf_init(&buf, (uint8_t *)data, len);
		buf.wr = len;
		if (osp_gbt_decode(&buf, &gbt) == 0 && gbt.block_data_len == 0) {
			return OSP_OK;
		}
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
		/* If delay is set and no messages, sleep to simulate real timeout */
		if (src->delay_ms > 0) {
			/* Simple busy-wait for delay (not thread-safe, fine for tests) */
			uint32_t start = 0;
			for (volatile uint32_t i = 0; i < src->delay_ms * 1000; i++) {
				start = i; /* prevent optimization */
			}
			(void)start;
		}
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

/* Peek the latest queued GBT block in client_rx and post an ack to server_rx (does not consume client messages). */
static osp_err_t mock_gbt_ack_pump(mock_transport_pair_t *p) {
	if (!p->gbt_ack_pump || p->client_rx.msg_count == 0) {
		return OSP_ERR_TIMEOUT;
	}
	uint32_t last = p->client_rx.msg_count - 1;
	uint32_t start = p->client_rx.msg_starts[last];
	uint32_t end = (last + 1 < p->client_rx.msg_count) ? p->client_rx.msg_starts[last + 1] : p->client_rx.len;
	uint32_t msg_len = end - start;
	if (msg_len < 2 || p->client_rx.data[start] != OSP_TAG_GENERAL_BLOCK_TRANSFER) {
		return OSP_ERR_TIMEOUT;
	}

	osp_general_block_transfer_t gbt;
	osp_buf_t buf;
	osp_buf_init(&buf, &p->client_rx.data[start], msg_len);
	buf.wr = msg_len;
	if (osp_gbt_decode(&buf, &gbt) != 0 || gbt.window == 0 || gbt.last_block) {
		return OSP_ERR_TIMEOUT;
	}

	uint8_t tx[32];
	osp_buf_t w;
	osp_buf_init(&w, tx, sizeof(tx));
	osp_general_block_transfer_t ack = {0};
	ack.last_block = true;
	ack.window = gbt.window;
	ack.block_number = 1;
	ack.block_number_ack = gbt.block_number;
	if (osp_gbt_encode(&w, &ack) != 0) {
		return OSP_ERR_INVALID;
	}
	return mock_send_to_peer(&p->server_rx, tx, w.wr);
}

void mock_transport_enable_gbt_ack_pump(mock_transport_pair_t *p, bool enable) {
	if (p) {
		p->gbt_ack_pump = enable;
	}
}

/* ── Delay for timeout tests ──────────────────────────────────────────── */

void mock_buf_set_delay(mock_buf_t *buf, uint32_t delay_ms) {
	if (buf) {
		buf->delay_ms = delay_ms;
	}
}

void mock_transport_set_recv_delay(mock_transport_pair_t *p, bool server_rx_delay, uint32_t delay_ms) {
	if (!p) return;
	if (server_rx_delay) {
		mock_buf_set_delay(&p->server_rx, delay_ms);
	} else {
		mock_buf_set_delay(&p->client_rx, delay_ms);
	}
}

/* Client send → write to server_rx */
static osp_err_t client_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_send_to_peer(&p->server_rx, data, len);
}

/* Server recv ← read from server_rx (what client sent) */
static osp_err_t server_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	if (p->server_rx.msg_index >= p->server_rx.msg_count) {
		(void)mock_gbt_ack_pump(p);
	}
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
