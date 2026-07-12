/**
 * push_listener.c — DLMS/COSEM Push (Data Notification) listener example
 *
 * Demonstrates:
 *   - Server sends Data Notification (0x0F) to connected client
 *   - Client receives and decodes the notification
 *   - Both event-notification and data-notification flows
 *
 * Build:
 *   cmake --build build --target openspodes_push_listener
 *   ./build/openspodes_push_listener
 */

#include "../src/openspodes.h"
#include "../src/client/client.h"
#include "../src/server/server.h"
#include "../src/ic/data.h"
#include "../src/service/notification.h"
#include "../src/security/security.h"
#include "../tests/mock_transport.h"
#include "../tests/mock_crypto.h"

#include <stdio.h>
#include <string.h>

/* ── Mock loopback transport ─────────────────────────────────────────────── */

static osp_server_t *g_server;

static osp_err_t push_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_loopback_send(p, g_server, data, len);
}

static osp_err_t push_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout);
}

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static const char *err_str(osp_err_t r) {
	switch (r) {
		case OSP_OK:            return "ok";
		case OSP_ERR_TIMEOUT:   return "timeout";
		case OSP_ERR_IO:        return "io";
		case OSP_ERR_INVALID:   return "invalid";
		case OSP_ERR_SECURITY:  return "security";
		case OSP_ERR_NOT_FOUND: return "not_found";
		default:                return "error";
	}
}

static void print_value(const char *label, const osp_value_t *v) {
	printf("  %-30s = ", label);
	switch (v->tag) {
		case OSP_TAG_DOUBLE_LONG_UNS:
			printf("%u", v->as.uint32.value);
			break;
		case OSP_TAG_OCTETSTRING:
			printf("octet[%u]", (unsigned)v->as.octetstring.len);
			break;
		case OSP_TAG_VISIBLESTRING:
			printf("\"%.*s\"", (int)v->as.visiblestring.len, v->as.visiblestring.data);
			break;
		default:
			printf("<tag %u>", v->tag);
			break;
	}
	printf("\n");
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(void) {
	printf("=== Push Listener Demo ===\n\n");

	/* Init crypto and transport */
	mock_crypto_init();
	mock_transport_pair_t pair;
	mock_transport_pair_init(&pair);
	pair.client_transport.send = push_send;
	pair.client_transport.recv = push_recv;
	pair.client_transport.ctx = &pair;

	/* Init server */
	osp_server_t server;
	if (osp_server_init(&server, &pair.server_transport, OSP_FRAMING_NONE) != OSP_OK) {
		fprintf(stderr, "server init failed\n");
		return 1;
	}

	/* Register a Data IC */
	osp_ic_data_t data_ic;
	osp_ic_data_init(&data_ic, (osp_obis_t){0, 0, 1, 0, 0, 255});
	data_ic.value = osp_val_u32(100);
	osp_server_register(&server, osp_ic_data_class(), &data_ic);

	/* Set security (lowest — no auth for demo) */
	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &sec);

	/* Init client */
	osp_client_t client;
	if (osp_client_init(&client, &pair.client_transport, OSP_FRAMING_NONE) != OSP_OK) {
		fprintf(stderr, "client init failed\n");
		return 1;
	}
	osp_client_set_security(&client, &sec);

	/* Connect client to server */
	g_server = &server;
	osp_err_t r = osp_client_connect(&client, 5000);
	if (r != OSP_OK) {
		fprintf(stderr, "connect failed: %s\n", err_str(r));
		return 1;
	}
	printf("[client] Connected to server\n");

	/* ── 1. Server sends Data Notification ─────────────────────────────── */

	printf("\n--- 1. Server sends Data Notification ---\n");

	osp_data_notification_t dn;
	memset(&dn, 0, sizeof(dn));
	dn.long_invoke_id_and_priority = 0x12345678;
	/* Set a simple date-time: 2026-07-12 12:00:00.00 UTC */
	uint8_t dt[] = {0x07, 0x00, 0x7D, 0x0C, 0x0C, 0x00, 0x00, 0x00, 0xFF, 0x80, 0x00, 0x00};
	memcpy(dn.date_time, dt, sizeof(dt));
	dn.date_time_len = sizeof(dt);
	dn.notification_body = osp_val_u32(9999);

	r = osp_server_send_data_notification(&server, &dn);
	printf("[server] Send data notification: %s\n", err_str(r));

	/* Client receives the notification */
	osp_data_notification_t received_dn;
	r = osp_client_recv_data_notification(&client, &received_dn, 5000);
	if (r == OSP_OK) {
		printf("[client] Received data notification:\n");
		printf("  invoke_id = 0x%08X\n", received_dn.long_invoke_id_and_priority);
		print_value("notification_body", &received_dn.notification_body);
	} else {
		printf("[client] Receive failed: %s\n", err_str(r));
	}

	/* ── 2. Server sends Event Notification ────────────────────────────── */

	printf("\n--- 2. Server sends Event Notification ---\n");

	osp_event_notification_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.has_time = 1;
	memcpy(ev.time, dt, sizeof(dt));
	ev.time_len = sizeof(dt);
	ev.attribute.class_id = 1;
	ev.attribute.instance_id = (osp_obis_t){0, 0, 1, 0, 0, 255};
	ev.attribute.attribute_id = 2;
	ev.value = osp_val_u32(42);

	r = osp_server_send_event_notification(&server, &ev);
	printf("[server] Send event notification: %s\n", err_str(r));

	osp_event_notification_t received_ev;
	r = osp_client_recv_event_notification(&client, &received_ev, 5000);
	if (r == OSP_OK) {
		printf("[client] Received event notification:\n");
		printf("  class_id     = %u\n", received_ev.attribute.class_id);
		printf("  attribute_id = %u\n", received_ev.attribute.attribute_id);
		print_value("value", &received_ev.value);
	} else {
		printf("[client] Receive failed: %s\n", err_str(r));
	}

	/* ── 3. Multiple rapid notifications ───────────────────────────────── */

	printf("\n--- 3. Burst: 5 rapid data notifications ---\n");
	for (int i = 0; i < 5; i++) {
		osp_data_notification_t burst_dn;
		memset(&burst_dn, 0, sizeof(burst_dn));
		burst_dn.long_invoke_id_and_priority = (uint32_t)(0xBEEF0000 + i);
		memcpy(burst_dn.date_time, dt, sizeof(dt));
		burst_dn.date_time_len = sizeof(dt);
		burst_dn.notification_body = osp_val_u32((uint32_t)(1000 + i * 111));

		r = osp_server_send_data_notification(&server, &burst_dn);
		printf("[server] Send #%d: %s", i + 1, err_str(r));

		osp_data_notification_t burst_recv;
		r = osp_client_recv_data_notification(&client, &burst_recv, 5000);
		if (r == OSP_OK) {
			printf(" -> [client] recv ok (body=%u)\n", burst_recv.notification_body.as.uint32.value);
		} else {
			printf(" -> [client] recv failed: %s\n", err_str(r));
		}
	}

	/* Cleanup */
	osp_client_disconnect(&client);
	osp_sec_context_destroy(&sec);

	printf("\n=== Done ===\n");
	return 0;
}
