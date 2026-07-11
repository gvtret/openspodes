/**
 * serial_server.c — DLMS/COSEM Serial server with HDLC framing
 *
 * Usage: openspodes_serial_server /dev/ttyUSB0 [baud]
 * Default: 9600
 *
 * Full server flow:
 *   1. Open serial port
 *   2. Wait for client HDLC SNRM → respond UA
 *   3. Accept AARQ → send AARE
 *   4. Handle GET/SET requests
 *   5. Wait for RLRQ → send RLRE
 *   6. Wait for DISC → send UA
 */

#include "../src/openspodes.h"
#include "../src/server/server.h"
#include "../src/transport/transport.h"
#include "../src/transport/hdlc_session.h"
#include "../src/ic/data.h"
#include "../src/security/security.h"
#include "../src/codec/serialize.h"
#include "serial_transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  HDLC transport adapter (same as client)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
	osp_hdlc_session_t *session;
} hdlc_transport_ctx_t;

static osp_err_t hdlc_transport_open(void *ctx) { (void)ctx; return OSP_OK; }
static void hdlc_transport_close(void *ctx) { (void)ctx; }

static osp_err_t hdlc_transport_send(void *ctx, const uint8_t *data, uint32_t len) {
	hdlc_transport_ctx_t *c = (hdlc_transport_ctx_t *)ctx;
	return osp_hdlc_session_send_apdu(c->session, data, len);
}

static osp_err_t hdlc_transport_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms) {
	hdlc_transport_ctx_t *c = (hdlc_transport_ctx_t *)ctx;
	return osp_hdlc_session_recv_apdu(c->session, buf, size, out_len, timeout_ms);
}

static bool hdlc_transport_is_connected(void *ctx) {
	hdlc_transport_ctx_t *c = (hdlc_transport_ctx_t *)ctx;
	return c && c->session && osp_hdlc_session_state(c->session) == OSP_HDLC_STATE_CONNECTED;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static speed_t parse_baud(const char *s) {
	if (!s) return B9600;
	uint32_t baud = (uint32_t)strtoul(s, NULL, 10);
	switch (baud) {
		case 1200: return B1200;  case 2400: return B2400;
		case 4800: return B4800;  case 9600: return B9600;
		case 19200: return B19200; case 38400: return B38400;
		case 57600: return B57600; case 115200: return B115200;
		default: return B9600;
	}
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <serial-port> [baud]\n", argv[0]);
		return 1;
	}

	const char *port = argv[1];
	speed_t baud = parse_baud(argc >= 3 ? argv[2] : "9600");

	printf("OpenSPODES Serial+HDLC DLMS/COSEM Server\n");
	printf("Port: %s, Baud: %u\n\n", port, baud);

	/* ── 1. Open serial port ─────────────────────────────────────── */

	linux_serial_ctx_t serial_ctx;
	osp_transport_t serial_transport;
	osp_err_t r = linux_serial_transport_init(&serial_transport, &serial_ctx, port, baud);
	if (r != OSP_OK) {
		fprintf(stderr, "Failed to open %s: %d\n", port, r);
		return 1;
	}
	printf("[1] Serial port opened\n");

	/* ── 2. Init HDLC session (server side) ──────────────────────── */

	osp_hdlc_session_t hdlc;
	osp_hdlc_session_init_server(&hdlc, &serial_transport,
	                              0x01, 1, /* server address = 1, 1 byte */
	                              0x02, 1  /* client address = 2, 1 byte */);

	printf("[2] HDLC session initialized (server=0x01, client=0x02)\n");

	/* ── 3. HDLC accept: wait for SNRM → respond UA ──────────────── */

	printf("[3] Waiting for HDLC SNRM...\n");
	r = osp_hdlc_session_connect(&hdlc, 30000); /* 30s timeout */
	if (r != OSP_OK) {
		fprintf(stderr, "HDLC accept failed: %d\n", r);
		serial_transport.close(serial_transport.ctx);
		return 1;
	}
	printf("    SNRM received, UA sent (max_info=%u, window=%u)\n",
	       hdlc.xid.max_info_tx, hdlc.xid.window_tx);

	/* ── 4. Setup server with Data IC objects ─────────────────────── */

	hdlc_transport_ctx_t hdlc_ctx = {.session = &hdlc};
	osp_transport_t hdlc_transport = {
	    .open = hdlc_transport_open,
	    .send = hdlc_transport_send,
	    .recv = hdlc_transport_recv,
	    .close = hdlc_transport_close,
	    .is_connected = hdlc_transport_is_connected,
	    .ctx = &hdlc_ctx,
	};

	osp_server_t server;
	osp_server_init(&server, &hdlc_transport, OSP_FRAMING_NONE);

	/* Register Data objects */
	osp_ic_data_t energy_a, energy_r, power;
	osp_ic_data_init(&energy_a, (osp_obis_t){1, 0, 1, 8, 0, 255});
	energy_a.value = osp_val_u32(123456);
	osp_server_register(&server, osp_ic_data_class(), &energy_a);

	osp_ic_data_init(&energy_r, (osp_obis_t){1, 0, 2, 8, 0, 255});
	energy_r.value = osp_val_u32(654321);
	osp_server_register(&server, osp_ic_data_class(), &energy_r);

	osp_ic_data_init(&power, (osp_obis_t){1, 0, 9, 7, 0, 255});
	power.value = osp_val_u32(1250);
	osp_server_register(&server, osp_ic_data_class(), &power);

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &sec);

	printf("[4] Server ready: 3 Data objects registered\n");
	printf("    1.0.1.8.0.255 = 123456 (Active energy A+)\n");
	printf("    1.0.2.8.0.255 = 654321 (Active energy R+)\n");
	printf("    1.0.9.7.0.255 = 1250   (Active power+)\n");

	/* ── 5. Accept requests loop ──────────────────────────────────── */

	printf("\n[5] Waiting for requests...\n");
	int request_count = 0;

	for (;;) {
		r = osp_server_accept(&server, 30000);
		if (r == OSP_ERR_TIMEOUT) {
			printf("    Timeout, waiting...\n");
			continue;
		}
		if (r != OSP_OK) {
			printf("    Accept error: %d\n", r);
			break;
		}
		request_count++;
		printf("    Request #%d handled\n", request_count);
	}

	printf("\n[6] Server shutting down (handled %d requests)\n", request_count);

	/* ── 7. HDLC disconnect ──────────────────────────────────────── */

	osp_hdlc_session_disconnect(&hdlc, 2000);
	serial_transport.close(serial_transport.ctx);
	printf("[7] Done.\n");

	return 0;
}
