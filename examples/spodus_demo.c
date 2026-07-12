/**
 * spodus_demo.c — SPODUS concentrator with 3 meters
 *
 * Demonstrates:
 *   - Registration of 3 meters in the registry
 *   - Polling each meter (GET attributes) via mock loopback
 *   - Caching results
 *   - Building channel lists for upstream
 *
 * Build:
 *   cmake --build build --target openspodes_spodus_demo
 *   ./build/openspodes_spodus_demo
 */

#include "openspodes.h"
#include "spodus/concentrator.h"
#include "spodus/spodus_data.h"
#include "spodus/spodus_obis.h"
#include "server/server.h"
#include "client/client.h"
#include "ic/data.h"
#include "service/service.h"
#include "codec/serialize.h"
#include "security/security.h"

#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  Mock transport: simple loopback for meter simulation
 * ═══════════════════════════════════════════════════════════════════════════ */

#define MOCK_BUF_SIZE 4096

typedef struct {
	uint8_t data[MOCK_BUF_SIZE];
	uint32_t len;
	uint32_t rpos;
	uint32_t msg_starts[64];
	uint32_t msg_count;
	uint32_t msg_index;
} mock_buf_t;

typedef struct {
	osp_transport_t client_transport;
	osp_transport_t server_transport;
	mock_buf_t server_rx;
	mock_buf_t client_rx;
	osp_server_t *server;
} mock_pair_t;

static osp_err_t mock_send_to(mock_buf_t *dst, const uint8_t *data, uint32_t len) {
	if (dst->len + len > MOCK_BUF_SIZE)
		return OSP_ERR_NOMEM;
	if (dst->msg_count < 64)
		dst->msg_starts[dst->msg_count++] = dst->len;
	memcpy(&dst->data[dst->len], data, len);
	dst->len += len;
	return OSP_OK;
}

static osp_err_t mock_recv_from(mock_buf_t *src, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms) {
	(void)timeout_ms;
	if (src->msg_index >= src->msg_count)
		return OSP_ERR_TIMEOUT;
	uint32_t start = src->msg_starts[src->msg_index];
	uint32_t end = (src->msg_index + 1 < src->msg_count) ? src->msg_starts[src->msg_index + 1] : src->len;
	uint32_t avail = end - start;
	if (avail == 0)
		return OSP_ERR_TIMEOUT;
	uint32_t n = avail < size ? avail : size;
	memcpy(buf, &src->data[start], n);
	src->msg_index++;
	src->rpos = end;
	*out_len = n;
	return OSP_OK;
}

/* Client send → server_rx, auto-process server */
static osp_err_t pair_client_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_pair_t *p = (mock_pair_t *)ctx;
	mock_send_to(&p->server_rx, data, len);
	if (p->server) {
		osp_server_accept(p->server, 0);
	}
	return OSP_OK;
}

static osp_err_t pair_client_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_pair_t *p = (mock_pair_t *)ctx;
	return mock_recv_from(&p->client_rx, buf, size, out_len, timeout);
}

static osp_err_t pair_server_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_pair_t *p = (mock_pair_t *)ctx;
	return mock_send_to(&p->client_rx, data, len);
}

static osp_err_t pair_server_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_pair_t *p = (mock_pair_t *)ctx;
	return mock_recv_from(&p->server_rx, buf, size, out_len, timeout);
}

static osp_err_t mock_open(void *ctx) { (void)ctx; return OSP_OK; }
static void mock_close(void *ctx) { (void)ctx; }
static bool mock_is_connected(void *ctx) { (void)ctx; return true; }

static void pair_init(mock_pair_t *p) {
	memset(p, 0, sizeof(*p));
	p->client_transport.open = mock_open;
	p->client_transport.send = pair_client_send;
	p->client_transport.recv = pair_client_recv;
	p->client_transport.close = mock_close;
	p->client_transport.is_connected = mock_is_connected;
	p->client_transport.ctx = p;
	p->server_transport.open = mock_open;
	p->server_transport.send = pair_server_send;
	p->server_transport.recv = pair_server_recv;
	p->server_transport.close = mock_close;
	p->server_transport.is_connected = mock_is_connected;
	p->server_transport.ctx = p;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Meter model: Data IC objects
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
	osp_server_t server;
	mock_pair_t pair;
	osp_ic_data_t energy;
	const char *name;
} simulated_meter_t;

static void meter_init(simulated_meter_t *m, const char *name, osp_obis_t obis, uint32_t value) {
	memset(m, 0, sizeof(*m));
	m->name = name;
	pair_init(&m->pair);

	osp_server_init(&m->server, &m->pair.server_transport, OSP_FRAMING_NONE);
	m->pair.server = &m->server;

	osp_server_init(&m->server, &m->pair.server_transport, OSP_FRAMING_NONE);

	osp_ic_data_init(&m->energy, obis);
	m->energy.value = osp_val_u32(value);
	osp_server_register(&m->server, osp_ic_data_class(), &m->energy);

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&m->server, &sec);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Value printing
 * ═══════════════════════════════════════════════════════════════════════════ */

static void print_value(const char *label, const osp_value_t *v) {
	printf("  %-24s = ", label);
	switch (v->tag) {
		case OSP_TAG_DOUBLE_LONG_UNS:
			printf("%u", v->as.uint32.value);
			break;
		case OSP_TAG_DOUBLE_LONG:
			printf("%d", v->as.int32.value);
			break;
		default:
			printf("<tag %u>", v->tag);
			break;
	}
	printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Poll single meter
 * ═══════════════════════════════════════════════════════════════════════════ */

static void poll_meter(simulated_meter_t *m, osp_obis_t obis, uint8_t attr_id, const char *label) {
	osp_client_t client;
	osp_client_init(&client, &m->pair.client_transport, OSP_FRAMING_NONE);
	osp_sec_context_t csec;
	osp_sec_context_init(&csec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &csec);

	if (osp_client_connect(&client, 1000) != OSP_OK) {
		printf("  [%s] connect FAIL\n", label);
		osp_client_disconnect(&client);
		return;
	}

	osp_value_t result;
	if (osp_client_get(&client, 1, &obis, attr_id, &result) == OSP_OK) {
		print_value(label, &result);
	} else {
		printf("  [%s] GET FAIL\n", label);
	}

	osp_client_release(&client);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
	printf("=== SPODUS Concentrator: 3 meters ===\n\n");

	/* ── 1. Create 3 simulated meters ─────────────────────────────── */

	simulated_meter_t meters[3];
	osp_obis_t energy_obis = {1, 0, 1, 8, 0, 255};

	meter_init(&meters[0], "MTR-0001", energy_obis, 123456);
	meter_init(&meters[1], "MTR-0002", energy_obis, 456789);
	meter_init(&meters[2], "MTR-0003", energy_obis, 789012);

	printf("Meters created:\n");
	for (int i = 0; i < 3; i++) {
		printf("  %d. %s (Data OBIS 1.0.1.8.0.255)\n", i + 1, meters[i].name);
	}

	/* ── 2. Register meters in concentrator registry ────────────────── */

	osp_spodus_concentrator_t conc;
	osp_spodus_concentrator_init(&conc);

	struct {
		const char *id;
		const char *model;
		uint32_t energy_val;
	} meter_info[3] = {
	    {"MTR-0001", "MOD-A", 123456},
	    {"MTR-0002", "MOD-B", 456789},
	    {"MTR-0003", "MOD-C", 789012},
	};

	for (int i = 0; i < 3; i++) {
		osp_spodus_meter_descriptor_t desc = {0};
		desc.meter_id_len = (uint8_t)strlen(meter_info[i].id);
		memcpy(desc.meter_id, meter_info[i].id, desc.meter_id_len);
		desc.meter_model_len = (uint8_t)strlen(meter_info[i].model);
		memcpy(desc.meter_model, meter_info[i].model, desc.meter_model_len);
		desc.channel_count = 1;
		desc.channels[0].id = 1;
		desc.channels[0].address_len = 1;
		desc.channels[0].address[0] = (uint8_t)(0x11 + i);

		osp_err_t r = osp_spodus_registry_add(&conc.registry, &desc);
		printf("  Register %s: %s\n", meter_info[i].id, r == OSP_OK ? "OK" : "FAIL");
	}

	/* ── 3. Connect downstream transport ─────────────────────────── */

	for (int i = 0; i < 3; i++) {
		const uint8_t *mid = (const uint8_t *)meter_info[i].id;
		uint8_t mid_len = (uint8_t)strlen(meter_info[i].id);
		osp_err_t r = osp_spodus_concentrator_attach_downstream(&conc, mid, mid_len, &meters[i].pair.client_transport, OSP_FRAMING_NONE);
		printf("  Connect %s: %s\n", meter_info[i].id, r == OSP_OK ? "OK" : "FAIL");
	}

	/* ── 4. Poll each meter via direct loopback ─────────────── */

	printf("\n--- Direct meter polling ---\n");
	for (int i = 0; i < 3; i++) {
		printf("\nMeter %d: %s\n", i + 1, meters[i].name);
		poll_meter(&meters[i], energy_obis, 1, "Energy A+ (kWh)");
	}

	/* ── 5. poll_meter (poll_meter) ──────────────────── */

	printf("\n--- poll_meter ---\n");
	for (int i = 0; i < 3; i++) {
		const uint8_t *mid = (const uint8_t *)meter_info[i].id;
		uint8_t mid_len = (uint8_t)strlen(meter_info[i].id);

		osp_spodus_attr_ref_t attrs[1];
		attrs[0].class_id = 1;
		attrs[0].obis = energy_obis;
		attrs[0].attribute_id = 1;

		osp_spodus_downstream_t *ds = osp_spodus_concentrator_downstream(&conc, mid, mid_len);
		if (!ds) {
			printf("  Meter %d: downstream not found\n", i + 1);
			continue;
		}

		/* poll_meter requires connected state — reconnect */
		osp_client_connect(&ds->client, 1000);
		uint32_t found = osp_spodus_poll_meter(&ds->client, &conc.registry, mid, mid_len, attrs, 1);
		osp_client_release(&ds->client);
		printf("  Meter %d (%s): polled %u attributes\n", i + 1, meter_info[i].id, found);

		const osp_value_t *cached = osp_spodus_registry_cached(&conc.registry, mid, mid_len, &energy_obis, 1);
		if (cached)
			print_value("  Energy cache", cached);
	}

	/* ── 6. Build meter list ────────────────────────────────────── */

	printf("\n--- Meter list in registry ---\n");
	osp_value_t meter_list;
	if (osp_spodus_registry_build_meter_list(&conc.registry, &meter_list) == OSP_OK) {
		printf("Total meters: %u\n", meter_list.as.array.elements.count);
		for (uint8_t i = 0; i < meter_list.as.array.elements.count; i++) {
			osp_value_t *item = &meter_list.as.array.elements.items[i];
			osp_value_t *id_val = &item->as.structure.elements.items[0];
			osp_value_t *model_val = &item->as.structure.elements.items[1];
			printf("  [%u] ", i + 1);
			if (id_val->tag == OSP_TAG_OCTETSTRING)
				fwrite(id_val->as.octetstring.data, 1, id_val->as.octetstring.len, stdout);
			if (model_val->tag == OSP_TAG_OCTETSTRING) {
				printf("  model: ");
				fwrite(model_val->as.octetstring.data, 1, model_val->as.octetstring.len, stdout);
			}
			printf("\n");
		}
	}

	/* ── 7. Cache summary ───────────────────────────────────── */

	printf("\n--- Readings cache ---\n");
	for (int i = 0; i < 3; i++) {
		const uint8_t *mid = (const uint8_t *)meter_info[i].id;
		uint8_t mid_len = (uint8_t)strlen(meter_info[i].id);
		const osp_value_t *cached = osp_spodus_registry_cached(&conc.registry, mid, mid_len, &energy_obis, 1);
		if (cached)
			printf("  %s: %u kWh\n", meter_info[i].id, cached->as.uint32.value);
	}

	printf("\n=== Done ===\n");
	return 0;
}
