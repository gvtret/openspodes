/**
 * initiate.h — xDLMS InitiateRequest / InitiateResponse (IEC 62056-5-3 §11.2)
 */

#ifndef OSP_INITIATE_H
#define OSP_INITIATE_H

#include "../openspodes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_INITIATE_REQUEST_TAG  0x01
#define OSP_INITIATE_RESPONSE_TAG 0x08

#define OSP_DLMS_VERSION_NUMBER       6
#define OSP_INITIATE_MIN_CLIENT_PDU   12

/* Confirmed-service-error initiate category values (IEC 62056-5-3) */
#define OSP_INITIATE_ERR_DLMS_VERSION_TOO_LOW   1
#define OSP_INITIATE_ERR_INCOMPATIBLE_CONFORMANCE 2
#define OSP_INITIATE_ERR_PDU_SIZE_TOO_SHORT     3

#define OSP_INITIATE_DEDICATED_KEY_MAX 32

typedef struct {
	uint8_t dedicated_key[OSP_INITIATE_DEDICATED_KEY_MAX];
	uint8_t dedicated_key_len;
	bool has_dedicated_key;
	bool response_allowed;
	bool has_qos;
	int8_t proposed_quality_of_service;
	uint8_t proposed_dlms_version;
	uint32_t proposed_conformance;
	uint16_t client_max_receive_pdu_size;
} osp_initiate_request_t;

typedef struct {
	bool has_qos;
	int8_t negotiated_quality_of_service;
	uint8_t negotiated_dlms_version;
	uint32_t negotiated_conformance;
	uint16_t server_max_receive_pdu_size;
	uint16_t vaa_name;
} osp_initiate_response_t;

void osp_initiate_request_default(osp_initiate_request_t *req);
void osp_initiate_response_default(osp_initiate_response_t *resp);

osp_err_t osp_initiate_request_encode(const osp_initiate_request_t *req, osp_buf_t *buf);
osp_err_t osp_initiate_request_decode(osp_buf_t *buf, osp_initiate_request_t *req);

osp_err_t osp_initiate_response_encode(const osp_initiate_response_t *resp, osp_buf_t *buf);
osp_err_t osp_initiate_response_decode(osp_buf_t *buf, osp_initiate_response_t *resp);

/** Encode Initiate user-information reject blob (ConfirmedServiceError). */
osp_err_t osp_initiate_error_encode(osp_buf_t *buf, uint8_t initiate_error_value);

#ifdef __cplusplus
}
#endif

#endif /* OSP_INITIATE_H */
