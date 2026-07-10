/**
 * mock_transport.h — In-memory transport pair for integration tests.
 *
 * Creates two osp_transport_t: one for client, one for server.
 * Client send() → data goes to server_recv_buf
 * Server send() → data goes to client_recv_buf
 */
#ifndef OSP_MOCK_TRANSPORT_H
#define OSP_MOCK_TRANSPORT_H

#include "../src/openspodes.h"
#include "../src/transport/transport.h"

#define MOCK_BUF_SIZE 4096

typedef struct {
	uint8_t data[MOCK_BUF_SIZE];
	uint32_t len;
	uint32_t rpos;
} mock_buf_t;

typedef struct {
	osp_transport_t client_transport;
	osp_transport_t server_transport;
	mock_buf_t server_rx; /* client send → here */
	mock_buf_t client_rx; /* server send → here */
} mock_transport_pair_t;

void mock_transport_pair_init(mock_transport_pair_t *p);

/* Queue helpers (used by loopback transport) */
osp_err_t mock_send_to_peer(mock_buf_t *dst, const uint8_t *data, uint32_t len);
osp_err_t mock_recv_from_peer(mock_buf_t *src, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms);

#endif
