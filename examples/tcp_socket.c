#include "tcp_socket.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

static osp_err_t map_io_err(void) {
	if (errno == EAGAIN || errno == EWOULDBLOCK) {
		return OSP_ERR_TIMEOUT;
	}
	if (errno == EINTR) {
		return OSP_ERR_IO;
	}
	return OSP_ERR_IO;
}

static osp_err_t read_exact(int fd, uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
	uint32_t got = 0;
	while (got < len) {
		struct pollfd pfd = {.fd = fd, .events = POLLIN};
		int pr = poll(&pfd, 1, (int)timeout_ms);
		if (pr == 0) {
			return OSP_ERR_TIMEOUT;
		}
		if (pr < 0) {
			return map_io_err();
		}
		ssize_t n = read(fd, &buf[got], len - got);
		if (n < 0) {
			return map_io_err();
		}
		if (n == 0) {
			return OSP_ERR_IO;
		}
		got += (uint32_t)n;
	}
	return OSP_OK;
}

static osp_err_t write_all(int fd, const uint8_t *data, uint32_t len) {
	uint32_t sent = 0;
	while (sent < len) {
		ssize_t n = write(fd, &data[sent], len - sent);
		if (n < 0) {
			return map_io_err();
		}
		if (n == 0) {
			return OSP_ERR_IO;
		}
		sent += (uint32_t)n;
	}
	return OSP_OK;
}

static osp_err_t tcp_open(void *ctx) {
	(void)ctx;
	return OSP_OK;
}

static osp_err_t tcp_send(void *ctx, const uint8_t *data, uint32_t len) {
	osp_tcp_wrapper_ctx_t *c = (osp_tcp_wrapper_ctx_t *)ctx;
	if (!c || c->fd < 0 || !data) {
		return OSP_ERR_INVALID;
	}
	uint8_t framed[OSP_TCP_MAX_PDU + OSP_WRAPPER_HEADER_SIZE];
	uint32_t framed_len = 0;
	osp_err_t r = osp_wrapper_encode(c->wrapper_source, c->wrapper_dest, data, len, framed, sizeof(framed), &framed_len);
	if (r != OSP_OK) {
		return r;
	}
	return write_all(c->fd, framed, framed_len);
}

static osp_err_t tcp_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms) {
	osp_tcp_wrapper_ctx_t *c = (osp_tcp_wrapper_ctx_t *)ctx;
	if (!c || c->fd < 0 || !buf || !out_len) {
		return OSP_ERR_INVALID;
	}

	uint8_t header[OSP_WRAPPER_HEADER_SIZE];
	osp_err_t r = read_exact(c->fd, header, sizeof(header), timeout_ms);
	if (r != OSP_OK) {
		return r;
	}

	uint16_t version = ((uint16_t)header[0] << 8) | header[1];
	uint32_t apdu_len = ((uint32_t)header[6] << 8) | header[7];
	if (version != OSP_WRAPPER_VERSION || apdu_len > OSP_TCP_MAX_PDU) {
		return OSP_ERR_INVALID;
	}
	if (apdu_len > size) {
		return OSP_ERR_NOMEM;
	}
	r = read_exact(c->fd, buf, apdu_len, timeout_ms);
	if (r != OSP_OK) {
		return r;
	}
	*out_len = apdu_len;
	return OSP_OK;
}

static void tcp_close(void *ctx) {
	osp_tcp_wrapper_ctx_t *c = (osp_tcp_wrapper_ctx_t *)ctx;
	if (c && c->fd >= 0) {
		close(c->fd);
		c->fd = -1;
	}
}

static bool tcp_is_connected(void *ctx) {
	osp_tcp_wrapper_ctx_t *c = (osp_tcp_wrapper_ctx_t *)ctx;
	return c && c->fd >= 0;
}

void osp_tcp_wrapper_transport_init(osp_transport_t *t, osp_tcp_wrapper_ctx_t *ctx, int fd, uint16_t wrapper_source,
                                    uint16_t wrapper_dest) {
	if (!t || !ctx) {
		return;
	}
	ctx->fd = fd;
	ctx->wrapper_source = wrapper_source;
	ctx->wrapper_dest = wrapper_dest;
	t->open = tcp_open;
	t->send = tcp_send;
	t->recv = tcp_recv;
	t->close = tcp_close;
	t->is_connected = tcp_is_connected;
	t->ctx = ctx;
}
