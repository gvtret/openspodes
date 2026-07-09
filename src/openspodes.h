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

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ─────────────────────────────────────────────────────────────── */

#define OPENSPODES_VERSION_MAJOR 0
#define OPENSPODES_VERSION_MINOR 1
#define OPENSPODES_VERSION_PATCH 0

/* ── Error codes ─────────────────────────────────────────────────────────── */

typedef enum {
    OSP_OK              =  0,
    OSP_ERR_NOMEM       = -1,
    OSP_ERR_IO          = -2,
    OSP_ERR_INVALID     = -3,
    OSP_ERR_UNSUPPORTED = -4,
    OSP_ERR_TIMEOUT     = -5,
    OSP_ERR_SECURITY    = -6,
    OSP_ERR_NOT_FOUND   = -7,
} osp_err_t;

/* ── OBIS code ───────────────────────────────────────────────────────────── */

typedef struct {
    uint8_t a;  /* medium: 0=abstract, 1=electricity, ... */
    uint8_t b;  /* channel */
    uint8_t c;  /* channel number / abstract: 0-89=context, 96=general */
    uint8_t d;  /* data item */
    uint8_t e;  /* tariff */
    uint8_t f;  /* sub-item */
} osp_obis_t;

/* ── COSEM data types (A-XDR tags) ──────────────────────────────────────── */

typedef enum {
    OSP_TYPE_NULL        = 0,
    OSP_TYPE_BOOLEAN     = 3,
    OSP_TYPE_BITSTRING   = 4,
    OSP_TYPE_INT32       = 5,
    OSP_TYPE_UINT32      = 6,
    OSP_TYPE_OCTETSTRING = 9,
    OSP_TYPE_UTF8STRING  = 10,
    OSP_TYPE_ARRAY       = 1,
    OSP_TYPE_STRUCTURE   = 2,
    OSP_TYPE_ENUM        = 15,
    OSP_TYPE_FLOAT32     = 12,
    OSP_TYPE_FLOAT64     = 13,
} osp_type_t;

/* ── Buffered array (zero-copy, no malloc) ───────────────────────────────── */

typedef struct {
    uint8_t *buf;
    uint32_t size;
    uint32_t rd;
    uint32_t wr;
} osp_buf_t;

static inline void osp_buf_init(osp_buf_t *b, uint8_t *mem, uint32_t size)
{
    b->buf  = mem;
    b->size = size;
    b->rd   = 0;
    b->wr   = 0;
}

static inline uint32_t osp_buf_written(const osp_buf_t *b) { return b->wr; }
static inline uint32_t osp_buf_unread(const osp_buf_t *b)  { return b->wr - b->rd; }
static inline uint32_t osp_buf_free(const osp_buf_t *b)    { return b->size - b->wr; }

/* ── Interface class vtable (spodes-rs::InterfaceClass → C) ─────────────── */

/* Forward declaration */
typedef struct osp_ic_class osp_ic_class_t;

/* Attribute descriptor */
typedef struct {
    uint8_t  id;
    uint8_t  access;  /* bit 0=get, bit 1=set */
    osp_type_t type;
} osp_ic_attr_t;

/* Method descriptor */
typedef struct {
    uint8_t  id;
    uint8_t  access;  /* bit 2=action */
    osp_type_t param_type;
} osp_ic_method_t;

/* Vtable: one per IC class definition */
struct osp_ic_class {
    const char         *name;
    uint16_t            class_id;
    uint8_t             version;

    /* Attribute/method metadata */
    const osp_ic_attr_t  *attrs;
    uint8_t               attr_count;
    const osp_ic_method_t *methods;
    uint8_t               method_count;

    /* Instance operations */
    osp_err_t (*serialize)(const void *inst, osp_buf_t *buf);
    osp_err_t (*deserialize)(void *inst, const osp_buf_t *buf);
    osp_err_t (*get_attr)(const void *inst, uint8_t attr_id, osp_buf_t *buf);
    osp_err_t (*set_attr)(void *inst, uint8_t attr_id, const osp_buf_t *buf);
    osp_err_t (*invoke)(void *inst, uint8_t method_id, const osp_buf_t *param, osp_buf_t *result);

    /* Instance size (for static allocation) */
    size_t    instance_size;
};

/* ── HAL interfaces (pluggable for MCU) ─────────────────────────────────── */

/* Transport HAL */
typedef struct {
    osp_err_t (*open)(void *ctx);
    osp_err_t (*send)(void *ctx, const uint8_t *data, uint32_t len);
    osp_err_t (*recv)(void *ctx, uint8_t *buf, uint32_t size, uint32_t timeout_ms);
    void      (*close)(void *ctx);
    bool      (*is_connected)(void *ctx);
    void     *ctx;
} osp_transport_t;

/* Crypto HAL */
typedef struct {
    osp_err_t (*gcm_init)(void *ctx, const uint8_t *key, uint32_t key_len,
                          const uint8_t *iv, uint32_t iv_len,
                          const uint8_t *aad, uint32_t aad_len);
    osp_err_t (*gcm_update)(void *ctx, const uint8_t *in, uint32_t len, uint8_t *out);
    osp_err_t (*gcm_finish)(void *ctx, uint8_t *tag, uint32_t tag_len);
    void      (*gcm_free)(void *ctx);
    void     *ctx;
} osp_crypto_t;

/* Random HAL */
typedef struct {
    osp_err_t (*fill)(void *ctx, uint8_t *buf, uint32_t len);
    void     *ctx;
} osp_random_t;

/* Timer HAL */
typedef struct {
    uint32_t (*now_ms)(void *ctx);
    void     (*delay_ms)(void *ctx, uint32_t ms);
    void     *ctx;
} osp_timer_t;

/* System HAL (key storage, system title, etc.) */
typedef struct {
    uint8_t  system_title[8];
    uint8_t *(*get_key)(void *ctx, uint8_t sap, uint8_t key_id);
    void     *ctx;
} osp_system_t;

/* ── Combined HAL context ────────────────────────────────────────────────── */

typedef struct {
    osp_transport_t transport;
    osp_crypto_t    crypto;
    osp_random_t    random;
    osp_timer_t     timer;
    osp_system_t    system;
} osp_hal_t;

#ifdef __cplusplus
}
#endif

#endif /* OPENSPODES_H */
