/**
 * codec.h — BER/AXDR codec for OpenSPODES
 *
 * Based on spodes-rs serialization patterns and OpenDLMS codec.
 * Zero-copy: all operations work on osp_buf_t.
 */

#ifndef OSP_CODEC_H
#define OSP_CODEC_H

#include "../openspodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── BER read ────────────────────────────────────────────────────────────── */

typedef struct {
    uint8_t  tag_class;
    bool     tag_constructed;
    uint8_t  tag_number;
    uint32_t length;
} osp_ber_tag_t;

osp_err_t osp_ber_read_tag(osp_buf_t *buf, osp_ber_tag_t *tag);
osp_err_t osp_ber_read_length(osp_buf_t *buf, uint32_t *len);
osp_err_t osp_ber_read_uint(osp_buf_t *buf, uint32_t *val);
osp_err_t osp_ber_read_octet_string(osp_buf_t *buf, uint8_t *out, uint32_t max_len, uint32_t *out_len);

/* ── BER write ───────────────────────────────────────────────────────────── */

osp_err_t osp_ber_write_tag(osp_buf_t *buf, uint8_t tag_class, bool constructed, uint8_t tag_number);
osp_err_t osp_ber_write_length(osp_buf_t *buf, uint32_t len);
osp_err_t osp_ber_write_uint(osp_buf_t *buf, uint32_t val);
osp_err_t osp_ber_write_octet_string(osp_buf_t *buf, const uint8_t *data, uint32_t len);

/* ── AXDR read ───────────────────────────────────────────────────────────── */

osp_err_t osp_axdr_read_tag(osp_buf_t *buf, uint8_t *tag);
osp_err_t osp_axdr_read_u8(osp_buf_t *buf, uint8_t *val);
osp_err_t osp_axdr_read_u16(osp_buf_t *buf, uint16_t *val);
osp_err_t osp_axdr_read_u32(osp_buf_t *buf, uint32_t *val);
osp_err_t osp_axdr_read_bool(osp_buf_t *buf, bool *val);
osp_err_t osp_axdr_read_octet_string(osp_buf_t *buf, uint8_t *out, uint32_t max_len, uint32_t *out_len);

/* ── AXDR write ──────────────────────────────────────────────────────────── */

osp_err_t osp_axdr_write_tag(osp_buf_t *buf, uint8_t tag);
osp_err_t osp_axdr_write_u8(osp_buf_t *buf, uint8_t val);
osp_err_t osp_axdr_write_u16(osp_buf_t *buf, uint16_t val);
osp_err_t osp_axdr_write_u32(osp_buf_t *buf, uint32_t val);
osp_err_t osp_axdr_write_bool(osp_buf_t *buf, bool val);
osp_err_t osp_axdr_write_octet_string(osp_buf_t *buf, const uint8_t *data, uint32_t len);

/* ── AXDR type tags (IEC 62056-6-2 Table 2) ─────────────────────────────── */

#define OSP_AXDR_NULL        0
#define OSP_AXDR_ARRAY       1
#define OSP_AXDR_STRUCTURE   2
#define OSP_AXDR_BOOLEAN     3
#define OSP_AXDR_BITSTRING   4
#define OSP_AXDR_INT32       5
#define OSP_AXDR_UINT32      6
#define OSP_AXDR_OCTETSTRING 9
#define OSP_AXDR_UTF8STRING  10
#define OSP_AXDR_ENUM        15

#ifdef __cplusplus
}
#endif

#endif /* OSP_CODEC_H */
