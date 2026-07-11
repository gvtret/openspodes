/**
 * linux_hal.c — Linux HAL implementation for OpenSPODES
 *
 * Production-ready HAL using:
 *   - POSIX sockets for TCP transport
 *   - OpenSSL for AES-GCM / MD5 / SHA1 / SHA256
 *   - clock_gettime for timer
 *   - /dev/urandom for random
 */

#include "linux_hal.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#ifdef OSP_HAVE_OPENSSL_GCM
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 *  TCP transport
 * ═══════════════════════════════════════════════════════════════════════════ */

#define LINUX_HAL_MAX_PDU 4096

static osp_err_t tcp_open(void *ctx) {
	(void)ctx;
	return OSP_OK;
}

static osp_err_t read_exact(int fd, uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
	uint32_t got = 0;
	while (got < len) {
		struct pollfd pfd = {.fd = fd, .events = POLLIN};
		int pr = poll(&pfd, 1, (int)timeout_ms);
		if (pr == 0)
			return OSP_ERR_TIMEOUT;
		if (pr < 0)
			return (errno == EINTR) ? OSP_ERR_IO : OSP_ERR_IO;
		ssize_t n = read(fd, &buf[got], len - got);
		if (n < 0)
			return OSP_ERR_IO;
		if (n == 0)
			return OSP_ERR_IO;
		got += (uint32_t)n;
	}
	return OSP_OK;
}

static osp_err_t write_all(int fd, const uint8_t *data, uint32_t len) {
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

/* Read one HDLC frame from socket (flag-delimited) */
static osp_err_t hdlc_recv(int fd, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms) {
	/* Skip to opening flag */
	uint8_t byte;
	while (1) {
		osp_err_t r = read_exact(fd, &byte, 1, timeout_ms);
		if (r != OSP_OK)
			return r;
		if (byte == 0x7E)
			break;
	}

	buf[0] = 0x7E;
	uint32_t idx = 1;

	/* Read until closing flag */
	while (idx < size) {
		osp_err_t r = read_exact(fd, &byte, 1, timeout_ms);
		if (r != OSP_OK)
			return r;
		if (byte == 0x7E) {
			buf[idx++] = byte;
			*out_len = idx;
			return OSP_OK;
		}
		buf[idx++] = byte;
	}

	return OSP_ERR_NOMEM;
}

static osp_err_t tcp_send(void *ctx, const uint8_t *data, uint32_t len) {
	linux_hal_tcp_ctx_t *c = (linux_hal_tcp_ctx_t *)ctx;
	if (!c || c->fd < 0)
		return OSP_ERR_INVALID;

	switch (c->framing) {
		case OSP_FRAMING_WRAPPER: {
			uint8_t framed[LINUX_HAL_MAX_PDU + 8];
			uint32_t framed_len = 0;
			osp_err_t r = osp_wrapper_encode(c->wrapper_source, c->wrapper_dest, data, len, framed, sizeof(framed), &framed_len);
			if (r != OSP_OK)
				return r;
			return write_all(c->fd, framed, framed_len);
		}
		case OSP_FRAMING_HDLC:
			/* HDLC framing is handled by hdlc_session layer above */
			return write_all(c->fd, data, len);
		case OSP_FRAMING_NONE:
			return write_all(c->fd, data, len);
	}
	return OSP_ERR_UNSUPPORTED;
}

static osp_err_t tcp_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms) {
	linux_hal_tcp_ctx_t *c = (linux_hal_tcp_ctx_t *)ctx;
	if (!c || c->fd < 0)
		return OSP_ERR_INVALID;

	switch (c->framing) {
		case OSP_FRAMING_WRAPPER: {
			uint8_t header[8];
			osp_err_t r = read_exact(c->fd, header, sizeof(header), timeout_ms);
			if (r != OSP_OK)
				return r;
			uint16_t version = ((uint16_t)header[0] << 8) | header[1];
			uint32_t apdu_len = ((uint32_t)header[6] << 8) | header[7];
			if (version != 1 || apdu_len > size)
				return OSP_ERR_INVALID;
			r = read_exact(c->fd, buf, apdu_len, timeout_ms);
			if (r != OSP_OK)
				return r;
			*out_len = apdu_len;
			return OSP_OK;
		}
		case OSP_FRAMING_HDLC:
			return hdlc_recv(c->fd, buf, size, out_len, timeout_ms);
		case OSP_FRAMING_NONE:
			return read_exact(c->fd, buf, size, timeout_ms);
	}
	return OSP_ERR_UNSUPPORTED;
}

static void tcp_close(void *ctx) {
	linux_hal_tcp_ctx_t *c = (linux_hal_tcp_ctx_t *)ctx;
	if (c && c->fd >= 0) {
		close(c->fd);
		c->fd = -1;
	}
}

static bool tcp_is_connected(void *ctx) {
	linux_hal_tcp_ctx_t *c = (linux_hal_tcp_ctx_t *)ctx;
	return c && c->fd >= 0;
}

static int connect_tcp(const char *host, uint16_t port) {
	char port_str[8];
	snprintf(port_str, sizeof(port_str), "%u", port);

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo *res = NULL;
	if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res)
		return -1;

	int fd = -1;
	for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
		fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd < 0)
			continue;
		if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
			break;
		close(fd);
		fd = -1;
	}
	freeaddrinfo(res);
	return fd;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  AES-GCM crypto (OpenSSL)
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef OSP_HAVE_OPENSSL_GCM

static int linux_gcm_crypt(osp_gcm_dir_t dir, const uint8_t *key, uint32_t key_len,
                            const uint8_t iv[12], const uint8_t *aad, uint32_t aad_len,
                            const uint8_t *in, uint32_t in_len, uint8_t *out,
                            const uint8_t tag_in[16], uint8_t tag_out[16]) {
	const EVP_CIPHER *cipher = (key_len == 32) ? EVP_aes_256_gcm() : EVP_aes_128_gcm();
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (!ctx)
		return -1;

	int ok = -1;
	int enc = (dir == OSP_GCM_ENCRYPT) ? 1 : 0;
	int out_len = 0;

	if (EVP_CipherInit_ex(ctx, cipher, NULL, NULL, NULL, enc) != 1)
		goto done;
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) != 1)
		goto done;
	if (EVP_CipherInit_ex(ctx, cipher, NULL, key, iv, enc) != 1)
		goto done;
	if (aad && aad_len > 0) {
		if (EVP_CipherUpdate(ctx, NULL, &out_len, aad, (int)aad_len) != 1)
			goto done;
	}
	if (enc) {
		if (in && in_len > 0) {
			if (EVP_CipherUpdate(ctx, out, &out_len, in, (int)in_len) != 1)
				goto done;
		}
		int final_len = 0;
		if (EVP_CipherFinal_ex(ctx, out + out_len, &final_len) != 1)
			goto done;
		if (tag_out && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag_out) != 1)
			goto done;
	} else {
		if (tag_in && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void *)tag_in) != 1)
			goto done;
		if (in && in_len > 0) {
			if (EVP_CipherUpdate(ctx, out, &out_len, in, (int)in_len) != 1)
				goto done;
		}
		if (EVP_CipherFinal_ex(ctx, out + out_len, &out_len) != 1)
			goto done;
	}
	ok = 0;

done:
	EVP_CIPHER_CTX_free(ctx);
	return ok;
}

static void linux_md5(const uint8_t *input, uint32_t len, uint8_t output[16]) {
	unsigned int out_len = 16;
	EVP_Digest(input, len, output, &out_len, EVP_md5(), NULL);
}

static void linux_sha1(const uint8_t *input, uint32_t len, uint8_t output[20]) {
	unsigned int out_len = 20;
	EVP_Digest(input, len, output, &out_len, EVP_sha1(), NULL);
}

static void linux_sha256(const uint8_t *input, uint32_t len, uint8_t output[32]) {
	unsigned int out_len = 32;
	EVP_Digest(input, len, output, &out_len, EVP_sha256(), NULL);
}

void linux_hal_init_crypto(void) {
	osp_hal_gcm_crypt = linux_gcm_crypt;
	osp_hal_md5 = linux_md5;
	osp_hal_sha1 = linux_sha1;
	osp_hal_sha256 = linux_sha256;
}

#else /* No OpenSSL */

void linux_hal_init_crypto(void) {
	/* No crypto — only lowest/LLS security will work */
}

#endif

/* ═══════════════════════════════════════════════════════════════════════════
 *  Timer
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint32_t linux_now_ms(void *ctx) {
	(void)ctx;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void linux_delay_ms(void *ctx, uint32_t ms) {
	(void)ctx;
	struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};
	nanosleep(&ts, NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Random
 * ═══════════════════════════════════════════════════════════════════════════ */

static osp_err_t linux_random_fill(void *ctx, uint8_t *buf, uint32_t len) {
	(void)ctx;
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		return OSP_ERR_IO;

	uint32_t got = 0;
	while (got < len) {
		ssize_t n = read(fd, &buf[got], len - got);
		if (n <= 0) {
			close(fd);
			return OSP_ERR_IO;
		}
		got += (uint32_t)n;
	}
	close(fd);
	return OSP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  System title + key store
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint8_t *linux_get_key(void *ctx, uint8_t sap, uint8_t key_id) {
	linux_hal_system_ctx_t *sys = (linux_hal_system_ctx_t *)ctx;
	if (!sys)
		return NULL;

	for (uint8_t i = 0; i < sys->key_count; i++) {
		if (sys->keys[i].sap == sap && sys->keys[i].key_id == key_id) {
			return sys->keys[i].key;
		}
	}
	return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Public API
 * ═══════════════════════════════════════════════════════════════════════════ */

void linux_hal_init(osp_hal_t *hal) {
	memset(hal, 0, sizeof(*hal));
	linux_hal_t *lh = (linux_hal_t *)hal;

	/* Transport */
	hal->transport.open = tcp_open;
	hal->transport.send = tcp_send;
	hal->transport.recv = tcp_recv;
	hal->transport.close = tcp_close;
	hal->transport.is_connected = tcp_is_connected;
	hal->transport.ctx = &lh->tcp_ctx;
	lh->tcp_ctx.fd = -1;

	/* Timer */
	hal->timer.now_ms = linux_now_ms;
	hal->timer.delay_ms = linux_delay_ms;
	hal->timer.ctx = NULL;

	/* Random */
	hal->random.fill = linux_random_fill;
	hal->random.ctx = NULL;

	/* System */
	memcpy(lh->sys_ctx.system_title, (uint8_t[]){
		0x4C, 0x4F, 0x43, 0x41, 0x4C, 0x00, 0x00, 0x01}, 8);
	memcpy(hal->system.system_title, lh->sys_ctx.system_title, 8);
	hal->system.get_key = linux_get_key;
	hal->system.ctx = &lh->sys_ctx;

	/* Crypto */
	hal->crypto.ctx = NULL;
	linux_hal_init_crypto();
}

void linux_hal_set_tcp(osp_hal_t *hal, const char *host, uint16_t port) {
	linux_hal_t *lh = (linux_hal_t *)hal;
	linux_hal_tcp_ctx_t *ctx = &lh->tcp_ctx;
	if (ctx->fd >= 0)
		close(ctx->fd);
	ctx->fd = connect_tcp(host, port);
	/* Transport wraps/unwraps COSEM wrapper internally.
	   Client/server use FRAMING_NONE — HAL does the framing. */
	ctx->framing = OSP_FRAMING_WRAPPER;
	ctx->wrapper_source = 1000;
	ctx->wrapper_dest = 4059;
}

void linux_hal_set_tcp_hdlc(osp_hal_t *hal, const char *host, uint16_t port,
                             uint32_t client_addr, uint32_t server_addr) {
	linux_hal_t *lh = (linux_hal_t *)hal;
	linux_hal_tcp_ctx_t *ctx = &lh->tcp_ctx;
	if (ctx->fd >= 0)
		close(ctx->fd);
	ctx->fd = connect_tcp(host, port);
	ctx->framing = OSP_FRAMING_HDLC;
	(void)client_addr;
	(void)server_addr;
}

void linux_hal_set_system_title(osp_hal_t *hal, const uint8_t title[8]) {
	linux_hal_t *lh = (linux_hal_t *)hal;
	memcpy(lh->sys_ctx.system_title, title, 8);
	memcpy(hal->system.system_title, title, 8);
}

void linux_hal_add_key(osp_hal_t *hal, uint8_t sap, uint8_t key_id,
                        const uint8_t *key, uint8_t key_len) {
	linux_hal_t *lh = (linux_hal_t *)hal;
	linux_hal_system_ctx_t *sys = &lh->sys_ctx;
	if (sys->key_count >= OSP_LINUX_HAL_MAX_KEYS || key_len > OSP_LINUX_HAL_MAX_KEY_LEN)
		return;

	linux_hal_key_entry_t *e = &sys->keys[sys->key_count++];
	e->sap = sap;
	e->key_id = key_id;
	e->key_len = key_len;
	memcpy(e->key, key, key_len);
}
