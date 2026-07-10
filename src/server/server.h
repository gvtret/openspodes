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
#include "../service/notification.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_SERVER_MAX_PDU 1024
#define OSP_SERVER_PENDING_MAX (OSP_SERVER_MAX_PDU * 4)

typedef struct {
	bool active;
	uint8_t data[OSP_SERVER_PENDING_MAX];
	uint32_t data_len;
	uint32_t next_block;
	osp_attribute_descriptor_t set_attr;
	uint8_t set_buffer[OSP_SERVER_PENDING_MAX];
	uint32_t set_buffer_len;
} osp_server_pending_t;

typedef struct {
	bool active;
	osp_method_descriptor_t method;
	uint8_t buffer[OSP_SERVER_PENDING_MAX];
	uint32_t buffer_len;
} osp_server_pending_action_in_t;

typedef struct {
	bool active;
	uint8_t invoke_id_priority;
	osp_dar_t result;
	uint8_t data[OSP_SERVER_PENDING_MAX];
	uint32_t data_len;
	uint32_t next_block;
} osp_server_pending_action_out_t;

typedef struct {
	bool pending;
	osp_data_notification_t notification;
} osp_server_pending_push_t;

typedef struct {
	osp_transport_t *transport;
	osp_framing_type_t framing;
	osp_sec_context_t security;
	osp_dispatcher_t dispatcher;
	bool associated;
	uint8_t invoke_id;
	uint32_t max_pdu;
	bool gbt_enabled;
	uint32_t gbt_block_size;
	uint8_t gbt_window;
	bool gbt_streaming;
	bool ciphering_enabled;
	osp_sec_context_t cipher_tx;
	osp_sec_context_t cipher_rx;

	osp_server_pending_t pending_get;
	osp_server_pending_t pending_set;
	osp_server_pending_action_in_t pending_action_in;
	osp_server_pending_action_out_t pending_action_out;
	osp_server_pending_push_t pending_push;

	/* Buffers */
	uint8_t rx_buf[OSP_SERVER_MAX_PDU];
	uint8_t tx_buf[OSP_SERVER_MAX_PDU];
} osp_server_t;

/* Set max PDU size for block transfer segmentation (default: full buffer) */
void osp_server_set_max_pdu(osp_server_t *s, uint32_t max_pdu);

/* Enable general block transfer for APDUs longer than block_size (default payload 56 B) */
void osp_server_enable_gbt(osp_server_t *s, uint32_t block_size);

/* Set GBT window (0=unconfirmed, 1+=confirmed with ack between windows) */
void osp_server_set_gbt_window(osp_server_t *s, uint8_t window);

/* Set the STR bit on outbound GBT data blocks. */
void osp_server_set_gbt_streaming(osp_server_t *s, bool enabled);

/* Enable glo-ciphering (rx unprotects requests, tx protects responses) */
void osp_server_set_ciphering(osp_server_t *s, const osp_sec_context_t *tx, const osp_sec_context_t *rx);

/* Send unsolicited event-notification (0xC2) to the associated client */
osp_err_t osp_server_send_event_notification(osp_server_t *s, const osp_event_notification_t *ev);

/* Initialize server */
osp_err_t osp_server_init(osp_server_t *s, osp_transport_t *transport, osp_framing_type_t framing);

/* Register an IC object */
osp_err_t osp_server_register(osp_server_t *s, const osp_ic_class_t *cls, void *instance);

/* Set security context (optional, before accept) */
void osp_server_set_security(osp_server_t *s, const osp_sec_context_t *sec);

/* Bind Association LN for ACL enforcement on GET/SET/ACTION */
void osp_server_set_association(osp_server_t *s, osp_ic_association_ln_t *association);

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
