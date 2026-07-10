#ifndef OSP_TCP_SOCKET_H
#define OSP_TCP_SOCKET_H

#include "../src/openspodes.h"
#include "../src/transport/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_TCP_DLMS_PORT       4059
#define OSP_TCP_CLIENT_WPORT    1000
#define OSP_TCP_MAX_PDU         4096

typedef struct {
	int fd;
	uint16_t wrapper_source;
	uint16_t wrapper_dest;
} osp_tcp_wrapper_ctx_t;

/* Bind osp_transport_t to a connected TCP socket with COSEM wrapper framing. */
void osp_tcp_wrapper_transport_init(osp_transport_t *t, osp_tcp_wrapper_ctx_t *ctx, int fd, uint16_t wrapper_source,
                                    uint16_t wrapper_dest);

#ifdef __cplusplus
}
#endif

#endif /* OSP_TCP_SOCKET_H */
