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
#include "../service/service.h"
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

	/* Buffers */
	uint8_t tx_buf[OSP_CLIENT_MAX_PDU];
	uint8_t rx_buf[OSP_CLIENT_MAX_PDU];
} osp_client_t;

/* Initialize a client session */
osp_err_t osp_client_init(osp_client_t *c, osp_transport_t *transport, osp_framing_type_t framing);

/* Set security context (optional, before connect) */
void osp_client_set_security(osp_client_t *c, const osp_sec_context_t *sec);

/* Connect: AARQ→AARE→HLS pass3/4. Returns 0 on success. */
osp_err_t osp_client_connect(osp_client_t *c, uint32_t timeout_ms);

/* Release: RLRQ→RLRE */
osp_err_t osp_client_release(osp_client_t *c);

/* GET request/response */
osp_err_t osp_client_get(osp_client_t *c, uint16_t class_id, const osp_obis_t *obis, uint8_t attr_id, osp_value_t *result);

/* SET request/response */
osp_err_t osp_client_set(osp_client_t *c, uint16_t class_id, const osp_obis_t *obis, uint8_t attr_id, const osp_value_t *value);

/* ACTION request/response */
osp_err_t osp_client_action(osp_client_t *c, uint16_t class_id, const osp_obis_t *obis, uint8_t method_id, const osp_value_t *param, osp_value_t *result);

/* Disconnect (transport close) */
osp_err_t osp_client_disconnect(osp_client_t *c);

/* Receive unsolicited data-notification APDU */
osp_err_t osp_client_recv_data_notification(osp_client_t *c, osp_data_notification_t *dn, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OSP_CLIENT_H */
