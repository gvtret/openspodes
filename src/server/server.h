/**
 * server.h — DLMS/COSEM server request dispatcher
 *
 * Accepts incoming requests, performs AARQ/AARE association,
 * routes GET/SET/ACTION to registered IC objects.
 *
 * Memory configuration:
 * - OSP_SERVER_MAX_PDU (default 1024): rx/tx buffer size
 * - OSP_SERVER_PENDING_MAX (default 4096): block transfer reassembly
 * - OSP_MAX_OBJECTS (default 320): registered IC objects
 *
 * For constrained MCUs (< 32KB RAM), define before including:
 *   #define OSP_SERVER_MAX_PDU 512
 *   #define OSP_SERVER_PENDING_MAX 1024
 *   #define OSP_MAX_OBJECTS 16
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
/* Association LN object_list with ACLs is several KB even for 64 entries. */
#define OSP_SERVER_PENDING_MAX (OSP_SERVER_MAX_PDU * 32)

typedef enum {
	OSP_ACSE_LOG_AARE = 0,
	OSP_ACSE_LOG_HLS_PASS3 = 1,
} osp_acse_log_kind_t;

typedef enum {
	OSP_ACSE_USER_INFO_NONE = 0,
	OSP_ACSE_USER_INFO_INITIATE_RESPONSE = 1,
	OSP_ACSE_USER_INFO_INITIATE_ERROR = 2,
} osp_acse_user_info_kind_t;

typedef struct {
	osp_acse_log_kind_t kind;
	uint8_t client_sap;
	uint8_t aarq_application_context;
	uint8_t aarq_mechanism;
	bool aarq_has_mechanism;
	bool aarq_has_calling_auth;
	bool aarq_ireq_ok;
	uint8_t aarq_dlms_version;
	uint32_t aarq_conformance;
	uint16_t aarq_client_pdu;
	uint8_t aare_result;
	uint8_t aare_diagnostic;
	uint8_t aare_diag_is_provider;
	uint8_t aare_initiate_error;
	bool aare_hls_pending;
	uint8_t aare_application_context;
	osp_acse_user_info_kind_t aare_user_info;
	bool hls_pass3_ok;
} osp_acse_log_event_t;

typedef void (*osp_server_acse_log_fn)(void *ctx, const osp_acse_log_event_t *ev);

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
	bool defer_flush; /* HDLC: send DN on the next service response after ACTION */
	osp_data_notification_t notification;
} osp_server_pending_push_t;

typedef struct {
	osp_transport_t *transport;
	osp_framing_type_t framing;
	osp_sec_context_t security;
	osp_dispatcher_t dispatcher;
	bool associated;
	bool hls_pending; /* AARE ok; waiting for HLS pass 3 before full AA */
	uint8_t invoke_id;
	uint32_t long_invoke_id; /* 24-bit Data-Notification invoke counter */
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

	/* Optional ACSE/HLS logging (spodes-server sets via osp_server_set_acse_log_cb). */
	osp_server_acse_log_fn acse_log_cb;
	void *acse_log_ctx;

	/* Buffers */
	uint8_t rx_buf[OSP_SERVER_MAX_PDU];
	uint8_t tx_buf[OSP_SERVER_MAX_PDU];
} osp_server_t;

/** @brief Set max PDU size for block transfer segmentation (default: full buffer). */
void osp_server_set_max_pdu(osp_server_t *s, uint32_t max_pdu);

/** @brief Enable general block transfer for APDUs longer than block_size (default payload 56 B). */
void osp_server_enable_gbt(osp_server_t *s, uint32_t block_size);

/** @brief Set GBT window (0=unconfirmed, 1+=confirmed with ack between windows). */
void osp_server_set_gbt_window(osp_server_t *s, uint8_t window);

/** @brief Set the STR bit on outbound GBT data blocks. */
void osp_server_set_gbt_streaming(osp_server_t *s, bool enabled);

/** @brief Enable glo-ciphering (rx unprotects requests, tx protects responses). */
void osp_server_set_ciphering(osp_server_t *s, const osp_sec_context_t *tx, const osp_sec_context_t *rx);

/** @brief Disable APDU ciphering (e.g. after AA release). */
void osp_server_clear_ciphering(osp_server_t *s);

/** @brief Register callback for AARQ/AARE and HLS pass-3 association logging. */
void osp_server_set_acse_log_cb(osp_server_t *s, osp_server_acse_log_fn fn, void *ctx);

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
 * @brief Queue in-band Data-Notification for the active HDLC association.
 * Flushed after the ACTION response (deferred) or immediately on non-HDLC links.
 */
osp_err_t osp_server_queue_pending_push(osp_server_t *s, const osp_value_t *body, bool confirmed);

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

/** @brief Set security context (optional, before accept). */
void osp_server_set_security(osp_server_t *s, const osp_sec_context_t *sec);

/** @brief Set HDLC addresses (optional, before accept, HDLC framing only). */
void osp_server_set_hdlc_addresses(osp_server_t *s, uint32_t server_addr, uint8_t server_addr_len,
                                    uint32_t client_addr, uint8_t client_addr_len);

/** @brief Bind Association LN for ACL enforcement on GET/SET/ACTION. */
void osp_server_set_association(osp_server_t *s, osp_ic_association_ln_t *association);

/**
 * @brief Clear current association (0.0.40.0.0.255) to idle and drop AA flag.
 * Call on RLRQ/DISC/transport drop (also invoked internally).
 */
void osp_server_clear_current_association(osp_server_t *s);

/**
 * @brief Get the actual client SAP after SNRM/UA exchange.
 *
 * Returns the client address value from the SNRM source field.
 * For HDLC: 1-byte address (e.g. 0x10=public, 0x20=reader, 0x30=configurator).
 * For WRAPPER: returns 0 (not applicable).
 *
 * @param s Server context (must have completed SNRM/UA via osp_server_accept).
 * @return Client SAP value, or 0 if not available.
 */
uint8_t osp_server_get_client_sap(osp_server_t *s);

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

/** @brief Convenience: run server until RLRQ/timeout. */
osp_err_t osp_server_run(osp_server_t *s, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SERVER_H */
