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
#include "../transport/hdlc_session.h"
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

	/* HDLC session (when framing == OSP_FRAMING_HDLC) */
	osp_hdlc_session_t hdlc;
	bool hdlc_active;

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

/**
 * @brief Send unsolicited event-notification (0xC2) to the associated client.
 * @param s  Associated server context.
 * @param ev Event notification to send.
 * @return 0 on success, negative on failure.
 */
osp_err_t osp_server_send_event_notification(osp_server_t *s, const osp_event_notification_t *ev);

/**
 * @brief Send unsolicited data-notification (0x0F) to the associated client.
 * @param s  Associated server context.
 * @param dn Data notification to send.
 * @return 0 on success, negative on failure.
 */
osp_err_t osp_server_send_data_notification(osp_server_t *s, const osp_data_notification_t *dn);

/**
 * @brief Initialize a server context.
 * @param s         Server context to initialize (caller-owned, should be static).
 * @param transport Transport HAL interface for I/O.
 * @param framing   Framing type: OSP_FRAMING_HDLC or OSP_FRAMING_WRAPPER.
 * @return OSP_OK on success, OSP_ERR_INVALID on bad arguments.
 */
osp_err_t osp_server_init(osp_server_t *s, osp_transport_t *transport, osp_framing_type_t framing);

/**
 * @brief Register a COSEM Interface Class with the server dispatcher.
 * @param s        Server context (must be initialized).
 * @param cls      IC class vtable (get_attr, set_attr, invoke, serialize, deserialize).
 * @param instance Pointer to the IC instance data (caller-owned, static storage recommended).
 * @return OSP_OK on success, OSP_ERR_NOMEM if dispatcher is full.
 */
osp_err_t osp_server_register(osp_server_t *s, const osp_ic_class_t *cls, void *instance);

/* Set security context (optional, before accept) */
void osp_server_set_security(osp_server_t *s, const osp_sec_context_t *sec);

/* Set HDLC addresses (optional, before accept, HDLC framing only) */
void osp_server_set_hdlc_addresses(osp_server_t *s, uint32_t server_addr, uint8_t server_addr_len,
                                    uint32_t client_addr, uint8_t client_addr_len);

/* Bind Association LN for ACL enforcement on GET/SET/ACTION */
void osp_server_set_association(osp_server_t *s, osp_ic_association_ln_t *association);

/**
 * @brief Accept and process one incoming request (blocking).
 *
 * Dispatches based on current association state:
 *   - CF_IDLE: processes AARQ -> sends AARE, performs HLS
 *   - CF_ASSOCIATED: processes GET/SET/ACTION/RLRQ
 *
 * @param s          Server context (must be initialized, optionally registered).
 * @param timeout_ms Maximum time to wait for incoming data (ms).
 * @return OSP_OK if request handled and response sent,
 *         OSP_ERR_TIMEOUT if no data within timeout,
 *         negative on transport/protocol error.
 */
osp_err_t osp_server_accept(osp_server_t *s, uint32_t timeout_ms);

/* Convenience: run server until RLRQ/timeout */
osp_err_t osp_server_run(osp_server_t *s, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SERVER_H */
