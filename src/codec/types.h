/**
 * types.h — COSEM data types (IEC 62056-6-2, Table 3)
 *
 * All 33 A-XDR primitive types with exact standard sizes.
 * osp_value_t is a tagged union holding any single value.
 * No malloc. MCU-safe with configurable max sizes.
 */

#ifndef OSP_TYPES_H
#define OSP_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Maximum sizes (configurable for MCU) ────────────────────────────────── */

#ifndef OSP_MAX_ARRAY_LEN
#define OSP_MAX_ARRAY_LEN 32
#endif
#ifndef OSP_MAX_STRUCT_LEN
#define OSP_MAX_STRUCT_LEN 16
#endif
#ifndef OSP_MAX_OCTET_LEN
#define OSP_MAX_OCTET_LEN 256
#endif
#ifndef OSP_MAX_STRING_LEN
#define OSP_MAX_STRING_LEN 256
#endif
#ifndef OSP_MAX_BITSTRING_LEN
#define OSP_MAX_BITSTRING_LEN 32
#endif

/*
 * Memory optimization notes:
 *
 * sizeof(osp_value_t) ≈ 4 + padding + max(octetstring, bitstring, visiblestring)
 * With OSP_MAX_OCTET_LEN=256: sizeof(osp_value_t) ≈ 264 bytes
 * With OSP_MAX_OCTET_LEN=128: sizeof(osp_value_t) ≈ 136 bytes (49% savings)
 * With OSP_MAX_OCTET_LEN=64:  sizeof(osp_value_t) ≈ 72 bytes  (73% savings)
 *
 * For constrained MCUs (< 32KB RAM), define these before including types.h:
 *   #define OSP_MAX_OCTET_LEN 64
 *   #define OSP_MAX_STRING_LEN 64
 *   #define OSP_MAX_ARRAY_LEN 8
 *   #define OSP_MAX_STRUCT_LEN 4
 */

/* ── AXDR tags (IEC 62056-6-2 Table 3) ──────────────────────────────────── */

typedef enum {
	OSP_TAG_NULL = 0,
	OSP_TAG_ARRAY = 1,
	OSP_TAG_STRUCTURE = 2,
	OSP_TAG_BOOLEAN = 3,
	OSP_TAG_BITSTRING = 4,
	OSP_TAG_DOUBLE_LONG = 5,     /* int32 */
	OSP_TAG_DOUBLE_LONG_UNS = 6, /* uint32 */
	OSP_TAG_OCTETSTRING = 9,
	OSP_TAG_VISIBLESTRING = 10,
	OSP_TAG_UTF8STRING = 12,
	OSP_TAG_BCD = 13,
	OSP_TAG_INTEGER = 15,       /* int8 */
	OSP_TAG_LONG = 16,          /* int16 */
	OSP_TAG_UNSIGNED = 17,      /* uint8 */
	OSP_TAG_LONG_UNSIGNED = 18, /* uint16 */
	OSP_TAG_COMPACT_ARRAY = 19,
	OSP_TAG_LONG64 = 20,          /* int64 */
	OSP_TAG_LONG64_UNSIGNED = 21, /* uint64 */
	OSP_TAG_ENUM = 22,
	OSP_TAG_FLOAT32 = 23,
	OSP_TAG_FLOAT64 = 24,
	OSP_TAG_DATETIME = 25, /* 12 bytes */
	OSP_TAG_DATE = 26,     /* 5 bytes  */
	OSP_TAG_TIME = 27,     /* 4 bytes  */
	OSP_TAG_DELTA_INTEGER = 28,
	OSP_TAG_DELTA_LONG = 29,
	OSP_TAG_DELTA_DLONG = 30,
	OSP_TAG_DELTA_UNSIGNED = 31,
	OSP_TAG_DELTA_LUNSIGNED = 32,
	OSP_TAG_DELTA_DLONG_UNS = 33,
} osp_axdr_tag_t;

/* ── Fixed size lookup (used by compact_array) ───────────────────────────── */

static inline uint8_t osp_axdr_type_size(uint8_t tag) {
	/* Lookup table indexed by AXDR tag value (max tag = 33) */
	static const uint8_t size_table[34] = {
	    [OSP_TAG_NULL] = 0,
	    [OSP_TAG_ARRAY] = 0,
	    [OSP_TAG_STRUCTURE] = 0,
	    [OSP_TAG_BOOLEAN] = 1,
	    [OSP_TAG_BITSTRING] = 0,
	    [OSP_TAG_DOUBLE_LONG] = 4,
	    [OSP_TAG_DOUBLE_LONG_UNS] = 4,
	    [OSP_TAG_OCTETSTRING] = 0,
	    [OSP_TAG_VISIBLESTRING] = 0,
	    [OSP_TAG_UTF8STRING] = 0,
	    [OSP_TAG_BCD] = 1,
	    [OSP_TAG_INTEGER] = 1,
	    [OSP_TAG_LONG] = 2,
	    [OSP_TAG_UNSIGNED] = 1,
	    [OSP_TAG_LONG_UNSIGNED] = 2,
	    [OSP_TAG_COMPACT_ARRAY] = 0,
	    [OSP_TAG_LONG64] = 8,
	    [OSP_TAG_LONG64_UNSIGNED] = 8,
	    [OSP_TAG_ENUM] = 1,
	    [OSP_TAG_FLOAT32] = 4,
	    [OSP_TAG_FLOAT64] = 8,
	    [OSP_TAG_DATETIME] = 12,
	    [OSP_TAG_DATE] = 5,
	    [OSP_TAG_TIME] = 4,
	    [OSP_TAG_DELTA_INTEGER] = 1,
	    [OSP_TAG_DELTA_LONG] = 2,
	    [OSP_TAG_DELTA_DLONG] = 4,
	    [OSP_TAG_DELTA_UNSIGNED] = 1,
	    [OSP_TAG_DELTA_LUNSIGNED] = 2,
	    [OSP_TAG_DELTA_DLONG_UNS] = 4,
	};

	if (tag < sizeof(size_table)) {
		return size_table[tag];
	}
	return 0;
}

/* ── Value list (array/structure elements) ───────────────────────────────── */

typedef struct osp_value osp_value_t;

typedef struct {
	osp_value_t *items;
	uint8_t count;
	uint8_t capacity;
} osp_value_list_t;

/* ── Individual type structs ─────────────────────────────────────────────── */

typedef struct {
	uint8_t _pad; /* empty by design (tag = 0), pad for strict C compliance */
} osp_null_t;

typedef struct {
	bool value;
} osp_boolean_t;

typedef struct {
	uint8_t bits[OSP_MAX_BITSTRING_LEN];
	uint32_t num_bits;
} osp_bitstring_t;

typedef struct {
	int32_t value;
} osp_int32_t;

typedef struct {
	uint32_t value;
} osp_uint32_t;

typedef struct {
	uint8_t data[OSP_MAX_OCTET_LEN];
	uint32_t len;
} osp_octetstring_t;

typedef struct {
	char data[OSP_MAX_STRING_LEN];
	uint32_t len;
} osp_visiblestring_t;

typedef struct {
	char data[OSP_MAX_STRING_LEN];
	uint32_t len;
} osp_utf8string_t;

typedef struct {
	uint8_t value;
} osp_bcd_t;

typedef struct {
	int8_t value;
} osp_int8_t;

typedef struct {
	int16_t value;
} osp_int16_t;

typedef struct {
	uint8_t value;
} osp_uint8_t;

typedef struct {
	uint16_t value;
} osp_uint16_t;

typedef struct {
	int64_t value;
} osp_int64_t;

typedef struct {
	uint64_t value;
} osp_uint64_t;

typedef struct {
	uint8_t value;
} osp_enum_t;

typedef struct {
	float value;
} osp_float32_t;

typedef struct {
	double value;
} osp_float64_t;

typedef struct {
	int8_t value;
} osp_delta_int8_t;

typedef struct {
	int16_t value;
} osp_delta_int16_t;

typedef struct {
	int32_t value;
} osp_delta_int32_t;

typedef struct {
	uint8_t value;
} osp_delta_uint8_t;

typedef struct {
	uint16_t value;
} osp_delta_uint16_t;

typedef struct {
	uint32_t value;
} osp_delta_uint32_t;

/* ── Date and time (IEC 62056-6-2 4.1.6.1) ─────────────────────────────── */

typedef struct {
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t day_of_week;
} osp_date_t;

typedef struct {
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	uint8_t ms;
} osp_time_t;

typedef struct {
	osp_date_t date;
	osp_time_t time;
} osp_datetime_t;

/** COSEM date-time octet-string (STO 34.01-5.1-006 §7.2.4, Blue Book §4.1.6.1). */
#define OSP_COSEM_DATETIME_LEN 12

typedef struct {
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t day_of_week;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	uint8_t hundredths;
	int16_t deviation;    /* minutes from UTC; 0x8000 = not specified */
	uint8_t clock_status; /* 0xFF = not specified */
} osp_cosem_datetime_t;

/* ── Complex types ───────────────────────────────────────────────────────── */

typedef struct {
	osp_value_list_t elements;
} osp_array_t;

typedef struct {
	osp_value_list_t elements;
} osp_structure_t;

/* ── Master tagged union ─────────────────────────────────────────────────── */

struct osp_value {
	uint8_t tag;

	union {
		osp_null_t null_val;
		osp_boolean_t boolean;
		osp_bitstring_t bitstring;
		osp_int32_t int32;
		osp_uint32_t uint32;
		osp_octetstring_t octetstring;
		osp_visiblestring_t visiblestring;
		osp_utf8string_t utf8string;
		osp_bcd_t bcd;
		osp_int8_t int8;
		osp_int16_t int16;
		osp_uint8_t uint8;
		osp_uint16_t uint16;
		osp_int64_t int64;
		osp_uint64_t uint64;
		osp_enum_t enum_val;
		osp_float32_t float32;
		osp_float64_t float64;
		osp_date_t date;
		osp_time_t time;
		osp_datetime_t datetime;
		osp_delta_int8_t delta_int8;
		osp_delta_int16_t delta_int16;
		osp_delta_int32_t delta_int32;
		osp_delta_uint8_t delta_uint8;
		osp_delta_uint16_t delta_uint16;
		osp_delta_uint32_t delta_uint32;
		osp_array_t array;
		osp_structure_t structure;
	} as;
};

/* ── Static value list (MCU-safe) ────────────────────────────────────────── */

typedef struct {
	osp_value_t items[OSP_MAX_ARRAY_LEN];
	uint8_t count;
} osp_static_list_t;

static inline void osp_static_list_init(osp_static_list_t *list) {
	list->count = 0;
}

/* ── Constructors ────────────────────────────────────────────────────────── */

static inline osp_value_t osp_val_null(void) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_NULL;
	return v;
}

static inline osp_value_t osp_val_bool(bool val) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_BOOLEAN;
	v.as.boolean.value = val;
	return v;
}

static inline osp_value_t osp_val_i8(int8_t val) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_INTEGER;
	v.as.int8.value = val;
	return v;
}

static inline osp_value_t osp_val_u8(uint8_t val) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_UNSIGNED;
	v.as.uint8.value = val;
	return v;
}

static inline osp_value_t osp_val_i16(int16_t val) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_LONG;
	v.as.int16.value = val;
	return v;
}

static inline osp_value_t osp_val_u16(uint16_t val) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_LONG_UNSIGNED;
	v.as.uint16.value = val;
	return v;
}

static inline osp_value_t osp_val_i32(int32_t val) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_DOUBLE_LONG;
	v.as.int32.value = val;
	return v;
}

static inline osp_value_t osp_val_u32(uint32_t val) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_DOUBLE_LONG_UNS;
	v.as.uint32.value = val;
	return v;
}

static inline osp_value_t osp_val_i64(int64_t val) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_LONG64;
	v.as.int64.value = val;
	return v;
}

static inline osp_value_t osp_val_u64(uint64_t val) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_LONG64_UNSIGNED;
	v.as.uint64.value = val;
	return v;
}

static inline osp_value_t osp_val_f32(float val) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_FLOAT32;
	v.as.float32.value = val;
	return v;
}

static inline osp_value_t osp_val_f64(double val) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_FLOAT64;
	v.as.float64.value = val;
	return v;
}

static inline osp_value_t osp_val_enum(uint8_t val) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_ENUM;
	v.as.enum_val.value = val;
	return v;
}

static inline osp_value_t osp_val_date(uint16_t year, uint8_t month, uint8_t day, uint8_t dow) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_DATE;
	v.as.date.year = year;
	v.as.date.month = month;
	v.as.date.day = day;
	v.as.date.day_of_week = dow;
	return v;
}

static inline osp_value_t osp_val_time(uint8_t h, uint8_t m, uint8_t s, uint16_t ms) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_TIME;
	v.as.time.hour = h;
	v.as.time.minute = m;
	v.as.time.second = s;
	v.as.time.ms = (uint8_t)(ms % 100);
	return v;
}

static inline osp_value_t osp_val_datetime(uint16_t y, uint8_t mo, uint8_t d, uint8_t dow, uint8_t h, uint8_t mi, uint8_t s, uint16_t ms) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_DATETIME;
	v.as.datetime.date.year = y;
	v.as.datetime.date.month = mo;
	v.as.datetime.date.day = d;
	v.as.datetime.date.day_of_week = dow;
	v.as.datetime.time.hour = h;
	v.as.datetime.time.minute = mi;
	v.as.datetime.time.second = s;
	v.as.datetime.time.ms = (uint8_t)(ms % 100);
	return v;
}

/* ── Extractors ──────────────────────────────────────────────────────────── */

static inline int32_t osp_get_i32(const osp_value_t *v) {
	return (v && v->tag == OSP_TAG_DOUBLE_LONG) ? v->as.int32.value : 0;
}

static inline uint32_t osp_get_u32(const osp_value_t *v) {
	return (v && v->tag == OSP_TAG_DOUBLE_LONG_UNS) ? v->as.uint32.value : 0;
}

static inline int16_t osp_get_i16(const osp_value_t *v) {
	return (v && v->tag == OSP_TAG_LONG) ? v->as.int16.value : 0;
}

static inline uint16_t osp_get_u16(const osp_value_t *v) {
	return (v && v->tag == OSP_TAG_LONG_UNSIGNED) ? v->as.uint16.value : 0;
}

static inline int8_t osp_get_i8(const osp_value_t *v) {
	return (v && v->tag == OSP_TAG_INTEGER) ? v->as.int8.value : 0;
}

static inline uint8_t osp_get_u8(const osp_value_t *v) {
	return (v && v->tag == OSP_TAG_UNSIGNED) ? v->as.uint8.value : 0;
}

static inline bool osp_get_bool(const osp_value_t *v) {
	return (v && v->tag == OSP_TAG_BOOLEAN) ? v->as.boolean.value : false;
}

static inline uint8_t osp_get_enum(const osp_value_t *v) {
	return (v && v->tag == OSP_TAG_ENUM) ? v->as.enum_val.value : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* OSP_TYPES_H */
