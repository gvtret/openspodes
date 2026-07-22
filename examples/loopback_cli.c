/**
 * loopback_cli.c — In-process DLMS client↔server demo (no network).
 *
 * Mirrors spodes-rs integration flow using the mock loopback transport.
 *
 * Usage:
 *   openspodes_loopback_cli demo
 *   openspodes_loopback_cli get  <class> <obis> <attr>
 *   openspodes_loopback_cli set  <class> <obis> <attr> u32:<value>
 *
 * Examples:
 *   openspodes_loopback_cli get 1 0.0.1.0.0.255 1
 *   openspodes_loopback_cli set 1 0.0.1.0.0.255 1 u32:100
 */

#include "../src/openspodes.h"
#include "../src/client/client.h"
#include "../src/server/server.h"
#include "../src/ic/data.h"
#include "../src/security/security.h"
#include "../tests/mock_transport.h"
#include "../tests/mock_crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	mock_transport_pair_t pair;
	osp_server_t server;
	osp_client_t client;
	osp_ic_data_t data1;
	osp_ic_data_t data2;
} loopback_session_t;

static osp_server_t *g_server;

static osp_err_t loopback_send(void *ctx, const uint8_t *data, uint32_t len) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_loopback_send(p, g_server, data, len);
}

static osp_err_t loopback_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout) {
	mock_transport_pair_t *p = (mock_transport_pair_t *)ctx;
	return mock_recv_from_peer(&p->client_rx, buf, size, out_len, timeout);
}

static const char *err_str(osp_err_t r) {
	switch (r) {
		case OSP_OK:
			return "ok";
		case OSP_ERR_NOMEM:
			return "nomem";
		case OSP_ERR_IO:
			return "io";
		case OSP_ERR_INVALID:
			return "invalid";
		case OSP_ERR_UNSUPPORTED:
			return "unsupported";
		case OSP_ERR_TIMEOUT:
			return "timeout";
		case OSP_ERR_SECURITY:
			return "security";
		case OSP_ERR_NOT_FOUND:
			return "not_found";
		default:
			return "error";
	}
}

static int parse_obis(const char *text, osp_obis_t *obis) {
	unsigned a, b, c, d, e, f;
	if (sscanf(text, "%u.%u.%u.%u.%u.%u", &a, &b, &c, &d, &e, &f) != 6) {
		return -1;
	}
	obis->a = (uint8_t)a;
	obis->b = (uint8_t)b;
	obis->c = (uint8_t)c;
	obis->d = (uint8_t)d;
	obis->e = (uint8_t)e;
	obis->f = (uint8_t)f;
	return 0;
}

static void print_obis(const osp_obis_t *obis) {
	printf("%u.%u.%u.%u.%u.%u", obis->a, obis->b, obis->c, obis->d, obis->e, obis->f);
}

static int print_value(const osp_value_t *v) {
	switch (v->tag) {
		case OSP_TAG_NULL:
			printf("null");
			return 0;
		case OSP_TAG_BOOLEAN:
			printf("%s", v->as.boolean.value ? "true" : "false");
			return 0;
		case OSP_TAG_INTEGER:
			printf("%d", v->as.int8.value);
			return 0;
		case OSP_TAG_LONG:
			printf("%d", v->as.int16.value);
			return 0;
		case OSP_TAG_DOUBLE_LONG:
			printf("%d", v->as.int32.value);
			return 0;
		case OSP_TAG_UNSIGNED:
			printf("%u", v->as.uint8.value);
			return 0;
		case OSP_TAG_LONG_UNSIGNED:
			printf("%u", v->as.uint16.value);
			return 0;
		case OSP_TAG_DOUBLE_LONG_UNS:
			printf("%u", v->as.uint32.value);
			return 0;
		case OSP_TAG_VISIBLESTRING:
			printf("\"%.*s\"", (int)v->as.visiblestring.len, v->as.visiblestring.data);
			return 0;
		case OSP_TAG_OCTETSTRING:
			printf("octet[");
			for (uint32_t i = 0; i < v->as.octetstring.len; i++) {
				printf("%s%02X", i ? " " : "", v->as.octetstring.data[i]);
			}
			printf("]");
			return 0;
		default:
			printf("<tag %u>", v->tag);
			return 0;
	}
}

static int parse_value_u32(const char *text, osp_value_t *out) {
	if (strncmp(text, "u32:", 4) != 0) {
		return -1;
	}
	char *end = NULL;
	unsigned long val = strtoul(text + 4, &end, 10);
	if (!end || *end != '\0') {
		return -1;
	}
	*out = osp_val_u32((uint32_t)val);
	return 0;
}

static void register_demo_objects(loopback_session_t *s) {
	osp_ic_data_init(&s->data1, (osp_obis_t){0, 0, 1, 0, 0, 255});
	s->data1.value = osp_val_u32(42);
	osp_server_register(&s->server, osp_ic_data_class(), &s->data1);

	osp_ic_data_init(&s->data2, (osp_obis_t){1, 0, 1, 8, 0, 255});
	s->data2.value = osp_val_u32(123456);
	osp_server_register(&s->server, osp_ic_data_class(), &s->data2);
}

static int session_open(loopback_session_t *s) {
	mock_crypto_init();
	mock_transport_pair_init(&s->pair);
	s->pair.client_transport.send = loopback_send;
	s->pair.client_transport.recv = loopback_recv;
	s->pair.client_transport.ctx = &s->pair;

	if (osp_server_init(&s->server, &s->pair.server_transport, OSP_FRAMING_NONE) != OSP_OK) {
		return -1;
	}
	register_demo_objects(s);

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&s->server, &sec);

	g_server = &s->server;
	if (osp_client_init(&s->client, &s->pair.client_transport, OSP_FRAMING_NONE) != OSP_OK) {
		return -1;
	}
	osp_client_set_security(&s->client, &sec);

	osp_err_t r = osp_client_connect(&s->client, 5000);
	if (r != OSP_OK) {
		fprintf(stderr, "connect failed: %s\n", err_str(r));
		return -1;
	}
	return 0;
}

static void session_close(loopback_session_t *s) {
	osp_client_release(&s->client);
	osp_client_disconnect(&s->client);
	g_server = NULL;
}

static int cmd_get(loopback_session_t *s, int argc, char **argv) {
	if (argc < 5) {
		fprintf(stderr, "usage: get <class> <obis> <attr>\n");
		return 1;
	}
	uint16_t class_id = (uint16_t)strtoul(argv[2], NULL, 10);
	osp_obis_t obis;
	if (parse_obis(argv[3], &obis) != 0) {
		fprintf(stderr, "invalid obis: %s\n", argv[3]);
		return 1;
	}
	uint8_t attr = (uint8_t)strtoul(argv[4], NULL, 10);

	osp_value_t result;
	osp_err_t r = osp_client_get(&s->client, class_id, &obis, attr, &result);
	if (r != OSP_OK) {
		fprintf(stderr, "GET failed: %s\n", err_str(r));
		return 1;
	}
	printf("GET class=%u obis=", class_id);
	print_obis(&obis);
	printf(" attr=%u -> ", attr);
	print_value(&result);
	printf("\n");
	return 0;
}

static int cmd_set(loopback_session_t *s, int argc, char **argv) {
	if (argc < 6) {
		fprintf(stderr, "usage: set <class> <obis> <attr> u32:<value>\n");
		return 1;
	}
	uint16_t class_id = (uint16_t)strtoul(argv[2], NULL, 10);
	osp_obis_t obis;
	if (parse_obis(argv[3], &obis) != 0) {
		fprintf(stderr, "invalid obis: %s\n", argv[3]);
		return 1;
	}
	uint8_t attr = (uint8_t)strtoul(argv[4], NULL, 10);
	osp_value_t value;
	if (parse_value_u32(argv[5], &value) != 0) {
		fprintf(stderr, "unsupported value (use u32:N): %s\n", argv[5]);
		return 1;
	}

	osp_err_t r = osp_client_set(&s->client, class_id, &obis, attr, &value);
	if (r != OSP_OK) {
		fprintf(stderr, "SET failed: %s\n", err_str(r));
		return 1;
	}
	printf("SET class=%u obis=", class_id);
	print_obis(&obis);
	printf(" attr=%u <- ", attr);
	print_value(&value);
	printf(" ok\n");
	return 0;
}

static int cmd_demo(loopback_session_t *s) {
	osp_obis_t obis = {0, 0, 1, 0, 0, 255};
	osp_value_t v;

	printf("== demo: GET 0.0.1.0.0.255 ==\n");
	if (osp_client_get(&s->client, 1, &obis, 2, &v) != OSP_OK) {
		return 1;
	}
	printf("value: ");
	print_value(&v);
	printf("\n");

	printf("== demo: SET u32:100 ==\n");
	osp_value_t w = osp_val_u32(100);
	if (osp_client_set(&s->client, 1, &obis, 2, &w) != OSP_OK) {
		return 1;
	}

	printf("== demo: GET again ==\n");
	if (osp_client_get(&s->client, 1, &obis, 2, &v) != OSP_OK) {
		return 1;
	}
	printf("value: ");
	print_value(&v);
	printf("\n");

	osp_obis_t energy = {1, 0, 1, 8, 0, 255};
	printf("== demo: GET active energy 1.0.1.8.0.255 ==\n");
	if (osp_client_get(&s->client, 1, &energy, 2, &v) != OSP_OK) {
		return 1;
	}
	printf("value: ");
	print_value(&v);
	printf("\n");
	return 0;
}

static void usage(const char *prog) {
	fprintf(stderr, "OpenSPODES loopback client/server demo\n\n");
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  %s demo\n", prog);
	fprintf(stderr, "  %s get  <class> <a.b.c.d.e.f> <attr>\n", prog);
	fprintf(stderr, "  %s set  <class> <a.b.c.d.e.f> <attr> u32:<value>\n", prog);
	fprintf(stderr, "\nBuilt-in objects:\n");
	fprintf(stderr, "  Data 0.0.1.0.0.255 value=42\n");
	fprintf(stderr, "  Data 1.0.1.8.0.255 value=123456\n");
}

int main(int argc, char **argv) {
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	loopback_session_t session;
	if (session_open(&session) != 0) {
		return 1;
	}

	int rc = 1;
	if (strcmp(argv[1], "demo") == 0) {
		rc = cmd_demo(&session);
	} else if (strcmp(argv[1], "get") == 0) {
		rc = cmd_get(&session, argc, argv);
	} else if (strcmp(argv[1], "set") == 0) {
		rc = cmd_set(&session, argc, argv);
	} else {
		usage(argv[0]);
		rc = 1;
	}

	session_close(&session);
	return rc;
}
