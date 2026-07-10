/* xdlms.c -- streaming xDLMS LN service codec. */
#include <string.h>
#include "xdlms.h"

/* ------------------------------------------------------- byte-level I/O */
static dlms_status pw(dlms_writer *w, uint8_t b) {
    if (w->pos >= w->cap) return DLMS_ERR_OVERFLOW;
    w->buf[w->pos++] = b; return DLMS_OK;
}
static dlms_status pwn(dlms_writer *w, const uint8_t *p, size_t n) {
    if (n > w->cap - w->pos) return DLMS_ERR_OVERFLOW;
    memcpy(w->buf + w->pos, p, n); w->pos += n; return DLMS_OK;
}
static dlms_status pr(dlms_reader *r, uint8_t *b) {
    if (dlms_remaining(r) < 1) return DLMS_ERR_TRUNCATED;
    *b = r->buf[r->pos++]; return DLMS_OK;
}

/* ============================ primitives ============================== */
dlms_status axdr_put_u8 (dlms_writer *w, uint8_t v)  { return pw(w, v); }
dlms_status axdr_put_i8 (dlms_writer *w, int8_t  v)  { return pw(w, (uint8_t)v); }
dlms_status axdr_put_u16(dlms_writer *w, uint16_t v) {
    dlms_status s = pw(w, (uint8_t)(v >> 8)); if (s) return s; return pw(w, (uint8_t)v);
}
dlms_status axdr_put_u32(dlms_writer *w, uint32_t v) {
    for (int i = 3; i >= 0; i--) { dlms_status s = pw(w, (uint8_t)(v >> (8*i))); if (s) return s; }
    return DLMS_OK;
}
dlms_status axdr_put_bool(dlms_writer *w, bool v)   { return pw(w, v ? 0x01 : 0x00); }
dlms_status axdr_put_fixed(dlms_writer *w, const uint8_t *p, size_t n) { return pwn(w, p, n); }
dlms_status axdr_put_octet_string(dlms_writer *w, const uint8_t *p, size_t n) {
    dlms_status s = dlms_write_len(w, (uint32_t)n); if (s) return s; return pwn(w, p, n);
}
dlms_status axdr_put_optional(dlms_writer *w, bool present) { return pw(w, present ? 0x01 : 0x00); }

dlms_status axdr_get_u8 (dlms_reader *r, uint8_t *v) { return pr(r, v); }
dlms_status axdr_get_i8 (dlms_reader *r, int8_t  *v) {
    uint8_t b; dlms_status s = pr(r, &b); if (s) return s; *v = (int8_t)b; return DLMS_OK;
}
dlms_status axdr_get_u16(dlms_reader *r, uint16_t *v) {
    if (dlms_remaining(r) < 2) return DLMS_ERR_TRUNCATED;
    *v = (uint16_t)((r->buf[r->pos] << 8) | r->buf[r->pos+1]); r->pos += 2; return DLMS_OK;
}
dlms_status axdr_get_u32(dlms_reader *r, uint32_t *v) {
    if (dlms_remaining(r) < 4) return DLMS_ERR_TRUNCATED;
    uint32_t x = 0; for (int i = 0; i < 4; i++) x = (x << 8) | r->buf[r->pos++];
    *v = x; return DLMS_OK;
}
dlms_status axdr_get_bool(dlms_reader *r, bool *v) {
    uint8_t b; dlms_status s = pr(r, &b); if (s) return s; *v = (b != 0); return DLMS_OK;
}
dlms_status axdr_get_fixed(dlms_reader *r, size_t n, const uint8_t **p) {
    if (dlms_remaining(r) < n) return DLMS_ERR_TRUNCATED;
    *p = r->buf + r->pos; r->pos += n; return DLMS_OK;
}
dlms_status axdr_get_octet_string(dlms_reader *r, const uint8_t **p, size_t *n) {
    uint32_t len; dlms_status s = dlms_read_len(r, &len); if (s) return s;
    if (dlms_remaining(r) < len) return DLMS_ERR_TRUNCATED;
    *p = r->buf + r->pos; *n = len; r->pos += len; return DLMS_OK;
}
dlms_status axdr_get_optional(dlms_reader *r, bool *present) {
    uint8_t b; dlms_status s = pr(r, &b); if (s) return s;
    if (b != 0 && b != 1) return DLMS_ERR_BAD_TAG;
    *present = (b == 1); return DLMS_OK;
}

/* ------------------------------------------------------ descriptors */
dlms_status dlms_put_attr_desc(dlms_writer *w, const cosem_attr_desc *d) {
    dlms_status s = axdr_put_u16(w, d->class_id); if (s) return s;
    s = axdr_put_fixed(w, d->instance_id, 6);     if (s) return s;
    return axdr_put_i8(w, d->attribute_id);
}
dlms_status dlms_get_attr_desc(dlms_reader *r, cosem_attr_desc *d) {
    dlms_status s = axdr_get_u16(r, &d->class_id); if (s) return s;
    const uint8_t *p; s = axdr_get_fixed(r, 6, &p); if (s) return s;
    memcpy(d->instance_id, p, 6);
    return axdr_get_i8(r, &d->attribute_id);
}
dlms_status dlms_put_method_desc(dlms_writer *w, const cosem_method_desc *m) {
    dlms_status s = axdr_put_u16(w, m->class_id); if (s) return s;
    s = axdr_put_fixed(w, m->instance_id, 6);     if (s) return s;
    return axdr_put_i8(w, m->method_id);
}
dlms_status dlms_get_method_desc(dlms_reader *r, cosem_method_desc *m) {
    dlms_status s = axdr_get_u16(r, &m->class_id); if (s) return s;
    const uint8_t *p; s = axdr_get_fixed(r, 6, &p); if (s) return s;
    memcpy(m->instance_id, p, 6);
    return axdr_get_i8(r, &m->method_id);
}

/* -------------------------------------- two-byte APDU/variant framing */
static dlms_status put_head(dlms_writer *w, uint8_t apdu, uint8_t variant) {
    dlms_status s = pw(w, apdu); if (s) return s; return pw(w, variant);
}
dlms_status dlms_read_apdu_header(dlms_reader *r, uint8_t *apdu_tag, uint8_t *variant_tag) {
    dlms_status s = pr(r, apdu_tag); if (s) return s;
    /* data-notification, event-notification and exception-response are plain
     * SEQUENCEs under their APDU tag -- no CHOICE variant byte follows. */
    if (*apdu_tag == APDU_DATA_NOTIFICATION ||
        *apdu_tag == APDU_EVENT_NOTIFICATION ||
        *apdu_tag == APDU_EXCEPTION_RESPONSE) {
        *variant_tag = 0; return DLMS_OK;
    }
    return pr(r, variant_tag);
}

/* ============================ GET request ============================= */
dlms_status dlms_enc_get_request_normal(dlms_writer *w, uint8_t iap,
                                        const cosem_attr_desc *d,
                                        bool has_selection, uint8_t access_selector) {
    dlms_status s = put_head(w, APDU_GET_REQUEST, GET_REQUEST_NORMAL); if (s) return s;
    s = axdr_put_u8(w, iap);        if (s) return s;
    s = dlms_put_attr_desc(w, d);   if (s) return s;
    s = axdr_put_optional(w, has_selection); if (s) return s;
    if (has_selection) s = axdr_put_u8(w, access_selector);
    return s; /* caller streams access-parameters Data next if has_selection */
}
dlms_status dlms_enc_get_request_next(dlms_writer *w, uint8_t iap, uint32_t block_number) {
    dlms_status s = put_head(w, APDU_GET_REQUEST, GET_REQUEST_NEXT); if (s) return s;
    s = axdr_put_u8(w, iap); if (s) return s;
    return axdr_put_u32(w, block_number);
}
dlms_status dlms_read_get_request_normal(dlms_reader *r, uint8_t *iap,
                                         cosem_attr_desc *d,
                                         bool *has_selection, uint8_t *access_selector) {
    dlms_status s = axdr_get_u8(r, iap);   if (s) return s;
    s = dlms_get_attr_desc(r, d);          if (s) return s;
    s = axdr_get_optional(r, has_selection); if (s) return s;
    if (*has_selection) s = axdr_get_u8(r, access_selector);
    return s;
}
dlms_status dlms_read_get_request_next(dlms_reader *r, uint8_t *iap, uint32_t *block_number) {
    dlms_status s = axdr_get_u8(r, iap); if (s) return s;
    return axdr_get_u32(r, block_number);
}

/* ============================ GET response =========================== */
dlms_status dlms_enc_get_response_normal_data(dlms_writer *w, uint8_t iap) {
    dlms_status s = put_head(w, APDU_GET_RESPONSE, GET_RESPONSE_NORMAL); if (s) return s;
    s = axdr_put_u8(w, iap);         if (s) return s;
    return axdr_put_u8(w, GDR_DATA); /* caller streams the Data next */
}
dlms_status dlms_enc_get_response_normal_result(dlms_writer *w, uint8_t iap, dlms_dar dar) {
    dlms_status s = put_head(w, APDU_GET_RESPONSE, GET_RESPONSE_NORMAL); if (s) return s;
    s = axdr_put_u8(w, iap);              if (s) return s;
    s = axdr_put_u8(w, GDR_ACCESS_RESULT); if (s) return s;
    return axdr_put_u8(w, (uint8_t)dar);
}
dlms_status dlms_enc_get_response_datablock(dlms_writer *w, uint8_t iap,
                                            bool last_block, uint32_t block_number,
                                            bool is_raw, size_t raw_len, dlms_dar dar) {
    dlms_status s = put_head(w, APDU_GET_RESPONSE, GET_RESPONSE_WITH_DATABLOCK); if (s) return s;
    s = axdr_put_u8(w, iap);              if (s) return s;
    s = axdr_put_bool(w, last_block);     if (s) return s;
    s = axdr_put_u32(w, block_number);    if (s) return s;
    if (is_raw) {
        s = axdr_put_u8(w, 0);            if (s) return s; /* raw-data [0] */
        return dlms_write_len(w, (uint32_t)raw_len); /* caller streams raw_len bytes */
    }
    s = axdr_put_u8(w, 1);                if (s) return s; /* data-access-result [1] */
    return axdr_put_u8(w, (uint8_t)dar);
}
dlms_status dlms_read_get_response_normal(dlms_reader *r, uint8_t *iap,
                                          bool *is_data, dlms_dar *dar) {
    dlms_status s = axdr_get_u8(r, iap); if (s) return s;
    uint8_t gdr; s = axdr_get_u8(r, &gdr); if (s) return s;
    if (gdr == GDR_DATA)          { *is_data = true;  return DLMS_OK; }
    if (gdr == GDR_ACCESS_RESULT) { uint8_t b; s = axdr_get_u8(r, &b); if (s) return s;
                                    *is_data = false; *dar = (dlms_dar)b; return DLMS_OK; }
    return DLMS_ERR_BAD_TAG;
}

/* ============================ SET ==================================== */
dlms_status dlms_enc_set_request_normal(dlms_writer *w, uint8_t iap,
                                        const cosem_attr_desc *d,
                                        bool has_selection, uint8_t access_selector) {
    dlms_status s = put_head(w, APDU_SET_REQUEST, SET_REQUEST_NORMAL); if (s) return s;
    s = axdr_put_u8(w, iap);        if (s) return s;
    s = dlms_put_attr_desc(w, d);   if (s) return s;
    s = axdr_put_optional(w, has_selection); if (s) return s;
    if (has_selection) s = axdr_put_u8(w, access_selector);
    return s; /* caller streams [access-params Data then] value Data */
}
dlms_status dlms_read_set_request_normal(dlms_reader *r, uint8_t *iap,
                                         cosem_attr_desc *d,
                                         bool *has_selection, uint8_t *access_selector) {
    dlms_status s = axdr_get_u8(r, iap);   if (s) return s;
    s = dlms_get_attr_desc(r, d);          if (s) return s;
    s = axdr_get_optional(r, has_selection); if (s) return s;
    if (*has_selection) s = axdr_get_u8(r, access_selector);
    return s;
}
dlms_status dlms_enc_set_response_normal(dlms_writer *w, uint8_t iap, dlms_dar dar) {
    dlms_status s = put_head(w, APDU_SET_RESPONSE, SET_RESPONSE_NORMAL); if (s) return s;
    s = axdr_put_u8(w, iap); if (s) return s;
    return axdr_put_u8(w, (uint8_t)dar);
}
dlms_status dlms_read_set_response_normal(dlms_reader *r, uint8_t *iap, dlms_dar *dar) {
    dlms_status s = axdr_get_u8(r, iap); if (s) return s;
    uint8_t b; s = axdr_get_u8(r, &b); if (s) return s;
    *dar = (dlms_dar)b; return DLMS_OK;
}

/* ============================ ACTION ================================= */
dlms_status dlms_enc_action_request_normal(dlms_writer *w, uint8_t iap,
                                           const cosem_method_desc *m, bool has_params) {
    dlms_status s = put_head(w, APDU_ACTION_REQUEST, ACTION_REQUEST_NORMAL); if (s) return s;
    s = axdr_put_u8(w, iap);          if (s) return s;
    s = dlms_put_method_desc(w, m);   if (s) return s;
    return axdr_put_optional(w, has_params); /* caller streams params Data if set */
}
dlms_status dlms_read_action_request_normal(dlms_reader *r, uint8_t *iap,
                                            cosem_method_desc *m, bool *has_params) {
    dlms_status s = axdr_get_u8(r, iap); if (s) return s;
    s = dlms_get_method_desc(r, m);      if (s) return s;
    return axdr_get_optional(r, has_params);
}
dlms_status dlms_enc_action_response_normal(dlms_writer *w, uint8_t iap,
                                            dlms_action_result res, bool has_return,
                                            bool ret_is_data, dlms_dar ret_dar) {
    dlms_status s = put_head(w, APDU_ACTION_RESPONSE, ACTION_RESPONSE_NORMAL); if (s) return s;
    s = axdr_put_u8(w, iap);            if (s) return s;
    s = axdr_put_u8(w, (uint8_t)res);   if (s) return s;
    s = axdr_put_optional(w, has_return); if (s) return s;
    if (!has_return) return DLMS_OK;
    if (ret_is_data) return axdr_put_u8(w, GDR_DATA); /* caller streams Data */
    s = axdr_put_u8(w, GDR_ACCESS_RESULT); if (s) return s;
    return axdr_put_u8(w, (uint8_t)ret_dar);
}
dlms_status dlms_read_action_response_normal(dlms_reader *r, uint8_t *iap,
                                             dlms_action_result *res, bool *has_return,
                                             bool *ret_is_data, dlms_dar *ret_dar) {
    dlms_status s = axdr_get_u8(r, iap); if (s) return s;
    uint8_t b; s = axdr_get_u8(r, &b); if (s) return s; *res = (dlms_action_result)b;
    s = axdr_get_optional(r, has_return); if (s) return s;
    if (!*has_return) { *ret_is_data = false; return DLMS_OK; }
    uint8_t gdr; s = axdr_get_u8(r, &gdr); if (s) return s;
    if (gdr == GDR_DATA)          { *ret_is_data = true;  return DLMS_OK; }
    if (gdr == GDR_ACCESS_RESULT) { s = axdr_get_u8(r, &b); if (s) return s;
                                    *ret_is_data = false; *ret_dar = (dlms_dar)b; return DLMS_OK; }
    return DLMS_ERR_BAD_TAG;
}

/* ======================= Data-Notification ========================== */
dlms_status dlms_enc_data_notification(dlms_writer *w, uint32_t long_iap,
                                       const uint8_t *date_time, size_t dt_len) {
    dlms_status s = pw(w, APDU_DATA_NOTIFICATION); if (s) return s;
    s = axdr_put_u32(w, long_iap); if (s) return s;
    return axdr_put_octet_string(w, date_time, dt_len); /* caller streams Data next */
}
dlms_status dlms_read_data_notification(dlms_reader *r, uint32_t *long_iap,
                                        const uint8_t **date_time, size_t *dt_len) {
    dlms_status s = axdr_get_u32(r, long_iap); if (s) return s;
    return axdr_get_octet_string(r, date_time, dt_len);
}

/* ===================== shared list building blocks =================== */
dlms_status dlms_put_attr_desc_with_selection(dlms_writer *w, const cosem_attr_desc *d,
                                              bool has_selection, uint8_t access_selector) {
    dlms_status s = dlms_put_attr_desc(w, d); if (s) return s;
    s = axdr_put_optional(w, has_selection);  if (s) return s;
    if (has_selection) s = axdr_put_u8(w, access_selector);
    return s; /* caller streams access-parameters Data if has_selection */
}
dlms_status dlms_read_attr_desc_with_selection(dlms_reader *r, cosem_attr_desc *d,
                                               bool *has_selection, uint8_t *access_selector) {
    dlms_status s = dlms_get_attr_desc(r, d);       if (s) return s;
    s = axdr_get_optional(r, has_selection);        if (s) return s;
    if (*has_selection) s = axdr_get_u8(r, access_selector);
    return s;
}

dlms_status dlms_put_get_data_result_data(dlms_writer *w) {
    return axdr_put_u8(w, GDR_DATA); /* caller streams Data */
}
dlms_status dlms_put_get_data_result_error(dlms_writer *w, dlms_dar dar) {
    dlms_status s = axdr_put_u8(w, GDR_ACCESS_RESULT); if (s) return s;
    return axdr_put_u8(w, (uint8_t)dar);
}
dlms_status dlms_read_get_data_result(dlms_reader *r, bool *is_data, dlms_dar *dar) {
    uint8_t gdr; dlms_status s = axdr_get_u8(r, &gdr); if (s) return s;
    if (gdr == GDR_DATA)          { *is_data = true;  return DLMS_OK; }
    if (gdr == GDR_ACCESS_RESULT) { uint8_t b; s = axdr_get_u8(r, &b); if (s) return s;
                                    *is_data = false; *dar = (dlms_dar)b; return DLMS_OK; }
    return DLMS_ERR_BAD_TAG;
}

/* ============================ GET with-list ========================= */
dlms_status dlms_enc_get_request_with_list(dlms_writer *w, uint8_t iap, uint16_t count) {
    dlms_status s = put_head(w, APDU_GET_REQUEST, GET_REQUEST_WITH_LIST); if (s) return s;
    s = axdr_put_u8(w, iap); if (s) return s;
    return dlms_write_len(w, count); /* SEQUENCE OF count; caller writes entries */
}
dlms_status dlms_read_get_request_with_list(dlms_reader *r, uint8_t *iap, uint16_t *count) {
    dlms_status s = axdr_get_u8(r, iap); if (s) return s;
    uint32_t n; s = dlms_read_len(r, &n); if (s) return s;
    if (n > 0xFFFF) return DLMS_ERR_RANGE;
    *count = (uint16_t)n; return DLMS_OK;
}
dlms_status dlms_enc_get_response_with_list(dlms_writer *w, uint8_t iap, uint16_t count) {
    dlms_status s = put_head(w, APDU_GET_RESPONSE, GET_RESPONSE_WITH_LIST); if (s) return s;
    s = axdr_put_u8(w, iap); if (s) return s;
    return dlms_write_len(w, count);
}
dlms_status dlms_read_get_response_with_list(dlms_reader *r, uint8_t *iap, uint16_t *count) {
    dlms_status s = axdr_get_u8(r, iap); if (s) return s;
    uint32_t n; s = dlms_read_len(r, &n); if (s) return s;
    if (n > 0xFFFF) return DLMS_ERR_RANGE;
    *count = (uint16_t)n; return DLMS_OK;
}

/* ======================= Event-Notification ========================= */
dlms_status dlms_enc_event_notification(dlms_writer *w, bool has_time,
                                        const uint8_t *time, size_t time_len,
                                        const cosem_attr_desc *d) {
    dlms_status s = pw(w, APDU_EVENT_NOTIFICATION); if (s) return s;
    s = axdr_put_optional(w, has_time); if (s) return s;
    if (has_time) { s = axdr_put_octet_string(w, time, time_len); if (s) return s; }
    return dlms_put_attr_desc(w, d); /* caller streams attribute-value Data */
}
dlms_status dlms_read_event_notification(dlms_reader *r, bool *has_time,
                                         const uint8_t **time, size_t *time_len,
                                         cosem_attr_desc *d) {
    dlms_status s = axdr_get_optional(r, has_time); if (s) return s;
    if (*has_time) { s = axdr_get_octet_string(r, time, time_len); if (s) return s; }
    else { *time = NULL; *time_len = 0; }
    return dlms_get_attr_desc(r, d);
}

/* ======================= Exception-Response ========================= */
dlms_status dlms_enc_exception_response(dlms_writer *w, dlms_exc_state state,
                                        dlms_exc_service service, uint32_t invocation_counter) {
    dlms_status s = pw(w, APDU_EXCEPTION_RESPONSE); if (s) return s;
    s = axdr_put_u8(w, (uint8_t)state);   if (s) return s;
    s = axdr_put_u8(w, (uint8_t)service); if (s) return s;
    if (service == EXC_SVC_INVOCATION_COUNTER_ERROR)
        return axdr_put_u32(w, invocation_counter);
    return DLMS_OK;
}
dlms_status dlms_read_exception_response(dlms_reader *r, dlms_exc_state *state,
                                         dlms_exc_service *service, bool *has_ic,
                                         uint32_t *invocation_counter) {
    uint8_t b; dlms_status s = axdr_get_u8(r, &b); if (s) return s; *state = (dlms_exc_state)b;
    s = axdr_get_u8(r, &b); if (s) return s; *service = (dlms_exc_service)b;
    if (*service == EXC_SVC_INVOCATION_COUNTER_ERROR) {
        *has_ic = true; return axdr_get_u32(r, invocation_counter);
    }
    *has_ic = false; return DLMS_OK;
}
