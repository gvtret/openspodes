/**
 * client.h — DLMS/COSEM client session driver
 *
 * Blocking request/response driver wrapping HAL transport + framing.
 * Performs AARQ→AARE→HLS handshake, then GET/SET/ACTION/release.
 *
 * Based on spodes-rs ClientSession architecture.
 */

#ifndef OSP_CLIENT_H
#define OSP_CLIENT_H

#include "../openspodes.h"
#include "../transport/transport.h"
#include "../transport/hdlc_session.h"
#include "../service/service.h"
#include "../service/initiate.h"
#include "../security/security.h"
#include "../service/notification.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_CLIENT_MAX_PDU 1024
#define OSP_CLIENT_REASSEMBLE_MAX (OSP_CLIENT_MAX_PDU * 4)
#define OSP_CLIENT_BLOCK_SIZE 64

typedef struct {
	osp_transport_t *transport;
	osp_framing_type_t framing;
	osp_sec_context_t security;
	bool associated;
	uint8_t invoke_id;
	bool gbt_enabled;
	uint32_t gbt_block_size;
	uint8_t gbt_window;
	bool gbt_streaming;
	bool ciphering_enabled;
	bool use_dedicated_key;
	uint8_t dedicated_key[OSP_INITIATE_DEDICATED_KEY_MAX];
	uint8_t dedicated_key_len;
	osp_sec_context_t cipher_tx;
	osp_sec_context_t cipher_rx;

	/* HDLC session (when framing == OSP_FRAMING_HDLC) */
	osp_hdlc_session_t hdlc;
	bool hdlc_active;

	/* Buffers */
	uint8_t tx_buf[OSP_CLIENT_MAX_PDU];
	uint8_t rx_buf[OSP_CLIENT_MAX_PDU];
} osp_client_t;

/* Initialize a client session */
osp_err_t osp_client_init(osp_client_t *c, osp_transport_t *transport, osp_framing_type_t framing);

/* Set security context (optional, before connect) */
void osp_client_set_security(osp_client_t *c, const osp_sec_context_t *sec);

/* Set HDLC addresses (optional, before connect, HDLC framing only) */
void osp_client_set_hdlc_addresses(osp_client_t *c, uint32_t client_addr, uint8_t client_addr_len,
                                    uint32_t server_addr, uint8_t server_addr_len);

/* Enable general block transfer for APDUs longer than block_size */
void osp_client_enable_gbt(osp_client_t *c, uint32_t block_size);

/* Set GBT window (0=unconfirmed, 1+=confirmed with ack between windows) */
void osp_client_set_gbt_window(osp_client_t *c, uint8_t window);

/* Set the STR bit on outbound GBT data blocks. */
void osp_client_set_gbt_streaming(osp_client_t *c, bool enabled);

/* Enable glo-ciphering (tx protects requests, rx unprotects responses) */
void osp_client_set_ciphering(osp_client_t *c, const osp_sec_context_t *tx, const osp_sec_context_t *rx);

/* Include dedicated encryption key in InitiateRequest (ded-ciphering after connect) */
void osp_client_set_dedicated_key(osp_client_t *c, const uint8_t *key, uint8_t key_len);

/* Connect: AARQ→AARE→HLS pass3/4. Returns 0 on success. */
osp_err_t osp_client_connect(osp_client_t *c, uint32_t timeout_ms);

/* Release: RLRQ→RLRE */
osp_err_t osp_client_release(osp_client_t *c);

typedef struct {
	uint16_t class_id;
	osp_obis_t instance_id;
	uint8_t attribute_id;
} osp_client_attr_ref_t;

typedef struct {
	uint16_t class_id;
	osp_obis_t instance_id;
	uint8_t method_id;
} osp_client_method_ref_t;

/* GET request/response */
osp_err_t osp_client_get(osp_client_t *c, uint16_t class_id, const osp_obis_t *obis, uint8_t attr_id, osp_value_t *result);

/* GET with selective access (ProfileGeneric buffer: date/entry range) */
osp_err_t osp_client_get_with_selective_access(osp_client_t *c, uint16_t class_id, const osp_obis_t *obis, uint8_t attr_id,
                                                 const osp_selective_access_t *sa, osp_value_t *result);

/* GET with-list (up to OSP_XDLMS_MAX_LIST attributes) */
osp_err_t osp_client_get_with_list(osp_client_t *c, const osp_client_attr_ref_t *attrs, uint8_t count, osp_get_result_item_t *results);

/* SET request/response */
osp_err_t osp_client_set(osp_client_t *c, uint16_t class_id, const osp_obis_t *obis, uint8_t attr_id, const osp_value_t *value);

/* SET with-list */
osp_err_t osp_client_set_with_list(osp_client_t *c, const osp_client_attr_ref_t *attrs, const osp_value_t *values, uint8_t count,
                                   osp_dar_t *results);

/* ACTION request/response */
osp_err_t osp_client_action(osp_client_t *c, uint16_t class_id, const osp_obis_t *obis, uint8_t method_id, const osp_value_t *param, osp_value_t *result);

/* ACTION with-list */
osp_err_t osp_client_action_with_list(osp_client_t *c, const osp_client_method_ref_t *methods, const osp_value_t *params, uint8_t count,
                                      osp_action_response_item_t *results);

/* Disconnect (transport close) */
osp_err_t osp_client_disconnect(osp_client_t *c);

/* Receive unsolicited data-notification APDU */
osp_err_t osp_client_recv_data_notification(osp_client_t *c, osp_data_notification_t *dn, uint32_t timeout_ms);

/* Receive unsolicited event-notification APDU (0xC2) */
osp_err_t osp_client_recv_event_notification(osp_client_t *c, osp_event_notification_t *ev, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OSP_CLIENT_H */
