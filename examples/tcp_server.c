/**
 * tcp_server.c — DLMS/COSEM TCP server with COSEM wrapper (IEC 62056-47).
 *
 * Usage: openspodes_tcp_server [port]
 * Default port: 4059
 *
 * Serves one Data object (active energy import) per TCP connection.
 */

#include "../src/openspodes.h"
#include "../src/server/server.h"
#include "../src/ic/data.h"
#include "../src/security/security.h"
#include "../tests/mock_crypto.h"
#include "tcp_socket.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void register_server_objects(osp_server_t *server, osp_ic_data_t *energy) {
	osp_ic_data_init(energy, (osp_obis_t){1, 0, 1, 8, 0, 255});
	energy->value = osp_val_u32(123456);
	osp_server_register(server, osp_ic_data_class(), energy);
}

static int handle_client(int cfd, struct sockaddr_in *peer) {
	printf("Client connected: %s:%u\n", inet_ntoa(peer->sin_addr), ntohs(peer->sin_port));

	osp_tcp_wrapper_ctx_t tctx;
	osp_transport_t transport;
	osp_tcp_wrapper_transport_init(&transport, &tctx, cfd, OSP_TCP_DLMS_PORT, OSP_TCP_CLIENT_WPORT);

	osp_server_t server;
	if (osp_server_init(&server, &transport, OSP_FRAMING_NONE) != OSP_OK) {
		return 1;
	}

	osp_ic_data_t energy;
	register_server_objects(&server, &energy);

	osp_sec_context_t sec;
	osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_LOWEST, NULL);
	osp_server_set_security(&server, &sec);

	mock_crypto_init();

	for (;;) {
		osp_err_t r = osp_server_accept(&server, 30000);
		if (r == OSP_ERR_TIMEOUT) {
			continue;
		}
		if (r != OSP_OK) {
			break;
		}
	}

	printf("Client disconnected\n");
	transport.close(transport.ctx);
	return 0;
}

int main(int argc, char **argv) {
	uint16_t port = OSP_TCP_DLMS_PORT;
	if (argc >= 2) {
		port = (uint16_t)strtoul(argv[1], NULL, 10);
	}

	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd < 0) {
		perror("socket");
		return 1;
	}

	int yes = 1;
	setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if (bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		perror("bind");
		close(lfd);
		return 1;
	}
	if (listen(lfd, 4) != 0) {
		perror("listen");
		close(lfd);
		return 1;
	}

	printf("OpenSPODES TCP server listening on port %u\n", port);

	for (;;) {
		struct sockaddr_in peer;
		socklen_t peer_len = sizeof(peer);
		int cfd = accept(lfd, (struct sockaddr *)&peer, &peer_len);
		if (cfd < 0) {
			perror("accept");
			continue;
		}
		handle_client(cfd, &peer);
	}

	close(lfd);
	return 0;
}
