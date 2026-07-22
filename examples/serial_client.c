/**
 * serial_client.c — Full DLMS/COSEM client over Serial + HDLC
 *
 * Usage: openspodes_serial_client /dev/ttyUSB0 [baud]
 * Default: 9600
 *
 * Full flow:
 *   1. Open serial port
 *   2. HDLC: SNRM/UA + XID parameter negotiation
 *   3. DLMS: AARQ → AARE → HLS handshake
 *   4. GET 1.0.1.8.0.255 attribute 1
 *   5. SET 1.0.1.8.0.255 attribute 1
 *   6. RLRQ → RLRE (release)
 *   7. HDLC: DISC/UA (disconnect)
 */

#include "../src/openspodes.h"
#include "../src/client/client.h"
#include "../src/server/server.h"
#include "../src/transport/transport.h"
#include "../src/transport/hdlc_session.h"
#include "../src/service/service.h"
#include "../src/service/initiate.h"
#include "../src/security/security.h"
#include "../src/codec/serialize.h"
#include "../src/ic/data.h"
#include "../src/service/xdms_selective.h"
#include "serial_transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  HDLC transport adapter — wraps osp_hdlc_session_t as osp_transport_t
 *
 *  This allows osp_client_t to use HDLC session layer transparently.
 *  send_apdu → hdlc_session_send_apdu (adds LLC + I-frame)
 *  recv_apdu → hdlc_session_recv_apdu (strips LLC + I-frame)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
	osp_hdlc_session_t *session;
} hdlc_transport_ctx_t;

static osp_err_t hdlc_transport_open(void *ctx) {
	(void)ctx;
	return OSP_OK;
}

static osp_err_t hdlc_transport_send(void *ctx, const uint8_t *data, uint32_t len) {
	hdlc_transport_ctx_t *c = (hdlc_transport_ctx_t *)ctx;
	if (!c || !c->session)
		return OSP_ERR_INVALID;
	return osp_hdlc_session_send_apdu(c->session, data, len);
}

static osp_err_t hdlc_transport_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms) {
	hdlc_transport_ctx_t *c = (hdlc_transport_ctx_t *)ctx;
	if (!c || !c->session)
		return OSP_ERR_INVALID;
	return osp_hdlc_session_recv_apdu(c->session, buf, size, out_len, timeout_ms);
}

static void hdlc_transport_close(void *ctx) {
	(void)ctx;
}

static bool hdlc_transport_is_connected(void *ctx) {
	hdlc_transport_ctx_t *c = (hdlc_transport_ctx_t *)ctx;
	return c && c->session && osp_hdlc_session_state(c->session) == OSP_HDLC_STATE_CONNECTED;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static speed_t parse_baud(const char *s) {
	if (!s)
		return B9600;
	uint32_t baud = (uint32_t)strtoul(s, NULL, 10);
	switch (baud) {
		case 1200: return B1200;
		case 2400: return B2400;
		case 4800: return B4800;
		case 9600: return B9600;
		case 19200: return B19200;
		case 38400: return B38400;
		case 57600: return B57600;
		case 115200: return B115200;
		default: return B9600;
	}
}

static void print_value(const char *label, const osp_value_t *v) {
	printf("  %-30s = ", label);
	switch (v->tag) {
		case OSP_TAG_DOUBLE_LONG_UNS:
			printf("%u", v->as.uint32.value);
			break;
		case OSP_TAG_DOUBLE_LONG:
			printf("%d", v->as.int32.value);
			break;
		case OSP_TAG_OCTETSTRING:
			printf("octetstring[%u]", v->as.octetstring.len);
			break;
		case OSP_TAG_NULL:
			printf("null");
			break;
		default:
			printf("<tag %u>", v->tag);
			break;
	}
	printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <serial-port> [baud]\n", argv[0]);
		fprintf(stderr, "  Example: %s /dev/ttyUSB0 9600\n", argv[0]);
		fprintf(stderr, "  Example: %s /dev/ttyS0 115200\n", argv[0]);
		return 1;
	}

	const char *port = argv[1];
	speed_t baud = parse_baud(argc >= 3 ? argv[2] : "9600");

	printf("OpenSPODES Serial+HDLC DLMS/COSEM Client\n");
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

	/* ── 2. Init HDLC session (client side) ──────────────────────── */

	osp_hdlc_session_t hdlc;
	osp_hdlc_session_init_client(&hdlc, &serial_transport,
	                              0x02, 1, /* client address = 2, 1 byte */
	                              0x01, 1  /* server address = 1, 1 byte */);

	printf("[2] HDLC session initialized (client=0x02, server=0x01)\n");

	/* ── 3. HDLC connect: SNRM/UA + XID ─────────────────────────── */

	printf("[3] HDLC connecting (SNRM/UA)...\n");
	r = osp_hdlc_session_connect(&hdlc, 5000);
	if (r != OSP_OK) {
		fprintf(stderr, "HDLC connect failed: %d\n", r);
		serial_transport.close(serial_transport.ctx);
		return 1;
	}
	printf("    Link established (max_info_tx=%u, max_info_rx=%u, window=%u)\n",
	       hdlc.xid.max_info_tx, hdlc.xid.max_info_rx, hdlc.xid.window_tx);

	/* ── 4. Setup DLMS client over HDLC transport adapter ────────── */

	hdlc_transport_ctx_t hdlc_ctx = {.session = &hdlc};
	osp_transport_t hdlc_transport = {
	    .open = hdlc_transport_open,
	    .send = hdlc_transport_send,
	    .recv = hdlc_transport_recv,
	    .close = hdlc_transport_close,
	    .is_connected = hdlc_transport_is_connected,
	    .ctx = &hdlc_ctx,
	};

	osp_client_t client;
	osp_client_init(&client, &hdlc_transport, OSP_FRAMING_NONE);

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &sec);

	printf("[4] DLMS client initialized\n");

	/* ── 5. AARQ → AARE (association) ───────────────────────────── */

	printf("[5] Sending AARQ...\n");
	r = osp_client_connect(&client, 5000);
	if (r != OSP_OK) {
		fprintf(stderr, "Association failed: %d\n", r);
		osp_hdlc_session_disconnect(&hdlc, 2000);
		serial_transport.close(serial_transport.ctx);
		return 1;
	}
	printf("    Associated successfully\n");

	/* ── 6. GET 1.0.1.8.0.255 attr 1 ───────────────────────────── */

	printf("[6] GET 1.0.1.8.0.255 attr 1...\n");
	osp_obis_t obis = {1, 0, 1, 8, 0, 255};
	osp_value_t result;
	r = osp_client_get(&client, 1, &obis, 2, &result);
	if (r == OSP_OK) {
		print_value("Active energy A+ (kWh)", &result);
	} else {
		printf("    GET failed: %d\n", r);
	}

	/* ── 7. GET 1.0.2.8.0.255 attr 1 ───────────────────────────── */

	printf("[7] GET 1.0.2.8.0.255 attr 1...\n");
	osp_obis_t obis2 = {1, 0, 2, 8, 0, 255};
	r = osp_client_get(&client, 1, &obis2, 2, &result);
	if (r == OSP_OK) {
		print_value("Active energy R+ (kWh)", &result);
	} else {
		printf("    GET failed: %d\n", r);
	}

	/* ── 8. SET 1.0.1.8.0.255 attr 1 ───────────────────────────── */

	printf("[8] SET 1.0.1.8.0.255 attr 1 = 999999...\n");
	osp_value_t newval = osp_val_u32(999999);
	r = osp_client_set(&client, 1, &obis, 2, &newval);
	if (r == OSP_OK) {
		printf("    SET successful\n");
	} else {
		printf("    SET failed: %d (expected if meter is read-only)\n", r);
	}

	/* ── 9. RLRQ → RLRE (release) ───────────────────────────────── */

	printf("[9] Releasing association...\n");
	r = osp_client_release(&client);
	if (r == OSP_OK) {
		printf("    Released\n");
	} else {
		printf("    Release failed: %d\n", r);
	}

	/* ── 10. HDLC disconnect: DISC/UA ────────────────────────────── */

	printf("[10] HDLC disconnecting (DISC/UA)...\n");
	r = osp_hdlc_session_disconnect(&hdlc, 2000);
	if (r == OSP_OK) {
		printf("     Disconnected\n");
	} else {
		printf("     Disconnect: %d\n", r);
	}

	/* ── 11. Close serial port ────────────────────────────────────── */

	serial_transport.close(serial_transport.ctx);
	printf("[11] Serial port closed\n\n");

	printf("Done.\n");
	return 0;
}
