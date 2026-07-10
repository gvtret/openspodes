/* xdlms.h -- streaming (allocation-free) codec for xDLMS LN service PDUs,
 * layered on top of the A-XDR primitives in dlms_codec.h.
 *
 * Symmetric: the same primitives serve client (request encode / response
 * decode) and server (request decode / response encode) sides. No PDU is
 * materialised into a struct tree; fixed header fields go through small flat
 * structs, and every `Data` / `SEQUENCE OF` payload is left to the caller to
 * stream with axdr_encode_data / axdr_decode_data / dlms_read_len.
 *
 * Encoder contract for the *_begin functions: on return the writer sits
 * exactly where the next payload element must be appended. The doc comment on
 * each function states what (if anything) the caller must append next.
 *
 * C11.
 */
#ifndef XDLMS_H
#define XDLMS_H

#include "dlms_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------ APDU / variant tags */
enum {
    APDU_GET_REQUEST        = 0xC0,
    APDU_SET_REQUEST        = 0xC1,
    APDU_EVENT_NOTIFICATION = 0xC2,
    APDU_ACTION_REQUEST     = 0xC3,
    APDU_GET_RESPONSE       = 0xC4,
    APDU_SET_RESPONSE       = 0xC5,
    APDU_ACTION_RESPONSE    = 0xC7,
    APDU_DATA_NOTIFICATION  = 0x0F,
    APDU_EXCEPTION_RESPONSE = 0xD8
};

/* Variant tags (shared value space per service; interpret with the APDU tag) */
enum {
    GET_REQUEST_NORMAL = 1, GET_REQUEST_NEXT = 2, GET_REQUEST_WITH_LIST = 3,
    GET_RESPONSE_NORMAL = 1, GET_RESPONSE_WITH_DATABLOCK = 2, GET_RESPONSE_WITH_LIST = 3,
    SET_REQUEST_NORMAL = 1,
    SET_RESPONSE_NORMAL = 1,
    ACTION_REQUEST_NORMAL = 1,
    ACTION_RESPONSE_NORMAL = 1
};

/* Get-Data-Result CHOICE tags */
enum { GDR_DATA = 0, GDR_ACCESS_RESULT = 1 };

/* Data-Access-Result (subset used everywhere; full list in the Green Book) */
typedef enum {
    DAR_SUCCESS = 0, DAR_HARDWARE_FAULT = 1, DAR_TEMPORARY_FAILURE = 2,
    DAR_READ_WRITE_DENIED = 3, DAR_OBJECT_UNDEFINED = 4,
    DAR_OBJECT_CLASS_INCONSISTENT = 9, DAR_OBJECT_UNAVAILABLE = 11,
    DAR_TYPE_UNMATCHED = 12, DAR_SCOPE_OF_ACCESS_VIOLATED = 13,
    DAR_DATA_BLOCK_UNAVAILABLE = 14, DAR_LONG_GET_ABORTED = 15,
    DAR_NO_LONG_GET_IN_PROGRESS = 16, DAR_OTHER_REASON = 250
} dlms_dar;

typedef enum {
    ACTR_SUCCESS = 0, ACTR_HARDWARE_FAULT = 1, ACTR_TEMPORARY_FAILURE = 2,
    ACTR_READ_WRITE_DENIED = 3, ACTR_OBJECT_UNDEFINED = 4,
    ACTR_OBJECT_UNAVAILABLE = 11, ACTR_TYPE_UNMATCHED = 12,
    ACTR_SCOPE_OF_ACCESS_VIOLATED = 13, ACTR_OTHER_REASON = 250
} dlms_action_result;

/* ------------------------------------------------------------- descriptors */
typedef struct {
    uint16_t class_id;
    uint8_t  instance_id[6];  /* OBIS, fixed 6 octets, no length on the wire */
    int8_t   attribute_id;
} cosem_attr_desc;

typedef struct {
    uint16_t class_id;
    uint8_t  instance_id[6];
    int8_t   method_id;
} cosem_method_desc;

/* Invoke-Id-And-Priority (Unsigned8) bit fields. */
static inline uint8_t dlms_iap_make(uint8_t invoke_id, bool confirmed, bool high_prio) {
    return (uint8_t)((invoke_id & 0x0F) | (confirmed ? 0x40 : 0) | (high_prio ? 0x80 : 0));
}
static inline uint8_t dlms_iap_invoke_id(uint8_t iap) { return iap & 0x0F; }
static inline bool    dlms_iap_confirmed(uint8_t iap) { return (iap & 0x40) != 0; }
static inline bool    dlms_iap_high_prio(uint8_t iap) { return (iap & 0x80) != 0; }

/* =====================================================================
 *  A-XDR primitives (public; usable to hand-build any PDU not wrapped below)
 * ===================================================================== */
dlms_status axdr_put_u8 (dlms_writer *w, uint8_t v);
dlms_status axdr_put_i8 (dlms_writer *w, int8_t  v);
dlms_status axdr_put_u16(dlms_writer *w, uint16_t v);
dlms_status axdr_put_u32(dlms_writer *w, uint32_t v);
dlms_status axdr_put_bool(dlms_writer *w, bool v);
dlms_status axdr_put_fixed(dlms_writer *w, const uint8_t *p, size_t n);  /* no length */
dlms_status axdr_put_octet_string(dlms_writer *w, const uint8_t *p, size_t n); /* len+bytes */
dlms_status axdr_put_optional(dlms_writer *w, bool present);            /* 00 / 01 */

dlms_status axdr_get_u8 (dlms_reader *r, uint8_t *v);
dlms_status axdr_get_i8 (dlms_reader *r, int8_t  *v);
dlms_status axdr_get_u16(dlms_reader *r, uint16_t *v);
dlms_status axdr_get_u32(dlms_reader *r, uint32_t *v);
dlms_status axdr_get_bool(dlms_reader *r, bool *v);
dlms_status axdr_get_fixed(dlms_reader *r, size_t n, const uint8_t **p);
dlms_status axdr_get_octet_string(dlms_reader *r, const uint8_t **p, size_t *n);
dlms_status axdr_get_optional(dlms_reader *r, bool *present);

dlms_status dlms_put_attr_desc(dlms_writer *w, const cosem_attr_desc *d);
dlms_status dlms_get_attr_desc(dlms_reader *r, cosem_attr_desc *d);
dlms_status dlms_put_method_desc(dlms_writer *w, const cosem_method_desc *d);
dlms_status dlms_get_method_desc(dlms_reader *r, cosem_method_desc *d);

/* =====================================================================
 *  Service-level headers.  *_begin => caller appends the noted payload next.
 * ===================================================================== */

/* ---- GET (client encodes request, server decodes) --------------------- */

/* get-request-normal. If has_selection: appends presence + access_selector,
 * then the CALLER streams the access-parameters Data. Else PDU is complete. */
dlms_status dlms_enc_get_request_normal(dlms_writer *w, uint8_t iap,
                                        const cosem_attr_desc *d,
                                        bool has_selection, uint8_t access_selector);

/* get-request-next: complete PDU. */
dlms_status dlms_enc_get_request_next(dlms_writer *w, uint8_t iap, uint32_t block_number);

/* Server side: parse the leading two tag bytes of any xDLMS PDU. */
dlms_status dlms_read_apdu_header(dlms_reader *r, uint8_t *apdu_tag, uint8_t *variant_tag);

/* After dlms_read_apdu_header confirmed GET/normal: read its fixed head.
 * If *has_selection, reader is left at access_selector's following Data
 * (access_selector already returned); caller pulls that Data. */
dlms_status dlms_read_get_request_normal(dlms_reader *r, uint8_t *iap,
                                         cosem_attr_desc *d,
                                         bool *has_selection, uint8_t *access_selector);
dlms_status dlms_read_get_request_next(dlms_reader *r, uint8_t *iap, uint32_t *block_number);

/* ---- GET response (server encodes, client decodes) -------------------- */

/* get-response-normal carrying data: writes IAP + Get-Data-Result 'data' tag.
 * Caller then streams exactly one Data (the attribute value). */
dlms_status dlms_enc_get_response_normal_data(dlms_writer *w, uint8_t iap);
/* get-response-normal carrying an error: complete PDU. */
dlms_status dlms_enc_get_response_normal_result(dlms_writer *w, uint8_t iap, dlms_dar dar);

/* get-response-with-datablock header. writes IAP, last_block, block_number and
 * the DataBlock-G result CHOICE tag. If is_raw: writes raw-data length and the
 * caller streams `raw_len` bytes; else writes the data-access-result byte
 * (`dar`) and the PDU is complete. */
dlms_status dlms_enc_get_response_datablock(dlms_writer *w, uint8_t iap,
                                            bool last_block, uint32_t block_number,
                                            bool is_raw, size_t raw_len, dlms_dar dar);

/* Client side. Reads IAP + Get-Data-Result tag. If *is_data, reader is at the
 * Data value (caller pulls with axdr_decode_data/axdr_walk); else *dar set. */
dlms_status dlms_read_get_response_normal(dlms_reader *r, uint8_t *iap,
                                          bool *is_data, dlms_dar *dar);

/* ---- SET (client encodes request, server encodes response) ------------ */

/* set-request-normal head. If has_selection: caller streams the access-
 * parameters Data, THEN the value Data. Else: caller streams the value Data. */
dlms_status dlms_enc_set_request_normal(dlms_writer *w, uint8_t iap,
                                        const cosem_attr_desc *d,
                                        bool has_selection, uint8_t access_selector);
dlms_status dlms_read_set_request_normal(dlms_reader *r, uint8_t *iap,
                                         cosem_attr_desc *d,
                                         bool *has_selection, uint8_t *access_selector);

dlms_status dlms_enc_set_response_normal(dlms_writer *w, uint8_t iap, dlms_dar dar);
dlms_status dlms_read_set_response_normal(dlms_reader *r, uint8_t *iap, dlms_dar *dar);

/* ---- ACTION ----------------------------------------------------------- */

/* action-request-normal head. If has_params, caller streams one Data
 * (method-invocation-parameters); else PDU is complete. */
dlms_status dlms_enc_action_request_normal(dlms_writer *w, uint8_t iap,
                                           const cosem_method_desc *m, bool has_params);
dlms_status dlms_read_action_request_normal(dlms_reader *r, uint8_t *iap,
                                            cosem_method_desc *m, bool *has_params);

/* action-response-normal head: IAP + Action-Result + return-parameters
 * presence. If has_return: writes the Get-Data-Result tag (is_data ? data :
 * access-result). For data, caller streams one Data; for access-result the
 * `ret_dar` byte is written and the PDU is complete. */
dlms_status dlms_enc_action_response_normal(dlms_writer *w, uint8_t iap,
                                            dlms_action_result res, bool has_return,
                                            bool ret_is_data, dlms_dar ret_dar);
dlms_status dlms_read_action_response_normal(dlms_reader *r, uint8_t *iap,
                                             dlms_action_result *res, bool *has_return,
                                             bool *ret_is_data, dlms_dar *ret_dar);

/* ---- Data-Notification (unsolicited; either side) --------------------- */

/* Writes Long-IAP + date-time octet-string. Caller then streams one Data. */
dlms_status dlms_enc_data_notification(dlms_writer *w, uint32_t long_iap,
                                       const uint8_t *date_time, size_t dt_len);
/* Reads Long-IAP + date-time (ptr+len into buffer). Reader left at the Data. */
dlms_status dlms_read_data_notification(dlms_reader *r, uint32_t *long_iap,
                                        const uint8_t **date_time, size_t *dt_len);

/* ---- shared list building blocks -------------------------------------- */

/* Cosem-Attribute-Descriptor-With-Selection. If has_selection, caller then
 * streams the access-parameters Data (encode) / pulls it (decode). */
dlms_status dlms_put_attr_desc_with_selection(dlms_writer *w, const cosem_attr_desc *d,
                                              bool has_selection, uint8_t access_selector);
dlms_status dlms_read_attr_desc_with_selection(dlms_reader *r, cosem_attr_desc *d,
                                               bool *has_selection, uint8_t *access_selector);

/* Get-Data-Result CHOICE. `_data` writes the data tag then the caller streams
 * one Data; `_error` writes the access-result and is complete. On decode,
 * *is_data => reader is at the Data; else *dar is set. */
dlms_status dlms_put_get_data_result_data(dlms_writer *w);
dlms_status dlms_put_get_data_result_error(dlms_writer *w, dlms_dar dar);
dlms_status dlms_read_get_data_result(dlms_reader *r, bool *is_data, dlms_dar *dar);

/* ---- GET with-list ---------------------------------------------------- */
/* Header only: writes C0 03 IAP <count>. Caller then writes `count` entries
 * with dlms_put_attr_desc_with_selection (+ Data per entry when selected). */
dlms_status dlms_enc_get_request_with_list(dlms_writer *w, uint8_t iap, uint16_t count);
dlms_status dlms_read_get_request_with_list(dlms_reader *r, uint8_t *iap, uint16_t *count);

/* Header only: writes C4 03 IAP <count>. Caller then writes `count` results
 * with dlms_put_get_data_result_* (+ Data per data-result). */
dlms_status dlms_enc_get_response_with_list(dlms_writer *w, uint8_t iap, uint16_t count);
dlms_status dlms_read_get_response_with_list(dlms_reader *r, uint8_t *iap, uint16_t *count);

/* ---- Event-Notification (no invoke-id; time is OPTIONAL) -------------- */
/* Writes C2, optional time octet-string, attribute descriptor. Caller then
 * streams one Data (attribute-value). */
dlms_status dlms_enc_event_notification(dlms_writer *w, bool has_time,
                                        const uint8_t *time, size_t time_len,
                                        const cosem_attr_desc *d);
dlms_status dlms_read_event_notification(dlms_reader *r, bool *has_time,
                                         const uint8_t **time, size_t *time_len,
                                         cosem_attr_desc *d);

/* ---- Exception-Response ---------------------------------------------- */
typedef enum {
    EXC_STATE_SERVICE_NOT_ALLOWED = 1,
    EXC_STATE_SERVICE_UNKNOWN     = 2
} dlms_exc_state;
typedef enum {
    EXC_SVC_OPERATION_NOT_POSSIBLE   = 1,
    EXC_SVC_SERVICE_NOT_SUPPORTED    = 2,
    EXC_SVC_OTHER_REASON             = 3,
    EXC_SVC_PDU_TOO_LONG             = 4,
    EXC_SVC_DECIPHERING_ERROR        = 5,
    EXC_SVC_INVOCATION_COUNTER_ERROR = 6   /* carries Unsigned32 */
} dlms_exc_service;

/* invocation_counter is written/read only when service == INVOCATION_COUNTER_ERROR. */
dlms_status dlms_enc_exception_response(dlms_writer *w, dlms_exc_state state,
                                        dlms_exc_service service, uint32_t invocation_counter);
dlms_status dlms_read_exception_response(dlms_reader *r, dlms_exc_state *state,
                                         dlms_exc_service *service, bool *has_ic,
                                         uint32_t *invocation_counter);

#ifdef __cplusplus
}
#endif
#endif /* XDLMS_H */
