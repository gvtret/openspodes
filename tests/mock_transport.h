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
#include "../src/server/server.h"

#define MOCK_BUF_SIZE 4096

typedef struct {
	uint8_t data[MOCK_BUF_SIZE];
	uint32_t len;
	uint32_t rpos;
	uint32_t msg_starts[64];
	uint32_t msg_count;
	uint32_t msg_index;
	uint32_t delay_ms; /* artificial delay before returning data (for timeout tests) */
} mock_buf_t;

typedef struct {
	osp_transport_t client_transport;
	osp_transport_t server_transport;
	mock_buf_t server_rx; /* client send → here */
	mock_buf_t client_rx; /* server send → here */
	bool gbt_ack_pump;
} mock_transport_pair_t;

void mock_transport_pair_init(mock_transport_pair_t *p);

/* Queue helpers (used by loopback transport) */
osp_err_t mock_send_to_peer(mock_buf_t *dst, const uint8_t *data, uint32_t len);
osp_err_t mock_recv_from_peer(mock_buf_t *src, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms);

/* Client send + optional server_accept; propagates either transport error */
osp_err_t mock_loopback_send(mock_transport_pair_t *pair, osp_server_t *server, const uint8_t *data, uint32_t len);

/* Auto-send GBT acks while server blocks waiting for client confirmation (loopback only) */
void mock_transport_enable_gbt_ack_pump(mock_transport_pair_t *p, bool enable);

/* Trace: dump raw hex of all queued messages in a buffer */
void mock_buf_trace_dump(const mock_buf_t *buf, const char *label);

/* Trace: dump all pending messages in the transport pair */
void mock_transport_trace_dump(const mock_transport_pair_t *p);

/* Delay: simulate real-time delays for timeout tests.
 * Set delay_ms on a buffer; recv_from_peer will sleep before returning.
 * Use mock_transport_set_recv_delay() to set delay on server_rx or client_rx. */
void mock_buf_set_delay(mock_buf_t *buf, uint32_t delay_ms);
void mock_transport_set_recv_delay(mock_transport_pair_t *p, bool server_rx_delay, uint32_t delay_ms);

#endif
