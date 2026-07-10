/**
 * serialize.c — AXDR serialize/deserialize for COSEM types
 *
 * Generic osp_value_read/write handles any type by tag.
 * Typed helpers for date/time, access_right, etc.
 */

#include "serialize.h"
#include <string.h>

/* Bump pool for nested structure/array elements during osp_value_read (no malloc). */
static osp_value_t value_read_pool[OSP_MAX_ARRAY_LEN * OSP_MAX_STRUCT_LEN];
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
	*val = ((int64_t)buf->buf[buf->rd] << 56) | ((int64_t)buf->buf[buf->rd + 1] << 48) | ((int64_t)buf->buf[buf->rd + 2] << 40) |
	    ((int64_t)buf->buf[buf->rd + 3] << 32) | ((int64_t)buf->buf[buf->rd + 4] << 24) | ((int64_t)buf->buf[buf->rd + 5] << 16) |
	    ((int64_t)buf->buf[buf->rd + 6] << 8) | (int64_t)buf->buf[buf->rd + 7];
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

static osp_err_t skip_type_description(osp_buf_t *buf) {
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
			r = skip_type_description(buf);
			if (r != OSP_OK) {
				return r;
			}
		}
	} else if (tag == OSP_TAG_ARRAY) {
		if (osp_buf_unread(buf) < 2) {
			return OSP_ERR_INVALID;
		}
		buf->rd += 2;
		r = skip_type_description(buf);
	}
	return r;
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
	if (value_read_depth == 0) {
		value_read_pool_used = 0;
	}
	value_read_depth++;
	osp_err_t r = osp_value_read_impl(buf, val);
	value_read_depth--;
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

/* Access right: structure { attribute_access, method_access } */
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

	/* attribute_access_descriptor: array of attribute_access_item */
	uint8_t acount;
	r = osp_array_begin_read(buf, &acount);
	if (r != OSP_OK) {
		return r;
	}
	if (acount > OSP_MAX_ACCESS_ITEMS) {
		acount = OSP_MAX_ACCESS_ITEMS;
	}
	ar->attr_count = acount;
	for (uint8_t i = 0; i < acount; i++) {
		/* attribute_access_item: { attribute_id, access_mode, access_selectors } */
		uint8_t nf2;
		r = osp_struct_begin_read(buf, &nf2);
		if (r != OSP_OK) {
			return r;
		}
		osp_axdr_read_i8(buf, &ar->attr_items[i].attribute_id);
		uint8_t am;
		osp_axdr_read_u8(buf, &am);
		ar->attr_items[i].access_mode = (osp_attr_access_t)am;
		/* access_selectors: CHOICE — skip for now */
		osp_value_skip(buf);
	}

	/* method_access_descriptor: array of method_access_item */
	uint8_t mcount;
	r = osp_array_begin_read(buf, &mcount);
	if (r != OSP_OK) {
		return r;
	}
	if (mcount > OSP_MAX_METHOD_ITEMS) {
		mcount = OSP_MAX_METHOD_ITEMS;
	}
	ar->method_count = mcount;
	for (uint8_t i = 0; i < mcount; i++) {
		uint8_t nf2;
		r = osp_struct_begin_read(buf, &nf2);
		if (r != OSP_OK) {
			return r;
		}
		osp_axdr_read_i8(buf, &ar->method_items[i].method_id);
		uint8_t mm;
		osp_axdr_read_u8(buf, &mm);
		ar->method_items[i].access_mode = (osp_method_access_t)mm;
	}
	return OSP_OK;
}

osp_err_t osp_access_right_write(osp_buf_t *buf, const osp_access_right_t *ar) {
	if (!buf || !ar) {
		return OSP_ERR_INVALID;
	}
	osp_struct_begin(buf, 2);

	osp_array_begin(buf, ar->attr_count);
	for (uint8_t i = 0; i < ar->attr_count; i++) {
		osp_struct_begin(buf, 3);
		osp_axdr_write_i8(buf, ar->attr_items[i].attribute_id);
		osp_axdr_write_u8(buf, (uint8_t)ar->attr_items[i].access_mode);
		osp_axdr_write_u8(buf, OSP_AXDR_NULL); /* access_selectors = null-data */
	}

	osp_array_begin(buf, ar->method_count);
	for (uint8_t i = 0; i < ar->method_count; i++) {
		osp_struct_begin(buf, 2);
		osp_axdr_write_i8(buf, ar->method_items[i].method_id);
		osp_axdr_write_u8(buf, (uint8_t)ar->method_items[i].access_mode);
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
	osp_axdr_read_u16(buf, &elem->class_id);
	osp_axdr_read_u8(buf, &elem->version);
	osp_obis_read(buf, &elem->logical_name);
	osp_access_right_read(buf, &elem->access_rights);
	return OSP_OK;
}

osp_err_t osp_object_list_element_write(osp_buf_t *buf, const osp_object_list_element_t *elem) {
	osp_struct_begin(buf, 4);
	osp_axdr_write_u16(buf, elem->class_id);
	osp_axdr_write_u8(buf, elem->version);
	osp_obis_write(buf, &elem->logical_name);
	osp_access_right_write(buf, &elem->access_rights);
	return OSP_OK;
}

/* Capture object definition */
osp_err_t osp_capture_object_read(osp_buf_t *buf, osp_capture_object_t *co) {
	uint8_t nf;
	osp_struct_begin_read(buf, &nf);
	osp_axdr_read_u16(buf, &co->class_id);
	osp_obis_read(buf, &co->logical_name);
	osp_axdr_read_i8(buf, &co->attribute_index);
	osp_axdr_read_u32(buf, &co->data_index);
	/* restriction_element: CHOICE — read as value */
	osp_value_skip(buf);
	return OSP_OK;
}

osp_err_t osp_capture_object_write(osp_buf_t *buf, const osp_capture_object_t *co) {
	osp_struct_begin(buf, 5);
	osp_axdr_write_u16(buf, co->class_id);
	osp_obis_write(buf, &co->logical_name);
	osp_axdr_write_i8(buf, co->attribute_index);
	osp_axdr_write_u32(buf, co->data_index);
	osp_axdr_write_u8(buf, OSP_AXDR_NULL); /* restriction = none */
	return OSP_OK;
}

/* Value definition: { class_id, logical_name, attribute_index } */
osp_err_t osp_value_definition_read(osp_buf_t *buf, osp_value_definition_t *vd) {
	uint8_t nf;
	osp_struct_begin_read(buf, &nf);
	osp_axdr_read_u16(buf, &vd->class_id);
	osp_obis_read(buf, &vd->logical_name);
	osp_axdr_read_i8(buf, &vd->attribute_index);
	return OSP_OK;
}

osp_err_t osp_value_definition_write(osp_buf_t *buf, const osp_value_definition_t *vd) {
	osp_struct_begin(buf, 3);
	osp_axdr_write_u16(buf, vd->class_id);
	osp_obis_write(buf, &vd->logical_name);
	osp_axdr_write_i8(buf, vd->attribute_index);
	return OSP_OK;
}

/* Scaler unit: { scaler, unit } */
osp_err_t osp_scaler_unit_read(osp_buf_t *buf, osp_scaler_unit_t *su) {
	uint8_t nf;
	osp_struct_begin_read(buf, &nf);
	osp_axdr_read_i8(buf, &su->scaler);
	osp_axdr_read_u8(buf, &su->unit);
	return OSP_OK;
}

osp_err_t osp_scaler_unit_write(osp_buf_t *buf, const osp_scaler_unit_t *su) {
	osp_struct_begin(buf, 2);
	osp_axdr_write_i8(buf, su->scaler);
	osp_axdr_write_u8(buf, su->unit);
	return OSP_OK;
}

/* User list item: { id, name } */
osp_err_t osp_user_list_item_read(osp_buf_t *buf, osp_user_list_item_t *item) {
	uint8_t nf;
	osp_struct_begin_read(buf, &nf);
	osp_axdr_read_i8(buf, &item->id);
	uint32_t len = 0;
	osp_axdr_read_visible_string(buf, item->name, OSP_MAX_NAME_LEN, &len);
	item->name_len = (uint8_t)len;
	return OSP_OK;
}

osp_err_t osp_user_list_item_write(osp_buf_t *buf, const osp_user_list_item_t *item) {
	osp_struct_begin(buf, 2);
	osp_axdr_write_i8(buf, item->id);
	osp_axdr_write_visible_string(buf, item->name, item->name_len);
	return OSP_OK;
}

/* Attribute descriptor: { class_id, instance_id, attribute_id } */
osp_err_t osp_attribute_descriptor_read(osp_buf_t *buf, osp_attribute_descriptor_t *ad) {
	uint8_t nf;
	osp_struct_begin_read(buf, &nf);
	osp_axdr_read_u16(buf, &ad->class_id);
	osp_obis_read(buf, &ad->instance_id);
	osp_axdr_read_i8(buf, &ad->attribute_id);
	return OSP_OK;
}

osp_err_t osp_attribute_descriptor_write(osp_buf_t *buf, const osp_attribute_descriptor_t *ad) {
	osp_struct_begin(buf, 3);
	osp_axdr_write_u16(buf, ad->class_id);
	osp_obis_write(buf, &ad->instance_id);
	osp_axdr_write_i8(buf, ad->attribute_id);
	return OSP_OK;
}
