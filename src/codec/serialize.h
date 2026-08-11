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

/** @brief Read a generic COSEM value (tag + data) from the buffer. */
osp_err_t osp_value_read(osp_buf_t *buf, osp_value_t *val);
/** @brief Write a generic COSEM value (tag + data) to the buffer. */
osp_err_t osp_value_write(osp_buf_t *buf, const osp_value_t *val);

/** @brief Encode a COSEM compact-array (tag 19) from a normal array value. */
osp_err_t osp_value_write_compact_array(osp_buf_t *buf, const osp_value_t *val);

/** @brief Skip a value without storing it (used for selective access). */
osp_err_t osp_value_skip(osp_buf_t *buf);

/* ═══════════════════════════════════════════════════════════════════════════
 *  PRIMITIVE TYPE HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Read an AXDR-encoded 8-bit signed integer. */
osp_err_t osp_axdr_read_i8(osp_buf_t *buf, int8_t *val);
/** @brief Read an AXDR-encoded 16-bit signed integer. */
osp_err_t osp_axdr_read_i16(osp_buf_t *buf, int16_t *val);
/** @brief Read an AXDR-encoded 32-bit signed integer. */
osp_err_t osp_axdr_read_i32(osp_buf_t *buf, int32_t *val);
/** @brief Read an AXDR-encoded 64-bit signed integer. */
osp_err_t osp_axdr_read_i64(osp_buf_t *buf, int64_t *val);
/** @brief Read an AXDR-encoded 64-bit unsigned integer. */
osp_err_t osp_axdr_read_u64(osp_buf_t *buf, uint64_t *val);

/** @brief Write an 8-bit signed integer in AXDR encoding. */
osp_err_t osp_axdr_write_i8(osp_buf_t *buf, int8_t val);
/** @brief Write a 16-bit signed integer in AXDR encoding. */
osp_err_t osp_axdr_write_i16(osp_buf_t *buf, int16_t val);
/** @brief Write a 32-bit signed integer in AXDR encoding. */
osp_err_t osp_axdr_write_i32(osp_buf_t *buf, int32_t val);
/** @brief Write a 64-bit signed integer in AXDR encoding. */
osp_err_t osp_axdr_write_i64(osp_buf_t *buf, int64_t val);
/** @brief Write a 64-bit unsigned integer in AXDR encoding. */
osp_err_t osp_axdr_write_u64(osp_buf_t *buf, uint64_t val);

/** @brief Read an AXDR-encoded visible string from the buffer. */
osp_err_t osp_axdr_read_visible_string(osp_buf_t *buf, char *out, uint32_t max, uint32_t *len);
/** @brief Write a visible string in AXDR encoding to the buffer. */
osp_err_t osp_axdr_write_visible_string(osp_buf_t *buf, const char *str, uint32_t len);

/* ═══════════════════════════════════════════════════════════════════════════
 *  DATE / TIME / DATETIME
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Read a COSEM date from the buffer. */
osp_err_t osp_date_read(osp_buf_t *buf, osp_date_t *date);
/** @brief Write a COSEM date to the buffer. */
osp_err_t osp_date_write(osp_buf_t *buf, const osp_date_t *date);
/** @brief Read a COSEM time from the buffer. */
osp_err_t osp_time_read(osp_buf_t *buf, osp_time_t *time);
/** @brief Write a COSEM time to the buffer. */
osp_err_t osp_time_write(osp_buf_t *buf, const osp_time_t *time);
/** @brief Read a COSEM datetime from the buffer. */
osp_err_t osp_datetime_read(osp_buf_t *buf, osp_datetime_t *dt);
/** @brief Write a COSEM datetime to the buffer. */
osp_err_t osp_datetime_write(osp_buf_t *buf, const osp_datetime_t *dt);

/** @brief Populate a COSEM datetime structure from raw bytes. */
void osp_cosem_datetime_from_bytes(osp_cosem_datetime_t *out, const uint8_t bytes[OSP_COSEM_DATETIME_LEN]);
/** @brief Serialize a COSEM datetime structure to raw bytes. */
void osp_cosem_datetime_to_bytes(const osp_cosem_datetime_t *dt, uint8_t bytes[OSP_COSEM_DATETIME_LEN]);
/** @brief Read a COSEM datetime from a generic value container. */
osp_err_t osp_cosem_datetime_read_value(const osp_value_t *val, osp_cosem_datetime_t *dt);
/** @brief Create a generic value containing a COSEM datetime. */
osp_value_t osp_val_cosem_datetime(const osp_cosem_datetime_t *dt);

/* ═══════════════════════════════════════════════════════════════════════════
 *  STRUCTURE SERIALIZATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Write a structure header (tag=2) with the given field count. */
osp_err_t osp_struct_begin(osp_buf_t *buf, uint8_t num_fields);
/** @brief Read a structure header and return the number of fields. */
osp_err_t osp_struct_begin_read(osp_buf_t *buf, uint8_t *num_fields);

/* ═══════════════════════════════════════════════════════════════════════════
 *  ARRAY SERIALIZATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Write an array header with the given element count. */
osp_err_t osp_array_begin(osp_buf_t *buf, uint8_t count);
/** @brief Read an array header and return the element count. */
osp_err_t osp_array_begin_read(osp_buf_t *buf, uint8_t *count);

/* ═══════════════════════════════════════════════════════════════════════════
 *  BITSTRING
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Read a bitstring from the buffer into the provided array. */
osp_err_t osp_bitstring_read(osp_buf_t *buf, uint8_t *bits, uint32_t max_bits, uint32_t *num_bits);
/** @brief Write a bitstring to the buffer from the provided array. */
osp_err_t osp_bitstring_write(osp_buf_t *buf, const uint8_t *bits, uint32_t num_bits);

/* ═══════════════════════════════════════════════════════════════════════════
 *  COMMON STRUCTURE HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Read an OBIS code from the buffer. */
osp_err_t osp_obis_read(osp_buf_t *buf, osp_obis_t *obis);
/** @brief Write an OBIS code to the buffer. */
osp_err_t osp_obis_write(osp_buf_t *buf, const osp_obis_t *obis);

/** @brief Read an access right structure from the buffer. */
osp_err_t osp_access_right_read(osp_buf_t *buf, osp_access_right_t *ar);
/** @brief Write an access right structure to the buffer. */
osp_err_t osp_access_right_write(osp_buf_t *buf, const osp_access_right_t *ar);

/** @brief Read an object list element from the buffer. */
osp_err_t osp_object_list_element_read(osp_buf_t *buf, osp_object_list_element_t *elem);
/** @brief Write an object list element to the buffer. */
osp_err_t osp_object_list_element_write(osp_buf_t *buf, const osp_object_list_element_t *elem);
/** @brief Write a full object_list as an A-XDR ARRAY (tag + elements). */
osp_err_t osp_object_list_write(osp_buf_t *buf, const osp_object_list_t *ol);
/** @brief Write a ProfileGeneric buffer as an A-XDR ARRAY of row structures. */
osp_err_t osp_profile_buffer_write(osp_buf_t *buf, const osp_profile_buffer_view_t *view);
/** @brief Write a capture-object list as an A-XDR ARRAY. */
osp_err_t osp_capture_object_list_write(osp_buf_t *buf, const osp_capture_object_list_t *list);
/** @brief Write a day-profile table as an A-XDR ARRAY (Blue Book layout). */
osp_err_t osp_day_profile_table_write(osp_buf_t *buf, const osp_day_profile_table_view_t *view);
/** @brief Write a season-profile table as an A-XDR ARRAY. */
osp_err_t osp_season_profile_table_write(osp_buf_t *buf, const osp_season_profile_table_view_t *view);
/** @brief Write a week-profile table as an A-XDR ARRAY. */
osp_err_t osp_week_profile_table_write(osp_buf_t *buf, const osp_week_profile_table_view_t *view);
/** @brief Write a Special Days table as an A-XDR ARRAY. */
osp_err_t osp_special_days_list_write(osp_buf_t *buf, const osp_special_days_list_view_t *view);
/** @brief Write a Schedule entries list as an A-XDR ARRAY. */
osp_err_t osp_schedule_list_write(osp_buf_t *buf, const osp_schedule_list_view_t *view);
/** @brief Write a Script Table scripts list as an A-XDR ARRAY. */
osp_err_t osp_script_list_write(osp_buf_t *buf, const osp_script_list_view_t *view);
/** @brief Write a Push Setup object list as an A-XDR ARRAY. */
osp_err_t osp_push_object_list_write(osp_buf_t *buf, const osp_push_object_list_view_t *view);
/** @brief Write Push communication windows as an A-XDR ARRAY. */
osp_err_t osp_comm_window_list_write(osp_buf_t *buf, const osp_comm_window_list_view_t *view);
/** @brief Write a register-monitor threshold list as an A-XDR ARRAY. */
osp_err_t osp_threshold_list_write(osp_buf_t *buf, const osp_threshold_list_t *tl);
/** @brief Write a SAP assignment list as an A-XDR ARRAY. */
osp_err_t osp_sap_assignment_list_write(osp_buf_t *buf, const osp_sap_assignment_list_t *list);
/** @brief Write a status-mapping table as an A-XDR ARRAY. */
osp_err_t osp_status_mapping_table_write(osp_buf_t *buf, const osp_status_mapping_table_view_t *view);
/** @brief Write Single Action Schedule execution times as an A-XDR ARRAY. */
osp_err_t osp_execution_time_list_write(osp_buf_t *buf, const osp_execution_time_list_view_t *view);
/** @brief Write a Data Protection methods list as an A-XDR ARRAY. */
osp_err_t osp_data_protection_list_write(osp_buf_t *buf, const osp_data_protection_list_t *list);
/** @brief Write an Association user list as an A-XDR ARRAY. */
osp_err_t osp_user_list_write(osp_buf_t *buf, const osp_user_list_view_t *view);
/** @brief Write a long-unsigned array (e.g. emergency group IDs). */
osp_err_t osp_u16_array_write(osp_buf_t *buf, const osp_u16_array_view_t *view);
/** @brief Write a double-long-unsigned array (e.g. IPv4 multicast). */
osp_err_t osp_u32_array_write(osp_buf_t *buf, const osp_u32_array_view_t *view);

/** @brief Read a capture object structure from the buffer. */
osp_err_t osp_capture_object_read(osp_buf_t *buf, osp_capture_object_t *co);
/** @brief Write a capture object structure to the buffer. */
osp_err_t osp_capture_object_write(osp_buf_t *buf, const osp_capture_object_t *co);

/** @brief Read a value definition structure from the buffer. */
osp_err_t osp_value_definition_read(osp_buf_t *buf, osp_value_definition_t *vd);
/** @brief Write a value definition structure to the buffer. */
osp_err_t osp_value_definition_write(osp_buf_t *buf, const osp_value_definition_t *vd);

/** @brief Read a scaler unit structure from the buffer. */
osp_err_t osp_scaler_unit_read(osp_buf_t *buf, osp_scaler_unit_t *su);
/** @brief Write a scaler unit structure to the buffer. */
osp_err_t osp_scaler_unit_write(osp_buf_t *buf, const osp_scaler_unit_t *su);

/** @brief Read a user list item from the buffer. */
osp_err_t osp_user_list_item_read(osp_buf_t *buf, osp_user_list_item_t *item);
/** @brief Write a user list item to the buffer. */
osp_err_t osp_user_list_item_write(osp_buf_t *buf, const osp_user_list_item_t *item);

/** @brief Read an attribute descriptor from the buffer. */
osp_err_t osp_attribute_descriptor_read(osp_buf_t *buf, osp_attribute_descriptor_t *ad);
/** @brief Write an attribute descriptor to the buffer. */
osp_err_t osp_attribute_descriptor_write(osp_buf_t *buf, const osp_attribute_descriptor_t *ad);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SERIALIZE_H */
