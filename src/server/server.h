/**
 * server.h — DLMS/COSEM server request dispatcher
 *
 * Accepts incoming requests, performs AARQ/AARE association,
 * routes GET/SET/ACTION to registered IC objects.
 */

#ifndef OSP_SERVER_H
#define OSP_SERVER_H

#include "../openspodes.h"
#include "../transport/transport.h"
#include "../service/service.h"
#include "../security/security.h"
#include "../server/dispatcher.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_SERVER_MAX_PDU 1024

typedef struct {
	osp_transport_t *transport;
	osp_framing_type_t framing;
	osp_sec_context_t security;
	osp_dispatcher_t dispatcher;
	bool associated;
	uint8_t invoke_id;

	/* Buffers */
	uint8_t rx_buf[OSP_SERVER_MAX_PDU];
	uint8_t tx_buf[OSP_SERVER_MAX_PDU];
} osp_server_t;

/* Initialize server */
osp_err_t osp_server_init(osp_server_t *s, osp_transport_t *transport, osp_framing_type_t framing);

/* Register an IC object */
osp_err_t osp_server_register(osp_server_t *s, const osp_ic_class_t *cls, void *instance);

/* Set security context (optional, before accept) */
void osp_server_set_security(osp_server_t *s, const osp_sec_context_t *sec);

/* Accept one incoming request (blocking). Returns:
 *   OSP_OK — request was handled (response sent)
 *   OSP_ERR_TIMEOUT — no data within timeout
 *   Other — error
 *
 * Internally:
 *   - If CF_IDLE: processes AARQ → sends AARE
 *   - If CF_ASSOCIATED: processes GET/SET/ACTION/RLRQ
 */
osp_err_t osp_server_accept(osp_server_t *s, uint32_t timeout_ms);

/* Convenience: run server until RLRQ/timeout */
osp_err_t osp_server_run(osp_server_t *s, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SERVER_H */
