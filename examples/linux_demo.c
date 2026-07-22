/**
 * linux_demo.c — Demo: Linux HAL + DLMS/COSEM client
 *
 * Usage: openspodes_linux_demo [host:port]
 * Default: 127.0.0.1:4059
 *
 * Connects to a DLMS/COSEM meter via TCP+wrapper, reads Data object.
 * Demonstrates the full HAL abstraction.
 */

#include "linux_hal.h"
#include "../src/client/client.h"
#include "../src/security/security.h"
#include "../src/ic/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_host_port(const char *text, char *host, size_t host_size, uint16_t *port) {
	const char *colon = strrchr(text, ':');
	if (colon && colon != text) {
		size_t hlen = (size_t)(colon - text);
		if (hlen >= host_size)
			return -1;
		memcpy(host, text, hlen);
		host[hlen] = '\0';
		*port = (uint16_t)strtoul(colon + 1, NULL, 10);
		return 0;
	}
	strncpy(host, text, host_size - 1);
	host[host_size - 1] = '\0';
	*port = 4059;
	return 0;
}

static void print_value(const osp_value_t *v) {
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
}

int main(int argc, char **argv) {
	const char *target = (argc >= 2) ? argv[1] : "127.0.0.1:4059";
	char host[256];
	uint16_t port;
	if (parse_host_port(target, host, sizeof(host), &port) != 0) {
		fprintf(stderr, "invalid host:port: %s\n", target);
		return 1;
	}

	/* Initialize Linux HAL */
	osp_hal_t hal;
	linux_hal_init(&hal);
	linux_hal_set_tcp(&hal, host, port);

	if (!hal.transport.is_connected(hal.transport.ctx)) {
		fprintf(stderr, "TCP connect failed to %s:%u\n", host, port);
		return 1;
	}

	printf("Connected to %s:%u\n", host, port);

	/* Initialize client */
	osp_client_t client;
	osp_client_init(&client, &hal.transport, OSP_FRAMING_NONE);

	/* Security: lowest (no authentication) */
	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &sec);

	/* Connect (AARQ→AARE) */
	printf("Associating...\n");
	if (osp_client_connect(&client, 10000) != OSP_OK) {
		fprintf(stderr, "Association failed\n");
		hal.transport.close(hal.transport.ctx);
		return 1;
	}
	printf("Associated.\n");

	/* GET Data 1.0.1.8.0.255 attribute 1 (active energy import) */
	osp_obis_t obis = {1, 0, 1, 8, 0, 255};
	osp_value_t result;
	printf("GET 1.0.1.8.0.255 attr 1... ");
	if (osp_client_get(&client, 1, &obis, 2, &result) == OSP_OK) {
		print_value(&result);
		printf("\n");
	} else {
		printf("FAILED\n");
	}

	/* Release */
	osp_client_release(&client);
	printf("Released.\n");

	hal.transport.close(hal.transport.ctx);
	return 0;
}
