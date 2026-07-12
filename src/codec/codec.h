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

/** BER tag decoded from wire (IEC 62056-6-2). */
typedef struct {
	uint8_t tag_class;    /**< 0=universal, 1=application, 2=context, 3=private */
	bool tag_constructed; /**< true if contains nested TLVs */
	uint8_t tag_number;   /**< tag number within the class */
	uint32_t length;      /**< content length in bytes */
} osp_ber_tag_t;

/** @brief Decode DLMS definite length (shared BER/A-XDR). */
osp_err_t osp_dlms_write_len(osp_buf_t *buf, uint32_t len);
/** @brief Encode DLMS definite length. */
osp_err_t osp_dlms_read_len(osp_buf_t *buf, uint32_t *len);

/** @brief Read a BER tag from the buffer. @param buf Input buffer. @param tag Output decoded tag. */
osp_err_t osp_ber_read_tag(osp_buf_t *buf, osp_ber_tag_t *tag);
/** @brief Read a BER length field. @param buf Input buffer. @param len Output length. */
osp_err_t osp_ber_read_length(osp_buf_t *buf, uint32_t *len);
/** @brief Read an unsigned integer encoded in BER (all remaining bytes, big-endian). */
osp_err_t osp_ber_read_uint(osp_buf_t *buf, uint32_t *val);
/** @brief Read a BER OCTET STRING. @param max_len Max bytes to read into out. @param out_len Actual bytes read. */
osp_err_t osp_ber_read_octet_string(osp_buf_t *buf, uint8_t *out, uint32_t max_len, uint32_t *out_len);

/* ── BER write ───────────────────────────────────────────────────────────── */

/** @brief Write a BER tag. @param tag_class 0-3, @param constructed true for SEQUENCE/SET, @param tag_number tag number. */
osp_err_t osp_ber_write_tag(osp_buf_t *buf, uint8_t tag_class, bool constructed, uint8_t tag_number);
/** @brief Write a BER length field (short form for <128, long form otherwise). */
osp_err_t osp_ber_write_length(osp_buf_t *buf, uint32_t len);
/** @brief Write an unsigned integer in BER encoding (minimal bytes, big-endian). */
osp_err_t osp_ber_write_uint(osp_buf_t *buf, uint32_t val);
/** @brief Write a BER OCTET STRING (tag 0x04 + length + data). */
osp_err_t osp_ber_write_octet_string(osp_buf_t *buf, const uint8_t *data, uint32_t len);

/* ── AXDR read ───────────────────────────────────────────────────────────── */

/** @brief Read a single AXDR tag byte. */
osp_err_t osp_axdr_read_tag(osp_buf_t *buf, uint8_t *tag);
/** @brief Read AXDR unsigned 8-bit. */
osp_err_t osp_axdr_read_u8(osp_buf_t *buf, uint8_t *val);
/** @brief Read AXDR unsigned 16-bit (big-endian). */
osp_err_t osp_axdr_read_u16(osp_buf_t *buf, uint16_t *val);
/** @brief Read AXDR unsigned 32-bit (big-endian). */
osp_err_t osp_axdr_read_u32(osp_buf_t *buf, uint32_t *val);
/** @brief Read AXDR boolean (0x00=false, 0xFF=true). */
osp_err_t osp_axdr_read_bool(osp_buf_t *buf, bool *val);
/** @brief Read AXDR OCTET STRING (length-prefixed). @param max_len Max bytes. @param out_len Actual bytes. */
osp_err_t osp_axdr_read_octet_string(osp_buf_t *buf, uint8_t *out, uint32_t max_len, uint32_t *out_len);

/* ── AXDR write ──────────────────────────────────────────────────────────── */

/** @brief Write a single AXDR tag byte. */
osp_err_t osp_axdr_write_tag(osp_buf_t *buf, uint8_t tag);
/** @brief Write AXDR unsigned 8-bit. */
osp_err_t osp_axdr_write_u8(osp_buf_t *buf, uint8_t val);
/** @brief Write AXDR unsigned 16-bit (big-endian). */
osp_err_t osp_axdr_write_u16(osp_buf_t *buf, uint16_t val);
/** @brief Write AXDR unsigned 32-bit (big-endian). */
osp_err_t osp_axdr_write_u32(osp_buf_t *buf, uint32_t val);
/** @brief Write AXDR boolean. */
osp_err_t osp_axdr_write_bool(osp_buf_t *buf, bool val);
/** @brief Write AXDR OCTET STRING (length-prefixed). */
osp_err_t osp_axdr_write_octet_string(osp_buf_t *buf, const uint8_t *data, uint32_t len);

/* xDLMS list counts, DataBlock-SA raw bytes, notification fields — same length rules */
int osp_axdr_push_length(osp_buf_t *buf, uint32_t length);
int osp_axdr_read_length(osp_buf_t *buf, uint32_t *length);

/* Dispatch BER (ACSE) vs A-XDR (xDLMS) by leading APDU tag */
typedef enum {
	OSP_DLMS_ENC_BER_ACSE,
	OSP_DLMS_ENC_AXDR_XDLMS,
	OSP_DLMS_ENC_UNKNOWN,
} osp_dlms_encoding_t;

osp_dlms_encoding_t osp_dlms_encoding_for_apdu(uint8_t first_tag);

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
