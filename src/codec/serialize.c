/**
 * serialize.c — AXDR serialize/deserialize for COSEM types
 *
 * Generic osp_value_read/write handles any type by tag.
 * Typed helpers for date/time, access_right, etc.
 *
 * Thread safety: This module uses a static pool for nested value decoding.
 * When osp_hal_mutex is set, the pool is protected by mutex lock/unlock.
 * For bare-metal or single-threaded use, leave osp_hal_mutex NULL (no overhead).
 */

#include "serialize.h"
#include <string.h>

/* Global mutex HAL pointer — defined here, declared in openspodes.h */
osp_mutex_t *osp_hal_mutex = NULL;

/* One long-lived handle from create(); never destroyed per osp_value_read. */
static void *g_shared_mutex_handle = NULL;

void osp_hal_mutex_lock(void) {
	if (!osp_hal_mutex || !osp_hal_mutex->lock) {
		return;
	}
	if (!g_shared_mutex_handle && osp_hal_mutex->create) {
		g_shared_mutex_handle = osp_hal_mutex->create(osp_hal_mutex->ctx);
	}
	if (g_shared_mutex_handle) {
		osp_hal_mutex->lock(osp_hal_mutex->ctx, g_shared_mutex_handle);
	}
}

void osp_hal_mutex_unlock(void) {
	if (g_shared_mutex_handle && osp_hal_mutex && osp_hal_mutex->unlock) {
		osp_hal_mutex->unlock(osp_hal_mutex->ctx, g_shared_mutex_handle);
	}
}

/* Bump pool for nested structure/array elements during osp_value_read (no malloc). */
static osp_value_t value_read_pool[OSP_VALUE_READ_POOL_LEN];
static uint16_t value_read_pool_used;
static uint8_t value_read_depth;

static osp_err_t osp_value_read_impl(osp_buf_t *buf, osp_value_t *val);
static osp_err_t axdr_read_f32(osp_buf_t *buf, float *f);
static osp_err_t axdr_write_f32(osp_buf_t *buf, float f);
static osp_err_t axdr_read_f64(osp_buf_t *buf, double *f);
static osp_err_t axdr_write_f64(osp_buf_t *buf, double f);

/* ═══════════════════════════════════════════════════════════════════════════
 *  PRIMITIVE HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_axdr_read_i8(osp_buf_t *buf, int8_t *val) {
	uint8_t v;
	osp_err_t r = osp_axdr_read_u8(buf, &v);
	if (r == OSP_OK) {
		*val = (int8_t)v;
	}
	return r;
}

osp_err_t osp_axdr_read_i16(osp_buf_t *buf, int16_t *val) {
	uint16_t v;
	osp_err_t r = osp_axdr_read_u16(buf, &v);
	if (r == OSP_OK) {
		*val = (int16_t)v;
	}
	return r;
}

osp_err_t osp_axdr_read_i32(osp_buf_t *buf, int32_t *val) {
	uint32_t v;
	osp_err_t r = osp_axdr_read_u32(buf, &v);
	if (r == OSP_OK) {
		*val = (int32_t)v;
	}
	return r;
}

osp_err_t osp_axdr_read_i64(osp_buf_t *buf, int64_t *val) {
	if (!buf || !val || osp_buf_unread(buf) < 8) {
		return OSP_ERR_INVALID;
	}
	*val = ((uint64_t)buf->buf[buf->rd] << 56) | ((uint64_t)buf->buf[buf->rd + 1] << 48) | ((uint64_t)buf->buf[buf->rd + 2] << 40) |
	    ((uint64_t)buf->buf[buf->rd + 3] << 32) | ((uint64_t)buf->buf[buf->rd + 4] << 24) | ((uint64_t)buf->buf[buf->rd + 5] << 16) |
	    ((uint64_t)buf->buf[buf->rd + 6] << 8) | (uint64_t)buf->buf[buf->rd + 7];
	buf->rd += 8;
	return OSP_OK;
}

osp_err_t osp_axdr_read_u64(osp_buf_t *buf, uint64_t *val) {
	if (!buf || !val || osp_buf_unread(buf) < 8) {
		return OSP_ERR_INVALID;
	}
	*val = ((uint64_t)buf->buf[buf->rd] << 56) | ((uint64_t)buf->buf[buf->rd + 1] << 48) | ((uint64_t)buf->buf[buf->rd + 2] << 40) |
	    ((uint64_t)buf->buf[buf->rd + 3] << 32) | ((uint64_t)buf->buf[buf->rd + 4] << 24) | ((uint64_t)buf->buf[buf->rd + 5] << 16) |
	    ((uint64_t)buf->buf[buf->rd + 6] << 8) | (uint64_t)buf->buf[buf->rd + 7];
	buf->rd += 8;
	return OSP_OK;
}

osp_err_t osp_axdr_write_i8(osp_buf_t *buf, int8_t val) {
	return osp_axdr_write_u8(buf, (uint8_t)val);
}

osp_err_t osp_axdr_write_i16(osp_buf_t *buf, int16_t val) {
	return osp_axdr_write_u16(buf, (uint16_t)val);
}

osp_err_t osp_axdr_write_i32(osp_buf_t *buf, int32_t val) {
	return osp_axdr_write_u32(buf, (uint32_t)val);
}

osp_err_t osp_axdr_write_i64(osp_buf_t *buf, int64_t val) {
	if (!buf || osp_buf_free(buf) < 8) {
		return OSP_ERR_INVALID;
	}
	uint8_t *p = &buf->buf[buf->wr];
	p[0] = (uint8_t)(val >> 56);
	p[1] = (uint8_t)(val >> 48);
	p[2] = (uint8_t)(val >> 40);
	p[3] = (uint8_t)(val >> 32);
	p[4] = (uint8_t)(val >> 24);
	p[5] = (uint8_t)(val >> 16);
	p[6] = (uint8_t)(val >> 8);
	p[7] = (uint8_t)(val);
	buf->wr += 8;
	return OSP_OK;
}

osp_err_t osp_axdr_write_u64(osp_buf_t *buf, uint64_t val) {
	if (!buf || osp_buf_free(buf) < 8) {
		return OSP_ERR_INVALID;
	}
	uint8_t *p = &buf->buf[buf->wr];
	p[0] = (uint8_t)(val >> 56);
	p[1] = (uint8_t)(val >> 48);
	p[2] = (uint8_t)(val >> 40);
	p[3] = (uint8_t)(val >> 32);
	p[4] = (uint8_t)(val >> 24);
	p[5] = (uint8_t)(val >> 16);
	p[6] = (uint8_t)(val >> 8);
	p[7] = (uint8_t)(val);
	buf->wr += 8;
	return OSP_OK;
}

osp_err_t osp_axdr_read_visible_string(osp_buf_t *buf, char *out, uint32_t max, uint32_t *len) {
	uint8_t tag;
	osp_err_t r = osp_axdr_read_tag(buf, &tag);
	if (r != OSP_OK) {
		return r;
	}
	if (tag != 10) {
		return OSP_ERR_INVALID; /* visible-string */
	}

	uint32_t slen;
	r = osp_ber_read_length(buf, &slen);
	if (r != OSP_OK) {
		return r;
	}
	if (slen > max - 1) {
		return OSP_ERR_NOMEM;
	}
	if (osp_buf_unread(buf) < slen) {
		return OSP_ERR_INVALID;
	}

	memcpy(out, &buf->buf[buf->rd], slen);
	buf->rd += slen;
	out[slen] = '\0';
	if (len) {
		*len = slen;
	}
	return OSP_OK;
}

osp_err_t osp_axdr_write_visible_string(osp_buf_t *buf, const char *str, uint32_t len) {
	osp_err_t r = osp_axdr_write_tag(buf, 10);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_ber_write_length(buf, len);
	if (r != OSP_OK) {
		return r;
	}
	if (osp_buf_free(buf) < len) {
		return OSP_ERR_NOMEM;
	}
	memcpy(&buf->buf[buf->wr], str, len);
	buf->wr += len;
	return OSP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  DATE / TIME / DATETIME
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Date: 5 bytes (year_hi, year_lo, month, day, day_of_week) */
osp_err_t osp_date_read(osp_buf_t *buf, osp_date_t *date) {
	if (!buf || !date || osp_buf_unread(buf) < 5) {
		return OSP_ERR_INVALID;
	}
	date->year = ((uint16_t)buf->buf[buf->rd] << 8) | buf->buf[buf->rd + 1];
	date->month = buf->buf[buf->rd + 2];
	date->day = buf->buf[buf->rd + 3];
	date->day_of_week = buf->buf[buf->rd + 4];
	buf->rd += 5;
	return OSP_OK;
}

osp_err_t osp_date_write(osp_buf_t *buf, const osp_date_t *date) {
	if (!buf || !date || osp_buf_free(buf) < 5) {
		return OSP_ERR_INVALID;
	}
	buf->buf[buf->wr] = (uint8_t)(date->year >> 8);
	buf->buf[buf->wr + 1] = (uint8_t)(date->year & 0xFF);
	buf->buf[buf->wr + 2] = date->month;
	buf->buf[buf->wr + 3] = date->day;
	buf->buf[buf->wr + 4] = date->day_of_week;
	buf->wr += 5;
	return OSP_OK;
}

/* Time: 4 bytes (hour, minute, second, ms) */
osp_err_t osp_time_read(osp_buf_t *buf, osp_time_t *time) {
	if (!buf || !time || osp_buf_unread(buf) < 4) {
		return OSP_ERR_INVALID;
	}
	time->hour = buf->buf[buf->rd];
	time->minute = buf->buf[buf->rd + 1];
	time->second = buf->buf[buf->rd + 2];
	time->ms = buf->buf[buf->rd + 3];
	buf->rd += 4;
	return OSP_OK;
}

osp_err_t osp_time_write(osp_buf_t *buf, const osp_time_t *time) {
	if (!buf || !time || osp_buf_free(buf) < 4) {
		return OSP_ERR_INVALID;
	}
	buf->buf[buf->wr] = time->hour;
	buf->buf[buf->wr + 1] = time->minute;
	buf->buf[buf->wr + 2] = time->second;
	buf->buf[buf->wr + 3] = time->ms;
	buf->wr += 4;
	return OSP_OK;
}

/* DateTime: 12 bytes (5 date + 4 time + 2 reserved + 1 deviation) */
osp_err_t osp_datetime_read(osp_buf_t *buf, osp_datetime_t *dt) {
	osp_err_t r = osp_date_read(buf, &dt->date);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_time_read(buf, &dt->time);
	if (r != OSP_OK) {
		return r;
	}
	/* Skip 2 reserved bytes + 1 deviation byte */
	if (osp_buf_unread(buf) < 3) {
		return OSP_ERR_INVALID;
	}
	buf->rd += 3;
	return OSP_OK;
}

osp_err_t osp_datetime_write(osp_buf_t *buf, const osp_datetime_t *dt) {
	osp_err_t r = osp_date_write(buf, &dt->date);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_time_write(buf, &dt->time);
	if (r != OSP_OK) {
		return r;
	}
	if (osp_buf_free(buf) < 3) {
		return OSP_ERR_NOMEM;
	}
	buf->buf[buf->wr] = 0xFF;     /* reserved */
	buf->buf[buf->wr + 1] = 0xFF; /* reserved */
	buf->buf[buf->wr + 2] = 0x80; /* deviation: 0 = UTC */
	buf->wr += 3;
	return OSP_OK;
}

void osp_cosem_datetime_from_bytes(osp_cosem_datetime_t *out, const uint8_t bytes[OSP_COSEM_DATETIME_LEN]) {
	if (!out || !bytes) {
		return;
	}
	out->year = ((uint16_t)bytes[0] << 8) | bytes[1];
	out->month = bytes[2];
	out->day = bytes[3];
	out->day_of_week = bytes[4];
	out->hour = bytes[5];
	out->minute = bytes[6];
	out->second = bytes[7];
	out->hundredths = bytes[8];
	out->deviation = (int16_t)(((uint16_t)bytes[9] << 8) | bytes[10]);
	out->clock_status = bytes[11];
}

void osp_cosem_datetime_to_bytes(const osp_cosem_datetime_t *dt, uint8_t bytes[OSP_COSEM_DATETIME_LEN]) {
	if (!dt || !bytes) {
		return;
	}
	bytes[0] = (uint8_t)(dt->year >> 8);
	bytes[1] = (uint8_t)(dt->year & 0xFF);
	bytes[2] = dt->month;
	bytes[3] = dt->day;
	bytes[4] = dt->day_of_week;
	bytes[5] = dt->hour;
	bytes[6] = dt->minute;
	bytes[7] = dt->second;
	bytes[8] = dt->hundredths;
	bytes[9] = (uint8_t)((uint16_t)dt->deviation >> 8);
	bytes[10] = (uint8_t)((uint16_t)dt->deviation & 0xFF);
	bytes[11] = dt->clock_status;
}

osp_err_t osp_cosem_datetime_read_value(const osp_value_t *val, osp_cosem_datetime_t *dt) {
	if (!val || !dt || val->tag != OSP_TAG_OCTETSTRING || val->as.octetstring.len != OSP_COSEM_DATETIME_LEN) {
		return OSP_ERR_INVALID;
	}
	osp_cosem_datetime_from_bytes(dt, val->as.octetstring.data);
	return OSP_OK;
}

osp_value_t osp_val_cosem_datetime(const osp_cosem_datetime_t *dt) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_OCTETSTRING;
	v.as.octetstring.len = OSP_COSEM_DATETIME_LEN;
	if (dt) {
		osp_cosem_datetime_to_bytes(dt, v.as.octetstring.data);
	}
	return v;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  STRUCTURE / ARRAY
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_struct_begin(osp_buf_t *buf, uint8_t num_fields) {
	osp_err_t r = osp_axdr_write_tag(buf, OSP_AXDR_STRUCTURE);
	if (r != OSP_OK) {
		return r;
	}
	return osp_ber_write_length(buf, num_fields);
}

osp_err_t osp_struct_begin_read(osp_buf_t *buf, uint8_t *num_fields) {
	uint8_t tag;
	osp_err_t r = osp_axdr_read_tag(buf, &tag);
	if (r != OSP_OK) {
		return r;
	}
	if (tag != OSP_AXDR_STRUCTURE) {
		return OSP_ERR_INVALID;
	}
	uint32_t count;
	r = osp_ber_read_length(buf, &count);
	if (r != OSP_OK || count > UINT8_MAX) {
		return OSP_ERR_INVALID;
	}
	*num_fields = (uint8_t)count;
	return OSP_OK;
}

osp_err_t osp_array_begin(osp_buf_t *buf, uint8_t count) {
	osp_err_t r = osp_axdr_write_tag(buf, OSP_AXDR_ARRAY);
	if (r != OSP_OK) {
		return r;
	}
	return osp_ber_write_length(buf, count);
}

osp_err_t osp_array_begin_read(osp_buf_t *buf, uint8_t *count) {
	uint8_t tag;
	osp_err_t r = osp_axdr_read_tag(buf, &tag);
	if (r != OSP_OK) {
		return r;
	}
	if (tag != OSP_AXDR_ARRAY) {
		return OSP_ERR_INVALID;
	}
	uint32_t n;
	r = osp_ber_read_length(buf, &n);
	if (r != OSP_OK || n > UINT8_MAX) {
		return OSP_ERR_INVALID;
	}
	*count = (uint8_t)n;
	return OSP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BITSTRING
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_bitstring_read(osp_buf_t *buf, uint8_t *bits, uint32_t max_bits, uint32_t *num_bits) {
	uint8_t tag;
	osp_err_t r = osp_axdr_read_tag(buf, &tag);
	if (r != OSP_OK) {
		return r;
	}
	if (tag != OSP_AXDR_BITSTRING) {
		return OSP_ERR_INVALID;
	}

	uint32_t len;
	r = osp_ber_read_length(buf, &len);
	if (r != OSP_OK) {
		return r;
	}
	if (len == 0) {
		*num_bits = 0;
		return OSP_OK;
	}
	if (osp_buf_unread(buf) < len) {
		return OSP_ERR_INVALID;
	}

	/* Check for overflow: len * 8 must not exceed uint32_t max */
	if (len > 0x1FFFFFFFu) {
		return OSP_ERR_NOMEM;
	}
	uint32_t total_bits = len * 8;
	/* Last byte may have unused bits (specified by first byte of bitstring) */
	if (total_bits > max_bits) {
		return OSP_ERR_NOMEM;
	}

	/* First byte = number of unused bits in last byte */
	uint8_t unused = buf->buf[buf->rd];
	buf->rd++;
	len--;

	for (uint32_t i = 0; i < len; i++) {
		bits[i] = buf->buf[buf->rd + i];
	}
	buf->rd += len;

	*num_bits = total_bits - (len == 0 ? 0 : unused);
	return OSP_OK;
}

osp_err_t osp_bitstring_write(osp_buf_t *buf, const uint8_t *bits, uint32_t num_bits) {
	osp_err_t r = osp_axdr_write_tag(buf, OSP_AXDR_BITSTRING);
	if (r != OSP_OK) {
		return r;
	}

	uint32_t num_bytes = (num_bits + 7) / 8;
	uint8_t unused = (uint8_t)((num_bytes * 8) - num_bits);

	/* Total: 1 (unused count) + num_bytes */
	r = osp_ber_write_length(buf, 1 + num_bytes);
	if (r != OSP_OK) {
		return r;
	}
	if (osp_buf_free(buf) < 1 + num_bytes) {
		return OSP_ERR_NOMEM;
	}

	buf->buf[buf->wr++] = unused;
	if (num_bytes > 0) {
		memcpy(&buf->buf[buf->wr], bits, num_bytes);
		buf->wr += num_bytes;
	}
	return OSP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  COMPACT-ARRAY (COSEM Data tag 19)
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint8_t dlms_encoded_len_size(uint32_t v) {
	if (v < 0x80u) {
		return 1;
	}
	if (v <= 0xFFu) {
		return 2;
	}
	if (v <= 0xFFFFu) {
		return 3;
	}
	if (v <= 0xFFFFFFu) {
		return 4;
	}
	return 5;
}

#define OSP_MAX_TYPE_DEPTH 16

static osp_err_t skip_type_description_depth(osp_buf_t *buf, uint8_t depth) {
	if (depth >= OSP_MAX_TYPE_DEPTH) {
		return OSP_ERR_INVALID;
	}
	uint8_t tag;
	osp_err_t r = osp_axdr_read_u8(buf, &tag);
	if (r != OSP_OK) {
		return r;
	}
	if (tag == OSP_TAG_STRUCTURE) {
		uint32_t n;
		r = osp_ber_read_length(buf, &n);
		if (r != OSP_OK) {
			return r;
		}
		for (uint32_t i = 0; i < n; i++) {
			r = skip_type_description_depth(buf, depth + 1);
			if (r != OSP_OK) {
				return r;
			}
		}
	} else if (tag == OSP_TAG_ARRAY) {
		if (osp_buf_unread(buf) < 2) {
			return OSP_ERR_INVALID;
		}
		buf->rd += 2;
		r = skip_type_description_depth(buf, depth + 1);
	}
	return r;
}

static osp_err_t skip_type_description(osp_buf_t *buf) {
	return skip_type_description_depth(buf, 0);
}

static osp_err_t write_type_description(osp_buf_t *buf, const osp_value_t *val) {
	if (!buf || !val) {
		return OSP_ERR_INVALID;
	}

	osp_err_t r = osp_axdr_write_u8(buf, val->tag);
	if (r != OSP_OK) {
		return r;
	}

	switch (val->tag) {
		case OSP_TAG_STRUCTURE: {
			const osp_value_list_t *list = &val->as.structure.elements;
			r = osp_ber_write_length(buf, list->count);
			if (r != OSP_OK) {
				return r;
			}
			for (uint8_t i = 0; i < list->count; i++) {
				r = write_type_description(buf, &list->items[i]);
				if (r != OSP_OK) {
					return r;
				}
			}
			return OSP_OK;
		}
		case OSP_TAG_ARRAY: {
			const osp_value_list_t *list = &val->as.array.elements;
			if (list->count == 0) {
				return OSP_ERR_INVALID;
			}
			r = osp_axdr_write_u16(buf, list->count);
			if (r != OSP_OK) {
				return r;
			}
			return write_type_description(buf, &list->items[0]);
		}
		default:
			return OSP_OK;
	}
}

static size_t packed_value_size(const osp_value_t *val) {
	if (!val) {
		return 0;
	}

	switch (val->tag) {
		case OSP_TAG_NULL:
			return 0;
		case OSP_TAG_BOOLEAN:
		case OSP_TAG_BCD:
		case OSP_TAG_INTEGER:
		case OSP_TAG_UNSIGNED:
		case OSP_TAG_ENUM:
			return 1;
		case OSP_TAG_LONG:
		case OSP_TAG_LONG_UNSIGNED:
			return 2;
		case OSP_TAG_DOUBLE_LONG:
		case OSP_TAG_DOUBLE_LONG_UNS:
		case OSP_TAG_FLOAT32:
			return 4;
		case OSP_TAG_LONG64:
		case OSP_TAG_LONG64_UNSIGNED:
		case OSP_TAG_FLOAT64:
			return 8;
		case OSP_TAG_DATE:
			return 5;
		case OSP_TAG_TIME:
			return 4;
		case OSP_TAG_DATETIME:
			return 12;
		case OSP_TAG_OCTETSTRING:
		case OSP_TAG_VISIBLESTRING:
		case OSP_TAG_UTF8STRING:
			return dlms_encoded_len_size(val->as.octetstring.len) + val->as.octetstring.len;
		case OSP_TAG_BITSTRING:
			return dlms_encoded_len_size(val->as.bitstring.num_bits) + (val->as.bitstring.num_bits + 7u) / 8u;
		case OSP_TAG_ARRAY:
		case OSP_TAG_STRUCTURE: {
			const osp_value_list_t *list = (val->tag == OSP_TAG_ARRAY) ? &val->as.array.elements : &val->as.structure.elements;
			size_t sum = 0;
			for (uint8_t i = 0; i < list->count; i++) {
				sum += packed_value_size(&list->items[i]);
			}
			return sum;
		}
		default:
			return 0;
	}
}

static osp_err_t pack_value(osp_buf_t *buf, const osp_value_t *val) {
	if (!buf || !val) {
		return OSP_ERR_INVALID;
	}

	osp_err_t r;
	switch (val->tag) {
		case OSP_TAG_NULL:
			return OSP_OK;
		case OSP_TAG_BOOLEAN:
			return osp_axdr_write_u8(buf, val->as.boolean.value ? 1u : 0u);
		case OSP_TAG_BCD:
		case OSP_TAG_INTEGER:
			return osp_axdr_write_i8(buf, val->as.int8.value);
		case OSP_TAG_UNSIGNED:
		case OSP_TAG_ENUM:
			return osp_axdr_write_u8(buf, val->as.uint8.value);
		case OSP_TAG_LONG:
			return osp_axdr_write_i16(buf, val->as.int16.value);
		case OSP_TAG_LONG_UNSIGNED:
			return osp_axdr_write_u16(buf, val->as.uint16.value);
		case OSP_TAG_DOUBLE_LONG:
			return osp_axdr_write_i32(buf, val->as.int32.value);
		case OSP_TAG_DOUBLE_LONG_UNS:
			return osp_axdr_write_u32(buf, val->as.uint32.value);
		case OSP_TAG_LONG64:
			return osp_axdr_write_i64(buf, val->as.int64.value);
		case OSP_TAG_LONG64_UNSIGNED:
			return osp_axdr_write_u64(buf, val->as.uint64.value);
		case OSP_TAG_FLOAT32:
			return axdr_write_f32(buf, val->as.float32.value);
		case OSP_TAG_FLOAT64:
			return axdr_write_f64(buf, val->as.float64.value);
		case OSP_TAG_OCTETSTRING:
			r = osp_ber_write_length(buf, val->as.octetstring.len);
			if (r != OSP_OK) {
				return r;
			}
			if (osp_buf_free(buf) < val->as.octetstring.len) {
				return OSP_ERR_NOMEM;
			}
			memcpy(&buf->buf[buf->wr], val->as.octetstring.data, val->as.octetstring.len);
			buf->wr += val->as.octetstring.len;
			return OSP_OK;
		case OSP_TAG_VISIBLESTRING:
			r = osp_ber_write_length(buf, val->as.visiblestring.len);
			if (r != OSP_OK) {
				return r;
			}
			if (osp_buf_free(buf) < val->as.visiblestring.len) {
				return OSP_ERR_NOMEM;
			}
			memcpy(&buf->buf[buf->wr], val->as.visiblestring.data, val->as.visiblestring.len);
			buf->wr += val->as.visiblestring.len;
			return OSP_OK;
		case OSP_TAG_UTF8STRING:
			r = osp_ber_write_length(buf, val->as.utf8string.len);
			if (r != OSP_OK) {
				return r;
			}
			if (osp_buf_free(buf) < val->as.utf8string.len) {
				return OSP_ERR_NOMEM;
			}
			memcpy(&buf->buf[buf->wr], val->as.utf8string.data, val->as.utf8string.len);
			buf->wr += val->as.utf8string.len;
			return OSP_OK;
		case OSP_TAG_BITSTRING: {
			uint32_t num_bits = val->as.bitstring.num_bits;
			uint32_t num_bytes = (num_bits + 7u) / 8u;
			r = osp_ber_write_length(buf, num_bits);
			if (r != OSP_OK) {
				return r;
			}
			if (osp_buf_free(buf) < num_bytes) {
				return OSP_ERR_NOMEM;
			}
			if (num_bytes > 0) {
				memcpy(&buf->buf[buf->wr], val->as.bitstring.bits, num_bytes);
				buf->wr += num_bytes;
			}
			return OSP_OK;
		}
		case OSP_TAG_DATE:
			return osp_date_write(buf, &val->as.date);
		case OSP_TAG_TIME:
			return osp_time_write(buf, &val->as.time);
		case OSP_TAG_DATETIME:
			return osp_datetime_write(buf, &val->as.datetime);
		case OSP_TAG_ARRAY:
		case OSP_TAG_STRUCTURE: {
			const osp_value_list_t *list = (val->tag == OSP_TAG_ARRAY) ? &val->as.array.elements : &val->as.structure.elements;
			for (uint8_t i = 0; i < list->count; i++) {
				r = pack_value(buf, &list->items[i]);
				if (r != OSP_OK) {
					return r;
				}
			}
			return OSP_OK;
		}
		default:
			return OSP_ERR_UNSUPPORTED;
	}
}

static osp_err_t read_packed_bitstring(osp_buf_t *buf, osp_bitstring_t *bs) {
	uint32_t num_bits;
	osp_err_t r = osp_ber_read_length(buf, &num_bits);
	if (r != OSP_OK) {
		return r;
	}
	uint32_t num_bytes = (num_bits + 7u) / 8u;
	if (num_bits > (uint32_t)OSP_MAX_BITSTRING_LEN * 8u || osp_buf_unread(buf) < num_bytes) {
		return OSP_ERR_INVALID;
	}
	bs->num_bits = num_bits;
	if (num_bytes > 0) {
		memcpy(bs->bits, &buf->buf[buf->rd], num_bytes);
		buf->rd += num_bytes;
	}
	return OSP_OK;
}

static osp_err_t read_packed_value(osp_buf_t *buf, uint8_t tag, osp_value_t *val) {
	memset(val, 0, sizeof(*val));
	val->tag = tag;

	osp_err_t r;
	switch (tag) {
		case OSP_TAG_NULL:
			return OSP_OK;
		case OSP_TAG_BOOLEAN: {
			uint8_t b;
			r = osp_axdr_read_u8(buf, &b);
			if (r == OSP_OK) {
				val->as.boolean.value = (b != 0);
			}
			return r;
		}
		case OSP_TAG_BCD:
		case OSP_TAG_INTEGER:
			return osp_axdr_read_i8(buf, &val->as.int8.value);
		case OSP_TAG_UNSIGNED:
			return osp_axdr_read_u8(buf, &val->as.uint8.value);
		case OSP_TAG_LONG:
			return osp_axdr_read_i16(buf, &val->as.int16.value);
		case OSP_TAG_LONG_UNSIGNED:
			return osp_axdr_read_u16(buf, &val->as.uint16.value);
		case OSP_TAG_DOUBLE_LONG:
			return osp_axdr_read_i32(buf, &val->as.int32.value);
		case OSP_TAG_DOUBLE_LONG_UNS:
			return osp_axdr_read_u32(buf, &val->as.uint32.value);
		case OSP_TAG_LONG64:
			return osp_axdr_read_i64(buf, &val->as.int64.value);
		case OSP_TAG_LONG64_UNSIGNED:
			return osp_axdr_read_u64(buf, &val->as.uint64.value);
		case OSP_TAG_ENUM:
			return osp_axdr_read_u8(buf, &val->as.enum_val.value);
		case OSP_TAG_FLOAT32:
			return axdr_read_f32(buf, &val->as.float32.value);
		case OSP_TAG_FLOAT64:
			return axdr_read_f64(buf, &val->as.float64.value);
		case OSP_TAG_OCTETSTRING: {
			uint32_t slen;
			r = osp_ber_read_length(buf, &slen);
			if (r != OSP_OK) {
				return r;
			}
			if (slen > OSP_MAX_OCTET_LEN || osp_buf_unread(buf) < slen) {
				return OSP_ERR_INVALID;
			}
			memcpy(val->as.octetstring.data, &buf->buf[buf->rd], slen);
			buf->rd += slen;
			val->as.octetstring.len = slen;
			return OSP_OK;
		}
		case OSP_TAG_VISIBLESTRING: {
			uint32_t slen;
			r = osp_ber_read_length(buf, &slen);
			if (r != OSP_OK) {
				return r;
			}
			if (slen > OSP_MAX_STRING_LEN - 1 || osp_buf_unread(buf) < slen) {
				return OSP_ERR_INVALID;
			}
			memcpy(val->as.visiblestring.data, &buf->buf[buf->rd], slen);
			buf->rd += slen;
			val->as.visiblestring.data[slen] = '\0';
			val->as.visiblestring.len = slen;
			return OSP_OK;
		}
		case OSP_TAG_UTF8STRING: {
			uint32_t slen;
			r = osp_ber_read_length(buf, &slen);
			if (r != OSP_OK) {
				return r;
			}
			if (slen > OSP_MAX_STRING_LEN || osp_buf_unread(buf) < slen) {
				return OSP_ERR_INVALID;
			}
			memcpy(val->as.utf8string.data, &buf->buf[buf->rd], slen);
			buf->rd += slen;
			val->as.utf8string.len = slen;
			return OSP_OK;
		}
		case OSP_TAG_BITSTRING:
			return read_packed_bitstring(buf, &val->as.bitstring);
		case OSP_TAG_DATE:
			return osp_date_read(buf, &val->as.date);
		case OSP_TAG_TIME:
			return osp_time_read(buf, &val->as.time);
		case OSP_TAG_DATETIME:
			return osp_datetime_read(buf, &val->as.datetime);
		default:
			return OSP_ERR_UNSUPPORTED;
	}
}

static osp_err_t skip_packed_value(osp_buf_t *buf, uint8_t tag) {
	uint8_t fixed = osp_axdr_type_size(tag);
	if (fixed > 0) {
		if (osp_buf_unread(buf) < fixed) {
			return OSP_ERR_INVALID;
		}
		buf->rd += fixed;
		return OSP_OK;
	}

	uint32_t slen;
	osp_err_t r;
	switch (tag) {
		case OSP_TAG_NULL:
			return OSP_OK;
		case OSP_TAG_OCTETSTRING:
		case OSP_TAG_VISIBLESTRING:
		case OSP_TAG_UTF8STRING:
			r = osp_ber_read_length(buf, &slen);
			if (r != OSP_OK) {
				return r;
			}
			if (osp_buf_unread(buf) < slen) {
				return OSP_ERR_INVALID;
			}
			buf->rd += slen;
			return OSP_OK;
		case OSP_TAG_BITSTRING:
			r = osp_ber_read_length(buf, &slen);
			if (r != OSP_OK) {
				return r;
			}
			slen = (slen + 7u) / 8u;
			if (osp_buf_unread(buf) < slen) {
				return OSP_ERR_INVALID;
			}
			buf->rd += slen;
			return OSP_OK;
		default:
			return OSP_ERR_UNSUPPORTED;
	}
}

static osp_err_t compact_skip_description(osp_buf_t *desc, osp_buf_t *contents) {
	uint8_t tag;
	osp_err_t r = osp_axdr_read_u8(desc, &tag);
	if (r != OSP_OK) {
		return r;
	}

	if (tag == OSP_TAG_STRUCTURE) {
		uint32_t n;
		r = osp_ber_read_length(desc, &n);
		if (r != OSP_OK) {
			return r;
		}
		for (uint32_t i = 0; i < n; i++) {
			r = compact_skip_description(desc, contents);
			if (r != OSP_OK) {
				return r;
			}
		}
		return OSP_OK;
	}

	if (tag == OSP_TAG_ARRAY) {
		uint16_t n;
		r = osp_axdr_read_u16(desc, &n);
		if (r != OSP_OK) {
			return r;
		}
		uint32_t elem_desc = desc->rd;
		for (uint16_t i = 0; i < n; i++) {
			desc->rd = elem_desc;
			r = compact_skip_description(desc, contents);
			if (r != OSP_OK) {
				return r;
			}
		}
		return OSP_OK;
	}

	return skip_packed_value(contents, tag);
}

static osp_err_t compact_count_elements(osp_buf_t *desc, osp_buf_t *contents, uint8_t *count) {
	*count = 0;
	while (contents->rd < contents->wr) {
		if (*count >= OSP_MAX_ARRAY_LEN) {
			return OSP_ERR_NOMEM;
		}
		desc->rd = 0;
		osp_err_t r = compact_skip_description(desc, contents);
		if (r != OSP_OK) {
			return r;
		}
		(*count)++;
	}
	return OSP_OK;
}

static osp_err_t compact_walk_description(osp_buf_t *desc, osp_buf_t *contents, osp_value_t *out);

static osp_err_t compact_decode_elements(osp_buf_t *desc, osp_buf_t *contents, osp_value_t *out) {
	uint8_t count;
	osp_err_t r = compact_count_elements(desc, contents, &count);
	if (r != OSP_OK) {
		return r;
	}
	if (value_read_pool_used + count > (uint16_t)(sizeof(value_read_pool) / sizeof(value_read_pool[0]))) {
		return OSP_ERR_NOMEM;
	}

	osp_value_t *items = &value_read_pool[value_read_pool_used];
	value_read_pool_used += count;
	contents->rd = 0;

	for (uint8_t i = 0; i < count; i++) {
		desc->rd = 0;
		r = compact_walk_description(desc, contents, &items[i]);
		if (r != OSP_OK) {
			return r;
		}
	}

	out->tag = OSP_TAG_ARRAY;
	out->as.array.elements.items = items;
	out->as.array.elements.count = count;
	out->as.array.elements.capacity = count;
	return OSP_OK;
}

static osp_err_t compact_walk_description(osp_buf_t *desc, osp_buf_t *contents, osp_value_t *out) {
	uint8_t tag;
	osp_err_t r = osp_axdr_read_u8(desc, &tag);
	if (r != OSP_OK) {
		return r;
	}

	if (tag == OSP_TAG_STRUCTURE) {
		uint32_t n;
		r = osp_ber_read_length(desc, &n);
		if (r != OSP_OK) {
			return r;
		}
		if (n > OSP_MAX_STRUCT_LEN || value_read_pool_used + n > (uint16_t)(sizeof(value_read_pool) / sizeof(value_read_pool[0]))) {
			return OSP_ERR_NOMEM;
		}
		osp_value_t *items = &value_read_pool[value_read_pool_used];
		value_read_pool_used += (uint16_t)n;
		for (uint32_t i = 0; i < n; i++) {
			r = compact_walk_description(desc, contents, &items[i]);
			if (r != OSP_OK) {
				return r;
			}
		}
		out->tag = OSP_TAG_STRUCTURE;
		out->as.structure.elements.items = items;
		out->as.structure.elements.count = (uint8_t)n;
		out->as.structure.elements.capacity = (uint8_t)n;
		return OSP_OK;
	}

	if (tag == OSP_TAG_ARRAY) {
		uint16_t n;
		r = osp_axdr_read_u16(desc, &n);
		if (r != OSP_OK) {
			return r;
		}
		if (n > OSP_MAX_ARRAY_LEN || value_read_pool_used + n > (uint16_t)(sizeof(value_read_pool) / sizeof(value_read_pool[0]))) {
			return OSP_ERR_NOMEM;
		}
		uint32_t elem_desc = desc->rd;
		osp_value_t *items = &value_read_pool[value_read_pool_used];
		value_read_pool_used += n;
		for (uint16_t i = 0; i < n; i++) {
			desc->rd = elem_desc;
			r = compact_walk_description(desc, contents, &items[i]);
			if (r != OSP_OK) {
				return r;
			}
		}
		out->tag = OSP_TAG_ARRAY;
		out->as.array.elements.items = items;
		out->as.array.elements.count = n;
		out->as.array.elements.capacity = n;
		return OSP_OK;
	}

	return read_packed_value(contents, tag, out);
}

static osp_err_t read_compact_array(osp_buf_t *buf, osp_value_t *val) {
	if (!buf || !val) {
		return OSP_ERR_INVALID;
	}

	uint32_t desc_start = buf->rd;
	osp_buf_t desc_probe;
	desc_probe.buf = buf->buf;
	desc_probe.size = buf->size;
	desc_probe.rd = buf->rd;
	desc_probe.wr = buf->wr;

	osp_err_t r = skip_type_description(&desc_probe);
	if (r != OSP_OK) {
		return r;
	}
	uint32_t desc_len = desc_probe.rd - desc_start;

	osp_buf_t desc;
	desc.buf = &buf->buf[desc_start];
	desc.size = buf->size;
	desc.rd = 0;
	desc.wr = desc_len;
	buf->rd = desc_start + desc_len;

	uint32_t clen;
	r = osp_ber_read_length(buf, &clen);
	if (r != OSP_OK) {
		return r;
	}
	if (osp_buf_unread(buf) < clen) {
		return OSP_ERR_INVALID;
	}

	uint32_t contents_start = buf->rd;

	osp_buf_t contents;
	contents.buf = &buf->buf[contents_start];
	contents.size = buf->size;
	contents.rd = 0;
	contents.wr = clen;

	r = compact_decode_elements(&desc, &contents, val);
	buf->rd = contents_start + clen;
	return r;
}

osp_err_t osp_value_write_compact_array(osp_buf_t *buf, const osp_value_t *val) {
	if (!buf || !val || val->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_INVALID;
	}
	const osp_value_list_t *list = &val->as.array.elements;
	if (list->count == 0) {
		return OSP_ERR_INVALID;
	}

	osp_err_t r = osp_axdr_write_u8(buf, OSP_TAG_COMPACT_ARRAY);
	if (r != OSP_OK) {
		return r;
	}
	r = write_type_description(buf, &list->items[0]);
	if (r != OSP_OK) {
		return r;
	}

	size_t total = 0;
	for (uint8_t i = 0; i < list->count; i++) {
		total += packed_value_size(&list->items[i]);
	}
	if (total > 0xFFFFFFFFu) {
		return OSP_ERR_INVALID;
	}
	r = osp_ber_write_length(buf, (uint32_t)total);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < list->count; i++) {
		r = pack_value(buf, &list->items[i]);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  GENERIC VALUE (tagged union)
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_value_read(osp_buf_t *buf, osp_value_t *val) {
	if (!buf || !val) {
		return OSP_ERR_INVALID;
	}

	osp_hal_mutex_lock();

	if (value_read_depth == 0) {
		value_read_pool_used = 0;
	}
	value_read_depth++;
	osp_err_t r = osp_value_read_impl(buf, val);
	value_read_depth--;

	osp_hal_mutex_unlock();

	return r;
}

static osp_err_t axdr_read_f32(osp_buf_t *buf, float *f) {
	uint32_t u;
	osp_err_t r = osp_axdr_read_u32(buf, &u);
	if (r != OSP_OK) {
		return r;
	}
	memcpy(f, &u, sizeof(*f));
	return OSP_OK;
}

static osp_err_t axdr_write_f32(osp_buf_t *buf, float f) {
	uint32_t u;
	memcpy(&u, &f, sizeof(u));
	return osp_axdr_write_u32(buf, u);
}

static osp_err_t axdr_read_f64(osp_buf_t *buf, double *f) {
	uint64_t u;
	osp_err_t r = osp_axdr_read_u64(buf, &u);
	if (r != OSP_OK) {
		return r;
	}
	memcpy(f, &u, sizeof(*f));
	return OSP_OK;
}

static osp_err_t axdr_write_f64(osp_buf_t *buf, double f) {
	uint64_t u;
	memcpy(&u, &f, sizeof(u));
	return osp_axdr_write_u64(buf, u);
}

static osp_err_t osp_value_read_impl(osp_buf_t *buf, osp_value_t *val) {
	if (!buf || !val) {
		return OSP_ERR_INVALID;
	}

	uint8_t tag;
	osp_err_t r = osp_axdr_read_tag(buf, &tag);
	if (r != OSP_OK) {
		return r;
	}

	memset(val, 0, sizeof(*val));
	val->tag = tag;

	switch (tag) {
		case OSP_TAG_NULL:
			return OSP_OK;

		case OSP_TAG_BOOLEAN:
			return osp_axdr_read_bool(buf, &val->as.boolean.value);

		case OSP_TAG_INTEGER:
			return osp_axdr_read_i8(buf, &val->as.int8.value);

		case OSP_TAG_LONG:
			return osp_axdr_read_i16(buf, &val->as.int16.value);

		case OSP_TAG_DOUBLE_LONG:
			return osp_axdr_read_i32(buf, &val->as.int32.value);

		case OSP_TAG_LONG64:
			return osp_axdr_read_i64(buf, &val->as.int64.value);

		case OSP_TAG_UNSIGNED:
			return osp_axdr_read_u8(buf, &val->as.uint8.value);

		case OSP_TAG_LONG_UNSIGNED:
			return osp_axdr_read_u16(buf, &val->as.uint16.value);

		case OSP_TAG_DOUBLE_LONG_UNS:
			return osp_axdr_read_u32(buf, &val->as.uint32.value);

		case OSP_TAG_LONG64_UNSIGNED:
			return osp_axdr_read_u64(buf, &val->as.uint64.value);

		case OSP_TAG_ENUM:
			return osp_axdr_read_u8(buf, &val->as.enum_val.value);

		case OSP_TAG_FLOAT32:
			return axdr_read_f32(buf, &val->as.float32.value);

		case OSP_TAG_FLOAT64:
			return axdr_read_f64(buf, &val->as.float64.value);

		case OSP_TAG_OCTETSTRING: {
			uint32_t slen;
			r = osp_ber_read_length(buf, &slen);
			if (r != OSP_OK) {
				return r;
			}
			if (slen > OSP_MAX_OCTET_LEN) {
				return OSP_ERR_NOMEM;
			}
			if (osp_buf_unread(buf) < slen) {
				return OSP_ERR_INVALID;
			}
			memcpy(val->as.octetstring.data, &buf->buf[buf->rd], slen);
			buf->rd += slen;
			val->as.octetstring.len = slen;
			return OSP_OK;
		}

		case OSP_TAG_VISIBLESTRING: {
			uint32_t slen;
			r = osp_ber_read_length(buf, &slen);
			if (r != OSP_OK) {
				return r;
			}
			if (slen > OSP_MAX_STRING_LEN - 1) {
				return OSP_ERR_NOMEM;
			}
			if (osp_buf_unread(buf) < slen) {
				return OSP_ERR_INVALID;
			}
			memcpy(val->as.visiblestring.data, &buf->buf[buf->rd], slen);
			buf->rd += slen;
			val->as.visiblestring.data[slen] = '\0';
			val->as.visiblestring.len = slen;
			return OSP_OK;
		}

		case OSP_TAG_BITSTRING: {
			uint32_t blen;
			r = osp_axdr_read_length(buf, &blen);
			if (r != OSP_OK) return r;
			if (blen == 0) {
				val->as.bitstring.num_bits = 0;
				return OSP_OK;
			}
			if (osp_buf_unread(buf) < blen) return OSP_ERR_INVALID;
			uint8_t unused = buf->buf[buf->rd++];
			blen--;
			uint32_t total_bits = blen * 8;
			if (total_bits > OSP_MAX_BITSTRING_LEN * 8) return OSP_ERR_NOMEM;
			memcpy(val->as.bitstring.bits, &buf->buf[buf->rd], blen);
			buf->rd += blen;
			val->as.bitstring.num_bits = total_bits - unused;
			return OSP_OK;
		}

		case OSP_TAG_UTF8STRING: {
			uint32_t slen;
			r = osp_axdr_read_length(buf, &slen);
			if (r != OSP_OK) return r;
			if (slen > OSP_MAX_STRING_LEN - 1) return OSP_ERR_NOMEM;
			if (osp_buf_unread(buf) < slen) return OSP_ERR_INVALID;
			memcpy(val->as.utf8string.data, &buf->buf[buf->rd], slen);
			buf->rd += slen;
			val->as.utf8string.data[slen] = '\0';
			val->as.utf8string.len = slen;
			return OSP_OK;
		}

		case OSP_TAG_BCD: {
			return osp_axdr_read_u8(buf, &val->as.bcd.value);
		}

		case OSP_TAG_DATE:
			return osp_date_read(buf, &val->as.date);

		case OSP_TAG_TIME:
			return osp_time_read(buf, &val->as.time);

		case OSP_TAG_DATETIME:
			return osp_datetime_read(buf, &val->as.datetime);

		case OSP_TAG_ARRAY:
		case OSP_TAG_STRUCTURE: {
			uint32_t count;
			r = osp_ber_read_length(buf, &count);
			if (r != OSP_OK) {
				return r;
			}
			uint8_t max = (tag == OSP_TAG_ARRAY) ? OSP_MAX_ARRAY_LEN : OSP_MAX_STRUCT_LEN;
			if (count > max) {
				return OSP_ERR_NOMEM;
			}
			if (value_read_pool_used + count > (uint16_t)(sizeof(value_read_pool) / sizeof(value_read_pool[0]))) {
				return OSP_ERR_NOMEM;
			}
			osp_value_t *items = &value_read_pool[value_read_pool_used];
			value_read_pool_used += count;
			for (uint32_t i = 0; i < count; i++) {
				r = osp_value_read_impl(buf, &items[i]);
				if (r != OSP_OK) {
					return r;
				}
			}
			osp_value_list_t *list = (tag == OSP_TAG_ARRAY) ? &val->as.array.elements : &val->as.structure.elements;
			list->items = items;
			list->count = count;
			list->capacity = count;
			return OSP_OK;
		}

		case OSP_TAG_COMPACT_ARRAY:
			return read_compact_array(buf, val);

		default:
			return OSP_ERR_UNSUPPORTED;
	}
}

osp_err_t osp_value_write(osp_buf_t *buf, const osp_value_t *val) {
	if (!buf || !val) {
		return OSP_ERR_INVALID;
	}

	osp_err_t r = osp_axdr_write_u8(buf, val->tag);
	if (r != OSP_OK) {
		return r;
	}

	switch (val->tag) {
		case OSP_TAG_NULL:
			return OSP_OK;

		case OSP_TAG_BOOLEAN:
			return osp_axdr_write_bool(buf, val->as.boolean.value);

		case OSP_TAG_INTEGER:
			return osp_axdr_write_i8(buf, val->as.int8.value);

		case OSP_TAG_LONG:
			return osp_axdr_write_i16(buf, val->as.int16.value);

		case OSP_TAG_DOUBLE_LONG:
			return osp_axdr_write_i32(buf, val->as.int32.value);

		case OSP_TAG_LONG64:
			return osp_axdr_write_i64(buf, val->as.int64.value);

		case OSP_TAG_UNSIGNED:
			return osp_axdr_write_u8(buf, val->as.uint8.value);

		case OSP_TAG_LONG_UNSIGNED:
			return osp_axdr_write_u16(buf, val->as.uint16.value);

		case OSP_TAG_DOUBLE_LONG_UNS:
			return osp_axdr_write_u32(buf, val->as.uint32.value);

		case OSP_TAG_LONG64_UNSIGNED:
			return osp_axdr_write_u64(buf, val->as.uint64.value);

		case OSP_TAG_ENUM:
			return osp_axdr_write_u8(buf, val->as.enum_val.value);

		case OSP_TAG_FLOAT32:
			return axdr_write_f32(buf, val->as.float32.value);

		case OSP_TAG_FLOAT64:
			return axdr_write_f64(buf, val->as.float64.value);

		case OSP_TAG_OCTETSTRING:
			return osp_axdr_write_octet_string(buf, val->as.octetstring.data, val->as.octetstring.len);

		case OSP_TAG_VISIBLESTRING: {
			uint32_t slen = val->as.visiblestring.len;
			r = osp_ber_write_length(buf, slen);
			if (r != OSP_OK) {
				return r;
			}
			if (osp_buf_free(buf) < slen) {
				return OSP_ERR_NOMEM;
			}
			memcpy(&buf->buf[buf->wr], val->as.visiblestring.data, slen);
			buf->wr += slen;
			return OSP_OK;
		}

		case OSP_TAG_BITSTRING: {
			uint32_t num_bits = val->as.bitstring.num_bits;
			uint32_t num_bytes = (num_bits + 7) / 8;
			uint8_t unused = (uint8_t)((num_bytes * 8) - num_bits);
			r = osp_ber_write_length(buf, 1 + num_bytes);
			if (r != OSP_OK) return r;
			if (osp_buf_free(buf) < 1 + num_bytes) return OSP_ERR_NOMEM;
			buf->buf[buf->wr++] = unused;
			if (num_bytes > 0) {
				memcpy(&buf->buf[buf->wr], val->as.bitstring.bits, num_bytes);
				buf->wr += num_bytes;
			}
			return OSP_OK;
		}

		case OSP_TAG_UTF8STRING: {
			uint32_t slen = val->as.utf8string.len;
			r = osp_ber_write_length(buf, slen);
			if (r != OSP_OK) return r;
			if (osp_buf_free(buf) < slen) return OSP_ERR_NOMEM;
			memcpy(&buf->buf[buf->wr], val->as.utf8string.data, slen);
			buf->wr += slen;
			return OSP_OK;
		}

		case OSP_TAG_BCD:
			return osp_axdr_write_u8(buf, val->as.bcd.value);

		case OSP_TAG_DATE:
			return osp_date_write(buf, &val->as.date);

		case OSP_TAG_TIME:
			return osp_time_write(buf, &val->as.time);

		case OSP_TAG_DATETIME:
			return osp_datetime_write(buf, &val->as.datetime);

		case OSP_TAG_ARRAY:
		case OSP_TAG_STRUCTURE: {
			const osp_value_list_t *list = (val->tag == OSP_TAG_ARRAY) ? &val->as.array.elements : &val->as.structure.elements;
			r = osp_ber_write_length(buf, list->count);
			if (r != OSP_OK) {
				return r;
			}
			for (uint8_t i = 0; i < list->count; i++) {
				r = osp_value_write(buf, &list->items[i]);
				if (r != OSP_OK) {
					return r;
				}
			}
			return OSP_OK;
		}

		case OSP_TAG_OBJECT_LIST_REF: {
			/* Undo the placeholder tag byte written above; emit a real ARRAY. */
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_object_list_write(buf, (const osp_object_list_t *)val->as.ref);
		}

		case OSP_TAG_PROFILE_BUFFER_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_profile_buffer_write(buf, (const osp_profile_buffer_view_t *)val->as.ref);
		}

		case OSP_TAG_CAPTURE_OBJECT_LIST_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_capture_object_list_write(buf, (const osp_capture_object_list_t *)val->as.ref);
		}

		case OSP_TAG_DAY_PROFILE_TABLE_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_day_profile_table_write(buf, (const osp_day_profile_table_view_t *)val->as.ref);
		}

		case OSP_TAG_SEASON_PROFILE_TABLE_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_season_profile_table_write(buf, (const osp_season_profile_table_view_t *)val->as.ref);
		}

		case OSP_TAG_WEEK_PROFILE_TABLE_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_week_profile_table_write(buf, (const osp_week_profile_table_view_t *)val->as.ref);
		}

		case OSP_TAG_PUSH_OBJECT_LIST_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_push_object_list_write(buf, (const osp_push_object_list_view_t *)val->as.ref);
		}

		case OSP_TAG_COMM_WINDOW_LIST_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_comm_window_list_write(buf, (const osp_comm_window_list_view_t *)val->as.ref);
		}

		case OSP_TAG_THRESHOLD_LIST_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_threshold_list_write(buf, (const osp_threshold_list_t *)val->as.ref);
		}

		case OSP_TAG_SAP_ASSIGNMENT_LIST_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_sap_assignment_list_write(buf, (const osp_sap_assignment_list_t *)val->as.ref);
		}

		case OSP_TAG_STATUS_MAPPING_TABLE_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_status_mapping_table_write(buf, (const osp_status_mapping_table_view_t *)val->as.ref);
		}

		case OSP_TAG_EXECUTION_TIME_LIST_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_execution_time_list_write(buf, (const osp_execution_time_list_view_t *)val->as.ref);
		}

		case OSP_TAG_DATA_PROTECTION_LIST_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_data_protection_list_write(buf, (const osp_data_protection_list_t *)val->as.ref);
		}

		case OSP_TAG_USER_LIST_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_user_list_write(buf, (const osp_user_list_view_t *)val->as.ref);
		}

		case OSP_TAG_U16_ARRAY_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_u16_array_write(buf, (const osp_u16_array_view_t *)val->as.ref);
		}

		case OSP_TAG_U32_ARRAY_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_u32_array_write(buf, (const osp_u32_array_view_t *)val->as.ref);
		}

		case OSP_TAG_SPECIAL_DAYS_LIST_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_special_days_list_write(buf, (const osp_special_days_list_view_t *)val->as.ref);
		}

		case OSP_TAG_SCHEDULE_LIST_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_schedule_list_write(buf, (const osp_schedule_list_view_t *)val->as.ref);
		}

		case OSP_TAG_SCRIPT_LIST_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_script_list_write(buf, (const osp_script_list_view_t *)val->as.ref);
		}

		case OSP_TAG_MASK_LIST_REF: {
			if (buf->wr == 0) {
				return OSP_ERR_INVALID;
			}
			buf->wr--;
			return osp_mask_list_write(buf, (const osp_mask_list_t *)val->as.ref);
		}

		default:
			return OSP_ERR_UNSUPPORTED;
	}
}

osp_err_t osp_value_skip(osp_buf_t *buf) {
	osp_value_t dummy;
	return osp_value_read(buf, &dummy);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  COMMON STRUCTURE HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_obis_read(osp_buf_t *buf, osp_obis_t *obis) {
	if (!buf || !obis) {
		return OSP_ERR_INVALID;
	}
	/* OBIS encoded as 6 octets (no tag, no length — inline) */
	if (osp_buf_unread(buf) < 6) {
		return OSP_ERR_INVALID;
	}
	obis->a = buf->buf[buf->rd++];
	obis->b = buf->buf[buf->rd++];
	obis->c = buf->buf[buf->rd++];
	obis->d = buf->buf[buf->rd++];
	obis->e = buf->buf[buf->rd++];
	obis->f = buf->buf[buf->rd++];
	return OSP_OK;
}

osp_err_t osp_obis_write(osp_buf_t *buf, const osp_obis_t *obis) {
	if (!buf || !obis || osp_buf_free(buf) < 6) {
		return OSP_ERR_INVALID;
	}
	buf->buf[buf->wr++] = obis->a;
	buf->buf[buf->wr++] = obis->b;
	buf->buf[buf->wr++] = obis->c;
	buf->buf[buf->wr++] = obis->d;
	buf->buf[buf->wr++] = obis->e;
	buf->buf[buf->wr++] = obis->f;
	return OSP_OK;
}

/* Access right: structure { attribute_access, method_access } — tagged A-XDR */
osp_err_t osp_access_right_read(osp_buf_t *buf, osp_access_right_t *ar) {
	if (!buf || !ar) {
		return OSP_ERR_INVALID;
	}
	uint8_t nf;
	osp_err_t r = osp_struct_begin_read(buf, &nf);
	if (r != OSP_OK) {
		return r;
	}
	if (nf < 2) {
		return OSP_ERR_INVALID;
	}

	uint8_t acount;
	r = osp_array_begin_read(buf, &acount);
	if (r != OSP_OK) {
		return r;
	}
	uint8_t keep_a = acount;
	if (keep_a > OSP_MAX_ACCESS_ITEMS) {
		keep_a = OSP_MAX_ACCESS_ITEMS;
	}
	ar->attr_count = keep_a;
	for (uint8_t i = 0; i < acount; i++) {
		uint8_t nf2;
		r = osp_struct_begin_read(buf, &nf2);
		if (r != OSP_OK) {
			return r;
		}
		osp_value_t idv = {0};
		osp_value_t amv = {0};
		if ((r = osp_value_read(buf, &idv)) != OSP_OK) {
			return r;
		}
		if ((r = osp_value_read(buf, &amv)) != OSP_OK) {
			return r;
		}
		/* access_selectors CHOICE */
		if ((r = osp_value_skip(buf)) != OSP_OK) {
			return r;
		}
		if (i < keep_a) {
			ar->attr_items[i].attribute_id = osp_get_i8(&idv);
			ar->attr_items[i].access_mode = (osp_attr_access_t)osp_get_enum(&amv);
		}
	}

	uint8_t mcount;
	r = osp_array_begin_read(buf, &mcount);
	if (r != OSP_OK) {
		return r;
	}
	uint8_t keep_m = mcount;
	if (keep_m > OSP_MAX_METHOD_ITEMS) {
		keep_m = OSP_MAX_METHOD_ITEMS;
	}
	ar->method_count = keep_m;
	for (uint8_t i = 0; i < mcount; i++) {
		uint8_t nf2;
		r = osp_struct_begin_read(buf, &nf2);
		if (r != OSP_OK) {
			return r;
		}
		osp_value_t idv = {0};
		osp_value_t mmv = {0};
		if ((r = osp_value_read(buf, &idv)) != OSP_OK) {
			return r;
		}
		if ((r = osp_value_read(buf, &mmv)) != OSP_OK) {
			return r;
		}
		if (i < keep_m) {
			ar->method_items[i].method_id = osp_get_i8(&idv);
			ar->method_items[i].access_mode = (osp_method_access_t)osp_get_enum(&mmv);
		}
	}
	return OSP_OK;
}

osp_err_t osp_access_right_write(osp_buf_t *buf, const osp_access_right_t *ar) {
	if (!buf || !ar) {
		return OSP_ERR_INVALID;
	}
	uint8_t ac = ar->attr_count;
	uint8_t mc = ar->method_count;
	if (ac > OSP_MAX_ACCESS_ITEMS) {
		ac = OSP_MAX_ACCESS_ITEMS;
	}
	if (mc > OSP_MAX_METHOD_ITEMS) {
		mc = OSP_MAX_METHOD_ITEMS;
	}

	osp_err_t r = osp_struct_begin(buf, 2);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_array_begin(buf, ac);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < ac; i++) {
		r = osp_struct_begin(buf, 3);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_INTEGER);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_i8(buf, ar->attr_items[i].attribute_id);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_ENUM);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, (uint8_t)ar->attr_items[i].access_mode);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_NULL);
		if (r != OSP_OK) {
			return r;
		}
	}

	r = osp_array_begin(buf, mc);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < mc; i++) {
		r = osp_struct_begin(buf, 2);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_INTEGER);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_i8(buf, ar->method_items[i].method_id);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_ENUM);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, (uint8_t)ar->method_items[i].access_mode);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

/* Object list element: { class_id, version, logical_name, access_rights } */
osp_err_t osp_object_list_element_read(osp_buf_t *buf, osp_object_list_element_t *elem) {
	uint8_t nf;
	osp_err_t r = osp_struct_begin_read(buf, &nf);
	if (r != OSP_OK) {
		return r;
	}
	if ((r = osp_axdr_read_u16(buf, &elem->class_id)) != OSP_OK)
		return r;
	if ((r = osp_axdr_read_u8(buf, &elem->version)) != OSP_OK)
		return r;
	if ((r = osp_obis_read(buf, &elem->logical_name)) != OSP_OK)
		return r;
	if ((r = osp_access_right_read(buf, &elem->access_rights)) != OSP_OK)
		return r;
	return OSP_OK;
}

osp_err_t osp_object_list_element_write(osp_buf_t *buf, const osp_object_list_element_t *elem) {
	osp_err_t r;
	if ((r = osp_struct_begin(buf, 4)) != OSP_OK)
		return r;
	if ((r = osp_axdr_write_u16(buf, elem->class_id)) != OSP_OK)
		return r;
	if ((r = osp_axdr_write_u8(buf, elem->version)) != OSP_OK)
		return r;
	if ((r = osp_obis_write(buf, &elem->logical_name)) != OSP_OK)
		return r;
	if ((r = osp_access_right_write(buf, &elem->access_rights)) != OSP_OK)
		return r;
	return OSP_OK;
}

osp_err_t osp_object_list_write(osp_buf_t *buf, const osp_object_list_t *ol) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint16_t n = ol ? ol->count : 0;
	if (n > OSP_MAX_OBJECT_LIST) {
		n = OSP_MAX_OBJECT_LIST;
	}
	if (n > 255) {
		n = 255;
	}
	osp_err_t r = osp_array_begin(buf, (uint8_t)n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint16_t i = 0; i < n; i++) {
		const osp_object_list_element_t *e = &ol->elements[i];
		r = osp_struct_begin(buf, 4);
		if (r != OSP_OK) {
			return r;
		}
		/* class_id: long-unsigned */
		r = osp_axdr_write_u8(buf, OSP_TAG_LONG_UNSIGNED);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u16(buf, e->class_id);
		if (r != OSP_OK) {
			return r;
		}
		/* version: unsigned */
		r = osp_axdr_write_u8(buf, OSP_TAG_UNSIGNED);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, e->version);
		if (r != OSP_OK) {
			return r;
		}
		/* logical_name: octet-string(6) */
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		{
			uint8_t ln[6] = {e->logical_name.a, e->logical_name.b, e->logical_name.c,
			                 e->logical_name.d, e->logical_name.e, e->logical_name.f};
			r = osp_axdr_write_octet_string(buf, ln, 6);
			if (r != OSP_OK) {
				return r;
			}
		}
		r = osp_access_right_write(buf, &e->access_rights);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

osp_err_t osp_profile_buffer_write(osp_buf_t *buf, const osp_profile_buffer_view_t *view) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	const osp_profile_buffer_t *pb = view ? view->buf : NULL;
	uint8_t n = pb ? pb->row_count : 0;
	if (n > OSP_MAX_BUFFER_ROWS) {
		n = OSP_MAX_BUFFER_ROWS;
	}
	uint32_t from = view ? view->from_entry : 0;
	uint32_t to = view ? view->to_entry : 0;
	uint8_t out_n = 0;
	uint8_t idx[OSP_MAX_BUFFER_ROWS];
	for (uint8_t i = 0; i < n; i++) {
		uint32_t entry = (uint32_t)i + 1;
		if (from != 0 && entry < from) {
			continue;
		}
		if (to != 0 && entry > to) {
			continue;
		}
		idx[out_n++] = i;
	}
	osp_err_t r = osp_array_begin(buf, out_n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t k = 0; k < out_n; k++) {
		const osp_profile_row_t *row = &pb->rows[idx[k]];
		uint8_t nc = row->cell_count;
		if (nc > OSP_MAX_CAPTURE_OBJECTS) {
			nc = OSP_MAX_CAPTURE_OBJECTS;
		}
		r = osp_struct_begin(buf, nc);
		if (r != OSP_OK) {
			return r;
		}
		for (uint8_t j = 0; j < nc; j++) {
			r = osp_value_write(buf, &row->cells[j]);
			if (r != OSP_OK) {
				return r;
			}
		}
	}
	return OSP_OK;
}

osp_err_t osp_capture_object_list_write(osp_buf_t *buf, const osp_capture_object_list_t *list) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = list ? list->count : 0;
	if (n > OSP_MAX_CAPTURE_OBJECTS) {
		n = OSP_MAX_CAPTURE_OBJECTS;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_capture_object_t *co = &list->items[i];
		r = osp_struct_begin(buf, 4);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_LONG_UNSIGNED);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u16(buf, co->class_id);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		{
			uint8_t ln[6] = {co->logical_name.a, co->logical_name.b, co->logical_name.c,
			                 co->logical_name.d, co->logical_name.e, co->logical_name.f};
			r = osp_axdr_write_octet_string(buf, ln, 6);
			if (r != OSP_OK) {
				return r;
			}
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_INTEGER);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_i8(buf, co->attribute_index);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_LONG_UNSIGNED);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u16(buf, (uint16_t)co->data_index);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

osp_err_t osp_day_profile_table_write(osp_buf_t *buf, const osp_day_profile_table_view_t *view) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = (view && view->profiles) ? view->count : 0;
	if (n > OSP_MAX_DAY_PROFILE) {
		n = OSP_MAX_DAY_PROFILE;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_day_profile_t *dp = &view->profiles[i];
		uint8_t day_id = 1;
		if (dp->name_len > 0) {
			day_id = (uint8_t)dp->name[0];
		}
		uint8_t na = dp->action_count;
		if (na > OSP_MAX_DAY_ACTION) {
			na = OSP_MAX_DAY_ACTION;
		}
		r = osp_struct_begin(buf, 2);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_UNSIGNED);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, day_id);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_array_begin(buf, na);
		if (r != OSP_OK) {
			return r;
		}
		for (uint8_t j = 0; j < na; j++) {
			const osp_day_profile_action_t *act = &dp->actions[j];
			uint8_t script_ln[6] = {0, 0, 10, 0, 100, 255};
			uint16_t selector = 1;
			if (act->script_count > 0) {
				selector = (uint16_t)act->scripts[0].script_selector;
			}
			{
				bool ln_set = false;
				for (int b = 0; b < 6; b++) {
					if (act->script_logical_name[b] != 0) {
						ln_set = true;
						break;
					}
				}
				if (ln_set) {
					memcpy(script_ln, act->script_logical_name, 6);
				}
			}
			r = osp_struct_begin(buf, 3);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_octet_string(buf, act->time, 4);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_octet_string(buf, script_ln, 6);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_u8(buf, OSP_TAG_LONG_UNSIGNED);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_u16(buf, selector);
			if (r != OSP_OK) {
				return r;
			}
		}
	}
	return OSP_OK;
}

osp_err_t osp_season_profile_table_write(osp_buf_t *buf, const osp_season_profile_table_view_t *view) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = (view && view->seasons) ? view->count : 0;
	if (n > OSP_MAX_SEASON_PROFILE) {
		n = OSP_MAX_SEASON_PROFILE;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_season_t *s = &view->seasons[i];
		r = osp_struct_begin(buf, 3);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_octet_string(buf, (const uint8_t *)s->name, s->name_len);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_octet_string(buf, s->start, OSP_COSEM_DATETIME_LEN);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_octet_string(buf, (const uint8_t *)s->week_name, s->week_name_len);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

osp_err_t osp_week_profile_table_write(osp_buf_t *buf, const osp_week_profile_table_view_t *view) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = (view && view->weeks) ? view->count : 0;
	if (n > OSP_MAX_WEEK_PROFILE) {
		n = OSP_MAX_WEEK_PROFILE;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_week_profile_t *wp = &view->weeks[i];
		r = osp_struct_begin(buf, 8);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_octet_string(buf, (const uint8_t *)wp->name, wp->name_len);
		if (r != OSP_OK) {
			return r;
		}
		for (int d = 0; d < 7; d++) {
			r = osp_axdr_write_u8(buf, OSP_TAG_UNSIGNED);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_u8(buf, wp->day_names[d][0]);
			if (r != OSP_OK) {
				return r;
			}
		}
	}
	return OSP_OK;
}

osp_err_t osp_special_days_list_write(osp_buf_t *buf, const osp_special_days_list_view_t *view) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = (view && view->entries) ? view->count : 0;
	if (n > 32) {
		n = 32;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_special_day_t *e = &view->entries[i];
		r = osp_struct_begin(buf, 2);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_DOUBLE_LONG_UNS);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u32(buf, e->day_id);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_octet_string(buf, e->date, 5);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

osp_err_t osp_schedule_list_write(osp_buf_t *buf, const osp_schedule_list_view_t *view) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = (view && view->entries) ? view->count : 0;
	if (n > OSP_MAX_SCHEDULE_ENTRY) {
		n = OSP_MAX_SCHEDULE_ENTRY;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_schedule_entry_t *e = &view->entries[i];
		uint8_t sn = e->script_count;
		if (sn > OSP_MAX_SCRIPT_PER_ACTION) {
			sn = OSP_MAX_SCRIPT_PER_ACTION;
		}
		r = osp_struct_begin(buf, 4);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_BOOLEAN);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, e->enable ? 1 : 0);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_octet_string(buf, e->start_time, 4);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_octet_string(buf, e->end_time, 4);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_array_begin(buf, sn);
		if (r != OSP_OK) {
			return r;
		}
		for (uint8_t j = 0; j < sn; j++) {
			r = osp_struct_begin(buf, 2);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_u8(buf, OSP_TAG_DOUBLE_LONG_UNS);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_u32(buf, e->scripts[j].script_id);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_u8(buf, OSP_TAG_DOUBLE_LONG);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_i32(buf, e->scripts[j].script_selector);
			if (r != OSP_OK) {
				return r;
			}
		}
	}
	return OSP_OK;
}

osp_err_t osp_script_list_write(osp_buf_t *buf, const osp_script_list_view_t *view) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = (view && view->scripts) ? view->count : 0;
	if (n > OSP_MAX_SCRIPTS) {
		n = OSP_MAX_SCRIPTS;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_script_t *sc = &view->scripts[i];
		uint8_t an = sc->action_count;
		if (an > OSP_MAX_SCRIPT_ACTIONS) {
			an = OSP_MAX_SCRIPT_ACTIONS;
		}
		r = osp_struct_begin(buf, 2);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_DOUBLE_LONG_UNS);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u32(buf, sc->script_id);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_array_begin(buf, an);
		if (r != OSP_OK) {
			return r;
		}
		for (uint8_t j = 0; j < an; j++) {
			const osp_script_action_item_t *ai = &sc->actions[j];
			uint8_t ln[6] = {ai->logical_name.a, ai->logical_name.b, ai->logical_name.c,
			                 ai->logical_name.d, ai->logical_name.e, ai->logical_name.f};
			r = osp_struct_begin(buf, 4);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_u8(buf, OSP_TAG_LONG_UNSIGNED);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_u16(buf, ai->class_id);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_octet_string(buf, ln, 6);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_u8(buf, OSP_TAG_INTEGER);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_i8(buf, ai->method_id);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_value_write(buf, &ai->method_param);
			if (r != OSP_OK) {
				return r;
			}
		}
	}
	return OSP_OK;
}

osp_err_t osp_mask_list_write(osp_buf_t *buf, const osp_mask_list_t *list) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = list ? list->count : 0;
	if (n > OSP_MAX_MASK_LIST) {
		n = OSP_MAX_MASK_LIST;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_activation_mask_t *m = &list->items[i];
		uint8_t idx_n = m->register_list.count;
		if (idx_n > OSP_MAX_MASK_REGISTERS) {
			idx_n = OSP_MAX_MASK_REGISTERS;
		}
		r = osp_struct_begin(buf, 2);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_octet_string(buf, (const uint8_t *)m->mask_name, m->mask_name_len);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_array_begin(buf, idx_n);
		if (r != OSP_OK) {
			return r;
		}
		for (uint8_t j = 0; j < idx_n; j++) {
			r = osp_axdr_write_u8(buf, OSP_TAG_UNSIGNED);
			if (r != OSP_OK) {
				return r;
			}
			r = osp_axdr_write_u8(buf, (uint8_t)m->register_list.indices[j]);
			if (r != OSP_OK) {
				return r;
			}
		}
	}
	return OSP_OK;
}

osp_err_t osp_push_object_list_write(osp_buf_t *buf, const osp_push_object_list_view_t *view) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = (view && view->items) ? view->count : 0;
	if (n > OSP_MAX_PUSH_OBJECTS) {
		n = OSP_MAX_PUSH_OBJECTS;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_push_object_t *po = &view->items[i];
		r = osp_struct_begin(buf, 4);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_LONG_UNSIGNED);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u16(buf, po->class_id);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		{
			uint8_t ln[6] = {po->logical_name.a, po->logical_name.b, po->logical_name.c,
			                 po->logical_name.d, po->logical_name.e, po->logical_name.f};
			r = osp_axdr_write_octet_string(buf, ln, 6);
			if (r != OSP_OK) {
				return r;
			}
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_INTEGER);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_i8(buf, po->attribute_index);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_LONG_UNSIGNED);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u16(buf, po->data_index);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

osp_err_t osp_comm_window_list_write(osp_buf_t *buf, const osp_comm_window_list_view_t *view) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = (view && view->windows) ? view->count : 0;
	if (n > OSP_MAX_COMM_WINDOW) {
		n = OSP_MAX_COMM_WINDOW;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_comm_window_t *w = &view->windows[i];
		r = osp_struct_begin(buf, 2);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_octet_string(buf, w->start, OSP_COSEM_DATETIME_LEN);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_octet_string(buf, w->end, OSP_COSEM_DATETIME_LEN);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

osp_err_t osp_threshold_list_write(osp_buf_t *buf, const osp_threshold_list_t *tl) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = tl ? tl->count : 0;
	if (n > 16) {
		n = 16;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		r = osp_struct_begin(buf, 2);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_value_write(buf, &tl->thresholds[i].value);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_ENUM);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, (uint8_t)tl->thresholds[i].severity);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

osp_err_t osp_sap_assignment_list_write(osp_buf_t *buf, const osp_sap_assignment_list_t *list) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = list ? list->count : 0;
	if (n > OSP_MAX_SAP_ASSIGNMENTS) {
		n = OSP_MAX_SAP_ASSIGNMENTS;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_sap_assignment_item_t *item = &list->items[i];
		r = osp_struct_begin(buf, 2);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_LONG_UNSIGNED);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u16(buf, item->sap);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_octet_string(buf, item->logical_device_name, item->logical_device_name_len);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

osp_err_t osp_status_mapping_table_write(osp_buf_t *buf, const osp_status_mapping_table_view_t *view) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = (view && view->entries) ? view->count : 0;
	if (n > OSP_MAX_STATUS_MAPPINGS) {
		n = OSP_MAX_STATUS_MAPPINGS;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_status_mapping_entry_t *e = &view->entries[i];
		r = osp_struct_begin(buf, 2);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_UNSIGNED);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, e->status_flag_id);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_octet_string(buf, e->status_reference, 6);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

osp_err_t osp_execution_time_list_write(osp_buf_t *buf, const osp_execution_time_list_view_t *view) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = (view && view->items) ? view->count : 0;
	if (n > OSP_MAX_EXECUTION_TIMES) {
		n = OSP_MAX_EXECUTION_TIMES;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_execution_time_date_t *et = &view->items[i];
		r = osp_struct_begin(buf, 2);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_octet_string(buf, et->time, 4);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_octet_string(buf, et->date, 5);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

osp_err_t osp_data_protection_list_write(osp_buf_t *buf, const osp_data_protection_list_t *list) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = list ? list->count : 0;
	if (n > 8) {
		n = 8;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_data_protection_entry_t *e = &list->entries[i];
		r = osp_struct_begin(buf, 2);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_ENUM);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, (uint8_t)e->method);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_UNSIGNED);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, e->global_key_list_id);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

osp_err_t osp_user_list_write(osp_buf_t *buf, const osp_user_list_view_t *view) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = (view && view->items) ? view->count : 0;
	if (n > 16) {
		n = 16;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		const osp_user_list_item_t *item = &view->items[i];
		r = osp_struct_begin(buf, 2);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, OSP_TAG_INTEGER);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_i8(buf, item->id);
		if (r != OSP_OK) {
			return r;
		}
		/* osp_axdr_write_visible_string includes the VISIBLESTRING tag. */
		r = osp_axdr_write_visible_string(buf, item->name, item->name_len);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

osp_err_t osp_u16_array_write(osp_buf_t *buf, const osp_u16_array_view_t *view) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = (view && view->items) ? view->count : 0;
	if (view && view->max_count && n > view->max_count) {
		n = view->max_count;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		r = osp_axdr_write_u8(buf, OSP_TAG_LONG_UNSIGNED);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u16(buf, view->items[i]);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

osp_err_t osp_u32_array_write(osp_buf_t *buf, const osp_u32_array_view_t *view) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = (view && view->items) ? view->count : 0;
	if (view && view->max_count && n > view->max_count) {
		n = view->max_count;
	}
	osp_err_t r = osp_array_begin(buf, n);
	if (r != OSP_OK) {
		return r;
	}
	for (uint8_t i = 0; i < n; i++) {
		r = osp_axdr_write_u8(buf, OSP_TAG_DOUBLE_LONG_UNS);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u32(buf, view->items[i]);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

/* Capture object definition */
osp_err_t osp_capture_object_read(osp_buf_t *buf, osp_capture_object_t *co) {
	osp_err_t r;
	uint8_t nf;
	if ((r = osp_struct_begin_read(buf, &nf)) != OSP_OK) return r;
	if ((r = osp_axdr_read_u16(buf, &co->class_id)) != OSP_OK) return r;
	if ((r = osp_obis_read(buf, &co->logical_name)) != OSP_OK) return r;
	if ((r = osp_axdr_read_i8(buf, &co->attribute_index)) != OSP_OK) return r;
	if ((r = osp_axdr_read_u32(buf, &co->data_index)) != OSP_OK) return r;
	/* restriction_element: CHOICE — read as value */
	if ((r = osp_value_skip(buf)) != OSP_OK) return r;
	return OSP_OK;
}

osp_err_t osp_capture_object_write(osp_buf_t *buf, const osp_capture_object_t *co) {
	osp_err_t r;
	if ((r = osp_struct_begin(buf, 5)) != OSP_OK) return r;
	if ((r = osp_axdr_write_u16(buf, co->class_id)) != OSP_OK) return r;
	if ((r = osp_obis_write(buf, &co->logical_name)) != OSP_OK) return r;
	if ((r = osp_axdr_write_i8(buf, co->attribute_index)) != OSP_OK) return r;
	if ((r = osp_axdr_write_u32(buf, co->data_index)) != OSP_OK) return r;
	if ((r = osp_axdr_write_u8(buf, OSP_AXDR_NULL)) != OSP_OK) return r;
	return OSP_OK;
}

/* Value definition: { class_id, logical_name, attribute_index } */
osp_err_t osp_value_definition_read(osp_buf_t *buf, osp_value_definition_t *vd) {
	osp_err_t r;
	uint8_t nf;
	if ((r = osp_struct_begin_read(buf, &nf)) != OSP_OK) return r;
	if ((r = osp_axdr_read_u16(buf, &vd->class_id)) != OSP_OK) return r;
	if ((r = osp_obis_read(buf, &vd->logical_name)) != OSP_OK) return r;
	if ((r = osp_axdr_read_i8(buf, &vd->attribute_index)) != OSP_OK) return r;
	return OSP_OK;
}

osp_err_t osp_value_definition_write(osp_buf_t *buf, const osp_value_definition_t *vd) {
	osp_err_t r;
	if ((r = osp_struct_begin(buf, 3)) != OSP_OK) return r;
	if ((r = osp_axdr_write_u16(buf, vd->class_id)) != OSP_OK) return r;
	if ((r = osp_obis_write(buf, &vd->logical_name)) != OSP_OK) return r;
	if ((r = osp_axdr_write_i8(buf, vd->attribute_index)) != OSP_OK) return r;
	return OSP_OK;
}

/* Scaler unit: { scaler, unit } */
osp_err_t osp_scaler_unit_read(osp_buf_t *buf, osp_scaler_unit_t *su) {
	osp_err_t r;
	uint8_t nf;
	if ((r = osp_struct_begin_read(buf, &nf)) != OSP_OK) return r;
	if ((r = osp_axdr_read_i8(buf, &su->scaler)) != OSP_OK) return r;
	if ((r = osp_axdr_read_u8(buf, &su->unit)) != OSP_OK) return r;
	return OSP_OK;
}

osp_err_t osp_scaler_unit_write(osp_buf_t *buf, const osp_scaler_unit_t *su) {
	osp_err_t r;
	if ((r = osp_struct_begin(buf, 2)) != OSP_OK) return r;
	if ((r = osp_axdr_write_i8(buf, su->scaler)) != OSP_OK) return r;
	if ((r = osp_axdr_write_u8(buf, su->unit)) != OSP_OK) return r;
	return OSP_OK;
}

/* User list item: { id, name } */
osp_err_t osp_user_list_item_read(osp_buf_t *buf, osp_user_list_item_t *item) {
	osp_err_t r;
	uint8_t nf;
	if ((r = osp_struct_begin_read(buf, &nf)) != OSP_OK) return r;
	if ((r = osp_axdr_read_i8(buf, &item->id)) != OSP_OK) return r;
	uint32_t len = 0;
	if ((r = osp_axdr_read_visible_string(buf, item->name, OSP_MAX_NAME_LEN, &len)) != OSP_OK) return r;
	item->name_len = (uint8_t)len;
	return OSP_OK;
}

osp_err_t osp_user_list_item_write(osp_buf_t *buf, const osp_user_list_item_t *item) {
	osp_err_t r;
	if ((r = osp_struct_begin(buf, 2)) != OSP_OK) return r;
	if ((r = osp_axdr_write_i8(buf, item->id)) != OSP_OK) return r;
	if ((r = osp_axdr_write_visible_string(buf, item->name, item->name_len)) != OSP_OK) return r;
	return OSP_OK;
}

/* Attribute descriptor: { class_id, instance_id, attribute_id } */
osp_err_t osp_attribute_descriptor_read(osp_buf_t *buf, osp_attribute_descriptor_t *ad) {
	osp_err_t r;
	uint8_t nf;
	if ((r = osp_struct_begin_read(buf, &nf)) != OSP_OK) return r;
	if ((r = osp_axdr_read_u16(buf, &ad->class_id)) != OSP_OK) return r;
	if ((r = osp_obis_read(buf, &ad->instance_id)) != OSP_OK) return r;
	if ((r = osp_axdr_read_i8(buf, &ad->attribute_id)) != OSP_OK) return r;
	return OSP_OK;
}

osp_err_t osp_attribute_descriptor_write(osp_buf_t *buf, const osp_attribute_descriptor_t *ad) {
	osp_err_t r;
	if ((r = osp_struct_begin(buf, 3)) != OSP_OK) return r;
	if ((r = osp_axdr_write_u16(buf, ad->class_id)) != OSP_OK) return r;
	if ((r = osp_obis_write(buf, &ad->instance_id)) != OSP_OK) return r;
	if ((r = osp_axdr_write_i8(buf, ad->attribute_id)) != OSP_OK) return r;
	return OSP_OK;
}
