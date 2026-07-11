/**
 * serial_transport.c — Serial (UART/RS-485) transport for OpenSPODES
 *
 * Linux serial port implementation of osp_transport_t.
 * Supports standard baud rates, 8N1 default, optional RS-485 RTS control.
 */

#include "serial_transport.h"

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  Low-level I/O
 * ═══════════════════════════════════════════════════════════════════════════ */

static osp_err_t read_exact_serial(int fd, uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
	uint32_t got = 0;
	while (got < len) {
		struct pollfd pfd = {.fd = fd, .events = POLLIN};
		int pr = poll(&pfd, 1, (int)timeout_ms);
		if (pr == 0)
			return OSP_ERR_TIMEOUT;
		if (pr < 0)
			return OSP_ERR_IO;
		ssize_t n = read(fd, &buf[got], len - got);
		if (n < 0)
			return OSP_ERR_IO;
		if (n == 0)
			return OSP_ERR_IO;
		got += (uint32_t)n;
	}
	return OSP_OK;
}

static osp_err_t write_all_serial(int fd, const uint8_t *data, uint32_t len) {
	uint32_t sent = 0;
	while (sent < len) {
		ssize_t n = write(fd, &data[sent], len - sent);
		if (n < 0)
			return OSP_ERR_IO;
		if (n == 0)
			return OSP_ERR_IO;
		sent += (uint32_t)n;
	}
	return OSP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Serial HAL callbacks
 * ═══════════════════════════════════════════════════════════════════════════ */

static osp_err_t serial_open(void *ctx) {
	(void)ctx;
	return OSP_OK;
}

static osp_err_t serial_send(void *ctx, const uint8_t *data, uint32_t len) {
	linux_serial_ctx_t *c = (linux_serial_ctx_t *)ctx;
	if (!c || c->fd < 0)
		return OSP_ERR_INVALID;

	/* RS-485: assert RTS before transmitting */
	if (c->rts_fd >= 0) {
		uint8_t on = 1;
		write(c->rts_fd, &on, 1);
	}

	osp_err_t r = write_all_serial(c->fd, data, len);

	/* RS-485: deassert RTS after transmit (wait for TX complete) */
	if (c->rts_fd >= 0) {
		tcdrain(c->fd);
		uint8_t off = 0;
		write(c->rts_fd, &off, 1);
	}

	return r;
}

static osp_err_t serial_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms) {
	linux_serial_ctx_t *c = (linux_serial_ctx_t *)ctx;
	if (!c || c->fd < 0)
		return OSP_ERR_INVALID;

	return read_exact_serial(c->fd, buf, size, timeout_ms ? timeout_ms : c->timeout_ms);
}

static void serial_close(void *ctx) {
	linux_serial_ctx_t *c = (linux_serial_ctx_t *)ctx;
	if (c) {
		if (c->fd >= 0) {
			close(c->fd);
			c->fd = -1;
		}
		if (c->rts_fd >= 0) {
			close(c->rts_fd);
			c->rts_fd = -1;
		}
	}
}

static bool serial_is_connected(void *ctx) {
	linux_serial_ctx_t *c = (linux_serial_ctx_t *)ctx;
	return c && c->fd >= 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Public API
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t linux_serial_transport_init(osp_transport_t *t, linux_serial_ctx_t *ctx,
                                       const char *port, speed_t baud) {
	if (!t || !ctx || !port)
		return OSP_ERR_INVALID;

	memset(ctx, 0, sizeof(*ctx));
	ctx->fd = -1;
	ctx->rts_fd = -1;
	ctx->timeout_ms = 1000;

	/* Open serial port */
	int fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0)
		return OSP_ERR_IO;

	/* Configure terminal */
	struct termios tty;
	memset(&tty, 0, sizeof(tty));
	if (tcgetattr(fd, &tty) != 0) {
		close(fd);
		return OSP_ERR_IO;
	}

	cfsetospeed(&tty, baud);
	cfsetispeed(&tty, baud);

	/* 8N1, no flow control, enable receiver */
	tty.c_cflag &= ~PARENB;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;
	tty.c_cflag |= CREAD | CLOCAL;
	tty.c_cflag &= ~CRTSCTS;

	/* Raw input */
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

	/* Raw output */
	tty.c_oflag &= ~OPOST;

	/* Non-blocking read with VMIN=0, VTIME=0 (poll-based timeout) */
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 0;

	tcflush(fd, TCIOFLUSH);
	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		close(fd);
		return OSP_ERR_IO;
	}

	ctx->fd = fd;

	/* Setup transport */
	t->open = serial_open;
	t->send = serial_send;
	t->recv = serial_recv;
	t->close = serial_close;
	t->is_connected = serial_is_connected;
	t->ctx = ctx;

	return OSP_OK;
}

osp_err_t linux_serial_set_rts(linux_serial_ctx_t *ctx, const char *rts_gpio) {
	if (!ctx || !rts_gpio)
		return OSP_ERR_INVALID;

	/* Export GPIO and set as output */
	char path[128];
	int len = snprintf(path, sizeof(path), "/sys/class/gpio/%s/direction", rts_gpio);

	/* Try to export first */
	char export_path[] = "/sys/class/gpio/export";
	int efd = open(export_path, O_WRONLY);
	if (efd >= 0) {
		write(efd, rts_gpio, strlen(rts_gpio));
		close(efd);
		usleep(100000); /* Wait for GPIO to appear */
	}

	/* Set direction */
	int dfd = open(path, O_WRONLY);
	if (dfd >= 0) {
		write(dfd, "out", 3);
		close(dfd);
	}

	/* Open GPIO value file */
	char value_path[128];
	snprintf(value_path, sizeof(value_path), "/sys/class/gpio/%s/value", rts_gpio);
	ctx->rts_fd = open(value_path, O_WRONLY);
	if (ctx->rts_fd < 0)
		return OSP_ERR_IO;

	return OSP_OK;
}
