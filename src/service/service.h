/**
 * service.h — xDLMS service layer (IEC 62056-5-3)
 *
 * Service APDUs for LN referencing: GET, SET, ACTION, ACSE (AARQ/AARE),
 * Exception Response, Confirmed Service Error.
 *
 * Common building blocks: invoke-id-and-priority, attribute/method descriptors,
 * data-access-result codes, access selection.
 */

#ifndef OSP_SERVICE_H
#define OSP_SERVICE_H

#include "../openspodes.h"
#include "../codec/types.h"
#include "../codec/structures.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 *  APDU TAGS (IEC 62056-5-3 Table 60)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define OSP_TAG_DATA_NOTIFICATION       0x0F
#define OSP_TAG_GET_REQUEST             0xC0
#define OSP_TAG_SET_REQUEST             0xC1
#define OSP_TAG_EVENT_NOTIFICATION_REQ  0xC2
#define OSP_TAG_ACTION_REQUEST          0xC3
#define OSP_TAG_GET_RESPONSE            0xC4
#define OSP_TAG_SET_RESPONSE            0xC5
#define OSP_TAG_ACTION_RESPONSE         0xC7
#define OSP_TAG_EXCEPTION_RESPONSE      0xD8
#define OSP_TAG_CONFIRMED_SERVICE_ERROR 0xDC

/* ═══════════════════════════════════════════════════════════════════════════
 *  COMMON TYPES
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Invoke ID and Priority (1 byte: bits 7-5 unused, bit 4=PI, bits 3-0=invoke_id) */
#define OSP_IIDP(invoke, priority) ((uint8_t)(((priority) << 4) | ((invoke) & 0x0F)))
#define OSP_IIDP_INVOKE(iidp)      ((iidp) & 0x0F)
#define OSP_IIDP_PRIORITY(iidp)    (((iidp) >> 4) & 0x01)

/* Data Access Result (IEC 62056-5-3 Table 31) */
typedef enum {
	OSP_DAR_SUCCESS = 0,
	OSP_DAR_HARDWARE_FAULT = 1,
	OSP_DAR_TEMPORARY_FAILURE = 2,
	OSP_DAR_READ_DENIED = 3,
	OSP_DAR_OBJECT_UNDEFINED = 4,
	OSP_DAR_OBJECT_CLASS_UNKNOWN = 5,
	OSP_DAR_OBJECT_UNAVAILABLE = 6,
	OSP_DAR_TYPE_MISMATCH = 7,
	OSP_DAR_SCOPE_OF_ACCESS = 8,
	OSP_DAR_DATA_BLOCK_UNAVAILABLE = 9,
	OSP_DAR_LONG_GET_ABORTED = 10,
	OSP_DAR_NO_LONG_GET_IN_PROGRESS = 11,
	OSP_DAR_LONG_BLOCK_TRANSFER = 12,
	OSP_DAR_UNEXPECTED_ATTRIBUTE = 13,
	OSP_DAR_AUTHORIZATION_FAILURE = 14,
	OSP_DAR_HARDWARE_FAULT2 = 15,
} osp_dar_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  ACSE (IEC 62056-5-3 section 7)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define OSP_ACSE_AARQ_TAG 0x60 /* APPLICATION 0 constructed */
#define OSP_ACSE_AARE_TAG 0x61 /* APPLICATION 1 constructed */
#define OSP_ACSE_RLRQ_TAG 0x62 /* APPLICATION 2 constructed */
#define OSP_ACSE_RLRE_TAG 0x63 /* APPLICATION 3 constructed */

/* Application context identifiers */
#define OSP_CTX_LN           1
#define OSP_CTX_SN           2
#define OSP_CTX_LN_CIPHERING 3
#define OSP_CTX_SN_CIPHERING 4

/* Authentication mechanism identifiers */

/* Association results */
#define OSP_RESULT_ACCEPTED           0
#define OSP_RESULT_REJECTED_PERMANENT 1
#define OSP_RESULT_REJECTED_TRANSIENT 2

/* AARQ encode/decode */
typedef struct {
	uint8_t application_context; /* OSP_CTX_* */
	uint8_t mechanism;           /* OSP_MECH_* */
	uint8_t calling_ap_title[8]; /* system title (8 bytes) */
	uint8_t calling_ap_title_len;
	uint8_t calling_auth_value[64]; /* CtoS challenge */
	uint8_t calling_auth_value_len;
	uint8_t user_info[128]; /* xDLMS InitiateRequest */
	uint32_t user_info_len;
	osp_xdms_context_t xdms; /* conformance, PDU sizes */
} osp_aarq_t;

typedef struct {
	osp_aarq_t *req;
	osp_buf_t buf;
} osp_aarq_encoder_t;

int osp_aarq_encode(osp_aarq_t *aarq, osp_buf_t *buf);
int osp_aarq_decode(osp_buf_t *buf, osp_aarq_t *aarq);

/* AARE encode/decode */
typedef struct {
	uint8_t result; /* OSP_RESULT_* */
	uint8_t result_source_diagnostic;
	uint8_t mechanism;              /* OSP_MECH_* */
	uint8_t responding_ap_title[8]; /* system title */
	uint8_t responding_ap_title_len;
	uint8_t responding_auth_value[64]; /* StoC challenge */
	uint8_t responding_auth_value_len;
	uint8_t user_info[128]; /* xDLMS InitiateResponse */
	uint32_t user_info_len;
	osp_xdms_context_t xdms;
} osp_aare_t;

int osp_aare_encode(osp_aare_t *aare, osp_buf_t *buf);
int osp_aare_decode(osp_buf_t *buf, osp_aare_t *aare);

/* RLRQ/RLRE */
typedef struct {
	uint8_t reason;
} osp_rlrq_t;

int osp_rlrq_encode(osp_rlrq_t *rlrq, osp_buf_t *buf);
int osp_rlre_encode(osp_rlrq_t *rlre, osp_buf_t *buf);
int osp_rlrq_decode(osp_buf_t *buf, osp_rlrq_t *rlrq);
int osp_rlre_decode(osp_buf_t *buf, osp_rlrq_t *rlre);

/* ═══════════════════════════════════════════════════════════════════════════
 *  GET (IEC 62056-5-3 Table 105)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define OSP_XDLMS_MAX_LIST 8

typedef enum {
	OSP_GET_NORMAL = 1,
	OSP_GET_WITH_BLOCK = 2,
	OSP_GET_WITH_LIST = 3,
	OSP_GET_WITH_LIST_BLOCK = 4,
} osp_get_request_type;

typedef struct {
	osp_attribute_descriptor_t attr;
	uint8_t has_access_selection;
} osp_get_list_item_t;

typedef struct {
	osp_get_list_item_t items[OSP_XDLMS_MAX_LIST];
	uint8_t count;
} osp_get_request_with_list_t;

typedef struct {
	osp_attribute_descriptor_t attr;
	osp_selective_access_t access_selection;
} osp_get_request_normal_t;

typedef struct {
	uint8_t invoke_id_priority;
	uint32_t block_number;
} osp_get_request_next_t;

typedef struct {
	osp_get_request_type type;
	uint8_t invoke_id_priority; /* top-level: same for all variants */

	union {
		osp_get_request_normal_t normal;
		osp_get_request_next_t next;
		osp_get_request_with_list_t with_list;
	} as;
} osp_get_request_t;

int osp_get_request_encode(osp_buf_t *buf, const osp_get_request_t *req);
int osp_get_request_decode(osp_buf_t *buf, osp_get_request_t *req);

/* GET response */
typedef enum {
	OSP_GET_RESP_DATA = 1,
	OSP_GET_RESP_DATA_ERROR = 2,
	OSP_GET_RESP_BLOCK = 3,
	OSP_GET_RESP_BLOCK_LAST = 4,
	OSP_GET_RESP_WITH_LIST = 5,
} osp_get_response_type;

typedef struct {
	uint8_t is_data;
	osp_value_t data;
	osp_dar_t access_result;
} osp_get_result_item_t;

typedef struct {
	osp_get_result_item_t items[OSP_XDLMS_MAX_LIST];
	uint8_t count;
} osp_get_response_with_list_t;

typedef struct {
	osp_get_response_type type;
	uint8_t invoke_id_priority;
	osp_value_t data;
	osp_dar_t data_access_result;
	osp_data_block_t data_block;
	osp_get_response_with_list_t with_list;
} osp_get_response_t;

int osp_get_response_encode(osp_buf_t *buf, const osp_get_response_t *resp);
int osp_get_response_decode(osp_buf_t *buf, osp_get_response_t *resp);

/* ═══════════════════════════════════════════════════════════════════════════
 *  SET (IEC 62056-5-3 Table 107)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
	OSP_SET_NORMAL = 1,
	OSP_SET_WITH_FIRST_DATABLOCK = 2,
	OSP_SET_WITH_DATABLOCK = 3,
	OSP_SET_WITH_LIST = 4,
} osp_set_request_type;

typedef enum {
	OSP_SET_RESP_NORMAL = 1,
	OSP_SET_RESP_DATABLOCK = 2,
	OSP_SET_RESP_LAST_DATABLOCK = 3,
	OSP_SET_RESP_WITH_LIST = 5,
} osp_set_response_type;

typedef struct {
	osp_attribute_descriptor_t attr;
	osp_selective_access_t access_selection;
	osp_value_t data;
} osp_set_request_item_t;

typedef struct {
	uint8_t invoke_id_priority;
	osp_set_request_item_t items[8];
	uint8_t item_count;
} osp_set_request_normal_t;

typedef struct {
	uint8_t invoke_id_priority;
	osp_attribute_descriptor_t attr;
	osp_selective_access_t access_selection;
	osp_data_block_t datablock;
} osp_set_request_first_datablock_t;

typedef struct {
	uint8_t invoke_id_priority;
	osp_data_block_t datablock;
} osp_set_request_datablock_t;

typedef struct {
	osp_set_request_item_t items[OSP_XDLMS_MAX_LIST];
	uint8_t count;
} osp_set_request_with_list_t;

typedef struct {
	osp_set_request_type type;
	uint8_t invoke_id_priority;

	union {
		osp_set_request_normal_t normal;
		osp_set_request_first_datablock_t first_datablock;
		osp_set_request_datablock_t datablock;
		osp_set_request_with_list_t with_list;
	} as;
} osp_set_request_t;

int osp_set_request_encode(osp_buf_t *buf, const osp_set_request_t *req);
int osp_set_request_decode(osp_buf_t *buf, osp_set_request_t *req);

/* SET response */
typedef struct {
	uint8_t invoke_id_priority;
	osp_dar_t result;
} osp_set_response_normal_t;

typedef struct {
	osp_dar_t results[OSP_XDLMS_MAX_LIST];
	uint8_t count;
} osp_set_response_with_list_t;

typedef struct {
	osp_dar_t result;
	uint32_t block_number;
} osp_set_response_last_datablock_t;

typedef struct {
	osp_set_response_type type;
	uint8_t invoke_id_priority;

	union {
		osp_set_response_normal_t normal;
		osp_data_block_t datablock;
		osp_set_response_last_datablock_t last_datablock;
		osp_set_response_with_list_t with_list;
	} as;
} osp_set_response_t;

int osp_set_response_encode(osp_buf_t *buf, const osp_set_response_t *resp);
int osp_set_response_decode(osp_buf_t *buf, osp_set_response_t *resp);

/* ═══════════════════════════════════════════════════════════════════════════
 *  ACTION (IEC 62056-5-3 Table 109)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
	osp_method_descriptor_t method;
	osp_value_t data; /* null-data or method parameters */
} osp_action_request_item_t;

typedef struct {
	osp_method_descriptor_t method;
	osp_dar_t result;
	osp_value_t return_data;
} osp_action_response_item_t;

typedef struct {
	uint8_t invoke_id_priority;
	osp_action_request_item_t items[8];
	uint8_t item_count;
} osp_action_request_normal_t;

typedef enum {
	OSP_ACTION_NORMAL = 1,
	OSP_ACTION_NEXT_PARAM_BLOCK = 2,
	OSP_ACTION_WITH_LIST = 3,
	OSP_ACTION_WITH_FIRST_PARAM_BLOCK = 4,
	OSP_ACTION_WITH_PARAM_BLOCK = 6,
} osp_action_request_type;

typedef enum {
	OSP_ACTION_RESP_NORMAL = 1,
	OSP_ACTION_RESP_WITH_PARAM_BLOCK = 2,
	OSP_ACTION_RESP_WITH_LIST = 3,
	OSP_ACTION_RESP_NEXT_PARAM_BLOCK = 4,
} osp_action_response_type;

typedef struct {
	osp_method_descriptor_t method;
	osp_data_block_t param_block;
} osp_action_request_first_param_block_t;

typedef struct {
	osp_data_block_t param_block;
} osp_action_request_param_block_t;

typedef struct {
	uint32_t block_number;
} osp_action_request_next_param_block_t;

typedef struct {
	osp_data_block_t param_block;
} osp_action_response_param_block_t;

typedef struct {
	uint32_t block_number;
} osp_action_response_next_param_block_t;

typedef struct {
	osp_action_request_item_t items[OSP_XDLMS_MAX_LIST];
	uint8_t count;
} osp_action_request_with_list_t;

typedef struct {
	osp_action_response_item_t items[OSP_XDLMS_MAX_LIST];
	uint8_t count;
} osp_action_response_with_list_t;

typedef struct {
	osp_action_request_type type;
	uint8_t invoke_id_priority;

	union {
		osp_action_request_normal_t normal;
		osp_action_request_with_list_t with_list;
		osp_action_request_first_param_block_t first_param_block;
		osp_action_request_param_block_t with_param_block;
		osp_action_request_next_param_block_t next_param_block;
	} as;
} osp_action_request_t;

int osp_action_request_encode(osp_buf_t *buf, const osp_action_request_t *req);
int osp_action_request_decode(osp_buf_t *buf, osp_action_request_t *req);

typedef struct {
	osp_action_response_item_t items[8];
	uint8_t item_count;
	uint8_t invoke_id_priority;
} osp_action_response_normal_t;

typedef struct {
	osp_action_response_type type;
	uint8_t invoke_id_priority;

	union {
		osp_action_response_normal_t normal;
		osp_action_response_with_list_t with_list;
		osp_action_response_param_block_t with_param_block;
		osp_action_response_next_param_block_t next_param_block;
	} as;
} osp_action_response_t;

int osp_action_response_encode(osp_buf_t *buf, const osp_action_response_t *resp);
int osp_action_response_decode(osp_buf_t *buf, osp_action_response_t *resp);

/* DataBlock-SA helper (SET/ACTION block transfer) */
int osp_data_block_sa_encode(osp_buf_t *buf, const osp_data_block_t *block);
int osp_data_block_sa_decode(osp_buf_t *buf, osp_data_block_t *block);

/* ═══════════════════════════════════════════════════════════════════════════
 *  EXCEPTION RESPONSE (IEC 62056-5-3 Table 67)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
	uint8_t invoke_id_priority;
	uint8_t error_code;    /* service error code */
	uint8_t service_error; /* confirmed-service-error code */
} osp_exception_response_t;

int osp_exception_response_encode(osp_buf_t *buf, const osp_exception_response_t *resp);
int osp_exception_response_decode(osp_buf_t *buf, osp_exception_response_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SERVICE_H */
