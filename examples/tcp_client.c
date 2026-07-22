/**
 * tcp_client.c — DLMS/COSEM TCP client with COSEM wrapper (IEC 62056-47).
 *
 * Usage: openspodes_tcp_client [host:port]
 * Default: 127.0.0.1:4059
 *
 * Connects and reads Data 1.0.1.8.0.255 attribute 1 (value).
 */

#include "../src/openspodes.h"
#include "../src/client/client.h"
#include "../src/security/security.h"
#include "../tests/mock_crypto.h"
#include "tcp_socket.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int parse_host_port(const char *text, char *host, size_t host_size, uint16_t *port) {
	const char *colon = strrchr(text, ':');
	if (colon && colon != text) {
		size_t hlen = (size_t)(colon - text);
		if (hlen >= host_size) {
			return -1;
		}
		memcpy(host, text, hlen);
		host[hlen] = '\0';
		*port = (uint16_t)strtoul(colon + 1, NULL, 10);
		return 0;
	}
	strncpy(host, text, host_size - 1);
	host[host_size - 1] = '\0';
	*port = OSP_TCP_DLMS_PORT;
	return 0;
}

static int connect_tcp(const char *host, uint16_t port) {
	char port_str[8];
	snprintf(port_str, sizeof(port_str), "%u", port);

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo *res = NULL;
	if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) {
		return -1;
	}

	int fd = -1;
	for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
		fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (fd < 0) {
			continue;
		}
		if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
			break;
		}
		close(fd);
		fd = -1;
	}
	freeaddrinfo(res);
	return fd;
}

static void print_value(const osp_value_t *v) {
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
}

int main(int argc, char **argv) {
	const char *target = (argc >= 2) ? argv[1] : "127.0.0.1:4059";
	char host[256];
	uint16_t port;
	if (parse_host_port(target, host, sizeof(host), &port) != 0) {
		fprintf(stderr, "invalid host:port: %s\n", target);
		return 1;
	}

	printf("Connecting to %s:%u...\n", host, port);
	int fd = connect_tcp(host, port);
	if (fd < 0) {
		fprintf(stderr, "connect failed\n");
		return 1;
	}

	mock_crypto_init();

	osp_tcp_wrapper_ctx_t tctx;
	osp_transport_t transport;
	osp_tcp_wrapper_transport_init(&transport, &tctx, fd, OSP_TCP_CLIENT_WPORT, OSP_TCP_DLMS_PORT);

	osp_client_t client;
	if (osp_client_init(&client, &transport, OSP_FRAMING_NONE) != OSP_OK) {
		fprintf(stderr, "client init failed\n");
		close(fd);
		return 1;
	}

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_client_set_security(&client, &sec);

	if (osp_client_connect(&client, 10000) != OSP_OK) {
		fprintf(stderr, "association failed\n");
		transport.close(transport.ctx);
		return 1;
	}

	osp_obis_t obis = {1, 0, 1, 8, 0, 255};
	osp_value_t result;
	if (osp_client_get(&client, 1, &obis, 2, &result) != OSP_OK) {
		fprintf(stderr, "GET failed\n");
		osp_client_release(&client);
		transport.close(transport.ctx);
		return 1;
	}

	printf("GET 1.0.1.8.0.255 attr 1 = ");
	print_value(&result);
	printf("\n");

	osp_client_release(&client);
	transport.close(transport.ctx);
	return 0;
}
