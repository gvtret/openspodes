/**
 * transport.h — HDLC frame codec (IEC 62056-46) and COSEM wrapper (IEC 62056-47)
 *
 * Two framing sub-layers built on the HAL transport:
 *   - HDLC: works over any medium (serial, TCP)
 *   - Wrapper: TCP/UDP only (8-byte header prefix)
 *
 * Zero-copy: operates on osp_buf_t.
 */

#ifndef OSP_TRANSPORT_H
#define OSP_TRANSPORT_H

#include "../openspodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 *  HDLC (IEC 62056-46, ISO/IEC 13239)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define OSP_HDLC_FLAG           0x7E
#define OSP_HDLC_MAX_ADDR_LEN   4
#ifndef OSP_HDLC_MAX_FRAME_SIZE
#define OSP_HDLC_MAX_FRAME_SIZE 2048
#endif

/** @brief Compute CRC-16/X.25 (polynomial 0x8408 reflected, init 0xFFFF). */
uint16_t osp_hdlc_fcs16(const uint8_t *data, uint32_t len);

/* HDLC address: up to 4 bytes, LSB-first with extension bit */
typedef struct {
	uint8_t bytes[OSP_HDLC_MAX_ADDR_LEN];
	uint8_t length;
} osp_hdlc_address_t;

/** @brief Initialize an HDLC address from an integer value and byte length. */
void osp_hdlc_address_init(osp_hdlc_address_t *addr, uint32_t value, uint8_t length);

/** @brief Read the integer value of an HDLC address. */
uint32_t osp_hdlc_address_value(const osp_hdlc_address_t *addr);

/* HDLC frame types (ISO/IEC 13239 control field) */
typedef enum {
	/* I-frame (information): bit 0 = 0 */
	OSP_HDLC_TYPE_I = 0,

	/* S-frames (supervisory): bit 0 = 1, bit 1 = 0 */
	OSP_HDLC_TYPE_RR = 1,   /* Receive Ready */
	OSP_HDLC_TYPE_RNR = 2,  /* Receive Not Ready */
	OSP_HDLC_TYPE_REJ = 3,  /* Reject */

	/* U-frames (unnumbered): bit 0 = 1, bit 1 = 1 */
	OSP_HDLC_TYPE_SABM = 4, /* Set Asynchronous Balanced Mode */
	OSP_HDLC_TYPE_SNRM = 5, /* Set Normal Response Mode */
	OSP_HDLC_TYPE_DISC = 6, /* Disconnect */
	OSP_HDLC_TYPE_DM = 7,   /* Disconnected Mode */
	OSP_HDLC_TYPE_UA = 8,   /* Unnumbered Acknowledge */
	OSP_HDLC_TYPE_FRMR = 9, /* Frame Reject */
	OSP_HDLC_TYPE_UI = 10,  /* Unnumbered Information */
	OSP_HDLC_TYPE_XID = 11, /* Exchange Identification */
} osp_hdlc_frame_type_t;

/* HDLC control field */
typedef struct {
	osp_hdlc_frame_type_t type;
	uint8_t poll_final; /* P/F bit (position 4) */
	uint8_t send_seq;   /* N(S) for I-frames (bits 3:1, mod 8) */
	uint8_t recv_seq;   /* N(R) for I/S-frames (bits 7:5, mod 8) */
} osp_hdlc_control_t;

/** @brief Encode HDLC control field to a single byte. */
uint8_t osp_hdlc_control_encode(const osp_hdlc_control_t *ctrl);

/** @brief Decode HDLC control field from a single byte. */
osp_err_t osp_hdlc_control_decode(uint8_t byte, osp_hdlc_control_t *ctrl);

/* Complete HDLC frame */
typedef struct {
	osp_hdlc_address_t destination;
	osp_hdlc_address_t source;
	osp_hdlc_control_t control;
	uint8_t info[OSP_HDLC_MAX_FRAME_SIZE];
	uint16_t info_len;
} osp_hdlc_frame_t;

/** @brief Frame a message into HDLC: prepends/strips 7E flags, adds address, control, HCS, FCS. */
osp_err_t osp_hdlc_frame(const osp_hdlc_frame_t *frame, uint8_t *out, uint32_t max_len, uint32_t *out_len);

/** @brief Deframe: strip flags, validate FCS/HCS, parse into osp_hdlc_frame_t. */
osp_err_t osp_hdlc_deframe(const uint8_t *data, uint32_t len, osp_hdlc_frame_t *frame);

/* ═══════════════════════════════════════════════════════════════════════════
 *  COSEM Wrapper (IEC 62056-47)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define OSP_WRAPPER_VERSION     0x0001
#define OSP_WRAPPER_HEADER_SIZE 8

/* Wrapper header */
typedef struct {
	uint16_t version;
	uint16_t source;
	uint16_t destination;
	uint16_t length;
} osp_wrapper_header_t;

/** @brief Encode: prepend 8-byte COSEM wrapper header to APDU. */
osp_err_t osp_wrapper_encode(uint16_t source, uint16_t dest, const uint8_t *apdu, uint32_t apdu_len, uint8_t *out, uint32_t max_len, uint32_t *out_len);

/** @brief Decode: strip wrapper header, return APDU pointer and length. */
osp_err_t osp_wrapper_decode(const uint8_t *data, uint32_t len, osp_wrapper_header_t *header, const uint8_t **apdu, uint32_t *apdu_len);

/* ═══════════════════════════════════════════════════════════════════════════
 *  Data link layer: send/receive APDUs over HAL transport
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
	OSP_FRAMING_NONE = 0,
	OSP_FRAMING_HDLC = 1,
	OSP_FRAMING_WRAPPER = 2,
} osp_framing_type_t;

/** @brief Send a raw APDU through the framing layer (HDLC or wrapper). */
osp_err_t osp_transport_send_apdu(osp_transport_t *t, osp_framing_type_t framing, const uint8_t *apdu, uint32_t apdu_len);

/** @brief Receive a raw APDU from the framing layer. */
osp_err_t osp_transport_recv_apdu(osp_transport_t *t, osp_framing_type_t framing, uint8_t *buf, uint32_t buf_size, uint32_t *apdu_len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OSP_TRANSPORT_H */
