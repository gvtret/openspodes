/**
 * openspodes.h — Main include for the OpenSPODES DLMS/COSEM library
 *
 * A portable C99 implementation following IEC 62056, based on the architecture
 * of spodes-rs (Rust) with HAL abstractions for MCU portability.
 *
 * Copyright (c) 2025 OpenSPODES contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef OPENSPODES_H
#define OPENSPODES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ─────────────────────────────────────────────────────────────── */

#define OPENSPODES_VERSION_MAJOR 0
#define OPENSPODES_VERSION_MINOR 1
#define OPENSPODES_VERSION_PATCH 0

/* ── Error codes ─────────────────────────────────────────────────────────── */

typedef enum {
	OSP_OK = 0,
	OSP_ERR_NOMEM = -1,
	OSP_ERR_IO = -2,
	OSP_ERR_INVALID = -3,
	OSP_ERR_UNSUPPORTED = -4,
	OSP_ERR_TIMEOUT = -5,
	OSP_ERR_SECURITY = -6,
	OSP_ERR_NOT_FOUND = -7,
} osp_err_t;

/* ── OBIS code ───────────────────────────────────────────────────────────── */

typedef struct {
	uint8_t a; /* medium: 0=abstract, 1=electricity, ... */
	uint8_t b; /* channel */
	uint8_t c; /* channel number */
	uint8_t d; /* data item */
	uint8_t e; /* tariff */
	uint8_t f; /* sub-item */
} osp_obis_t;

static inline bool osp_obis_eq(const osp_obis_t *a, const osp_obis_t *b) {
	return a && b && memcmp(a, b, sizeof(osp_obis_t)) == 0;
}

/* ── Buffered array (zero-copy, no malloc) ───────────────────────────────── */

typedef struct {
	uint8_t *buf;
	uint32_t size;
	uint32_t rd;
	uint32_t wr;
} osp_buf_t;

static inline void osp_buf_init(osp_buf_t *b, uint8_t *mem, uint32_t size) {
	b->buf = mem;
	b->size = size;
	b->rd = 0;
	b->wr = 0;
}

static inline uint32_t osp_buf_written(const osp_buf_t *b) {
	return b->wr;
}

static inline uint32_t osp_buf_unread(const osp_buf_t *b) {
	return b->wr - b->rd;
}

static inline uint32_t osp_buf_free(const osp_buf_t *b) {
	return b->size - b->wr;
}

/* ── Include types ───────────────────────────────────────────────────────── */

#include "codec/types.h"

/* ── Interface class vtable (spodes-rs::InterfaceClass → C) ─────────────── */

typedef struct osp_ic_class osp_ic_class_t;

struct osp_ic_class {
	const char *name;
	uint16_t class_id;
	uint8_t version;

	/* Instance operations (all use osp_value_t) */
	osp_err_t (*get_attr)(const void *inst, uint8_t attr_id, osp_value_t *result);
	osp_err_t (*set_attr)(void *inst, uint8_t attr_id, const osp_value_t *value);
	osp_err_t (*invoke)(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result);

	size_t instance_size;
};

/* ── HAL interfaces (pluggable for MCU) ─────────────────────────────────── */

typedef struct {
	osp_err_t (*open)(void *ctx);
	osp_err_t (*send)(void *ctx, const uint8_t *data, uint32_t len);
	osp_err_t (*recv)(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms);
	void (*close)(void *ctx);
	bool (*is_connected)(void *ctx);
	void *ctx;
} osp_transport_t;

typedef struct {
	osp_err_t (*gcm_init)(void *ctx, const uint8_t *key, uint32_t key_len, const uint8_t *iv, uint32_t iv_len, const uint8_t *aad, uint32_t aad_len);
	osp_err_t (*gcm_update)(void *ctx, const uint8_t *in, uint32_t len, uint8_t *out);
	osp_err_t (*gcm_finish)(void *ctx, uint8_t *tag, uint32_t tag_len);
	void (*gcm_free)(void *ctx);
	void *ctx;
} osp_crypto_t;

typedef struct {
	osp_err_t (*fill)(void *ctx, uint8_t *buf, uint32_t len);
	void *ctx;
} osp_random_t;

typedef struct {
	uint32_t (*now_ms)(void *ctx);
	void (*delay_ms)(void *ctx, uint32_t ms);
	void *ctx;
} osp_timer_t;

typedef struct {
	uint8_t system_title[8];
	uint8_t *(*get_key)(void *ctx, uint8_t sap, uint8_t key_id);
	void *ctx;
} osp_system_t;

typedef struct {
	osp_transport_t transport;
	osp_crypto_t crypto;
	osp_random_t random;
	osp_timer_t timer;
	osp_system_t system;
} osp_hal_t;

#ifdef __cplusplus
}
#endif

#endif /* OPENSPODES_H */
