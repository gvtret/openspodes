/**
 * serialize.h — AXDR serialize/deserialize for COSEM value types
 *
 * Two approaches:
 *   osp_value_read/write  — generic (tag + data, for CHOICE/any-type attributes)
 *   osp_*_read/write      — typed helpers for specific IC structures
 *
 * All work on osp_buf_t (zero-copy, no malloc).
 */

#ifndef OSP_SERIALIZE_H
#define OSP_SERIALIZE_H

#include "../openspodes.h"
#include "codec.h"
#include "structures.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 *  GENERIC VALUE SERIALIZATION (tag + data)
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_value_read(osp_buf_t *buf, osp_value_t *val);
osp_err_t osp_value_write(osp_buf_t *buf, const osp_value_t *val);

/* Encode COSEM compact-array (tag 19) from a normal array value. */
osp_err_t osp_value_write_compact_array(osp_buf_t *buf, const osp_value_t *val);

/* Skip a value without storing it (for selective access) */
osp_err_t osp_value_skip(osp_buf_t *buf);

/* ═══════════════════════════════════════════════════════════════════════════
 *  PRIMITIVE TYPE HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_axdr_read_i8(osp_buf_t *buf, int8_t *val);
osp_err_t osp_axdr_read_i16(osp_buf_t *buf, int16_t *val);
osp_err_t osp_axdr_read_i32(osp_buf_t *buf, int32_t *val);
osp_err_t osp_axdr_read_i64(osp_buf_t *buf, int64_t *val);
osp_err_t osp_axdr_read_u64(osp_buf_t *buf, uint64_t *val);

osp_err_t osp_axdr_write_i8(osp_buf_t *buf, int8_t val);
osp_err_t osp_axdr_write_i16(osp_buf_t *buf, int16_t val);
osp_err_t osp_axdr_write_i32(osp_buf_t *buf, int32_t val);
osp_err_t osp_axdr_write_i64(osp_buf_t *buf, int64_t val);
osp_err_t osp_axdr_write_u64(osp_buf_t *buf, uint64_t val);

osp_err_t osp_axdr_read_visible_string(osp_buf_t *buf, char *out, uint32_t max, uint32_t *len);
osp_err_t osp_axdr_write_visible_string(osp_buf_t *buf, const char *str, uint32_t len);

/* ═══════════════════════════════════════════════════════════════════════════
 *  DATE / TIME / DATETIME
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_date_read(osp_buf_t *buf, osp_date_t *date);
osp_err_t osp_date_write(osp_buf_t *buf, const osp_date_t *date);
osp_err_t osp_time_read(osp_buf_t *buf, osp_time_t *time);
osp_err_t osp_time_write(osp_buf_t *buf, const osp_time_t *time);
osp_err_t osp_datetime_read(osp_buf_t *buf, osp_datetime_t *dt);
osp_err_t osp_datetime_write(osp_buf_t *buf, const osp_datetime_t *dt);

void osp_cosem_datetime_from_bytes(osp_cosem_datetime_t *out, const uint8_t bytes[OSP_COSEM_DATETIME_LEN]);
void osp_cosem_datetime_to_bytes(const osp_cosem_datetime_t *dt, uint8_t bytes[OSP_COSEM_DATETIME_LEN]);
osp_err_t osp_cosem_datetime_read_value(const osp_value_t *val, osp_cosem_datetime_t *dt);
osp_value_t osp_val_cosem_datetime(const osp_cosem_datetime_t *dt);

/* ═══════════════════════════════════════════════════════════════════════════
 *  STRUCTURE SERIALIZATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Write a structure header (tag=2 + length) */
osp_err_t osp_struct_begin(osp_buf_t *buf, uint8_t num_fields);
/* Read a structure header, return number of fields */
osp_err_t osp_struct_begin_read(osp_buf_t *buf, uint8_t *num_fields);

/* ═══════════════════════════════════════════════════════════════════════════
 *  ARRAY SERIALIZATION
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_array_begin(osp_buf_t *buf, uint8_t count);
osp_err_t osp_array_begin_read(osp_buf_t *buf, uint8_t *count);

/* ═══════════════════════════════════════════════════════════════════════════
 *  BITSTRING
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_bitstring_read(osp_buf_t *buf, uint8_t *bits, uint32_t max_bits, uint32_t *num_bits);
osp_err_t osp_bitstring_write(osp_buf_t *buf, const uint8_t *bits, uint32_t num_bits);

/* ═══════════════════════════════════════════════════════════════════════════
 *  COMMON STRUCTURE HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_obis_read(osp_buf_t *buf, osp_obis_t *obis);
osp_err_t osp_obis_write(osp_buf_t *buf, const osp_obis_t *obis);

osp_err_t osp_access_right_read(osp_buf_t *buf, osp_access_right_t *ar);
osp_err_t osp_access_right_write(osp_buf_t *buf, const osp_access_right_t *ar);

osp_err_t osp_object_list_element_read(osp_buf_t *buf, osp_object_list_element_t *elem);
osp_err_t osp_object_list_element_write(osp_buf_t *buf, const osp_object_list_element_t *elem);

osp_err_t osp_capture_object_read(osp_buf_t *buf, osp_capture_object_t *co);
osp_err_t osp_capture_object_write(osp_buf_t *buf, const osp_capture_object_t *co);

osp_err_t osp_value_definition_read(osp_buf_t *buf, osp_value_definition_t *vd);
osp_err_t osp_value_definition_write(osp_buf_t *buf, const osp_value_definition_t *vd);

osp_err_t osp_scaler_unit_read(osp_buf_t *buf, osp_scaler_unit_t *su);
osp_err_t osp_scaler_unit_write(osp_buf_t *buf, const osp_scaler_unit_t *su);

osp_err_t osp_user_list_item_read(osp_buf_t *buf, osp_user_list_item_t *item);
osp_err_t osp_user_list_item_write(osp_buf_t *buf, const osp_user_list_item_t *item);

osp_err_t osp_attribute_descriptor_read(osp_buf_t *buf, osp_attribute_descriptor_t *ad);
osp_err_t osp_attribute_descriptor_write(osp_buf_t *buf, const osp_attribute_descriptor_t *ad);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SERIALIZE_H */
