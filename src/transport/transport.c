/**
 * transport.c — HDLC frame codec + COSEM wrapper implementation
 *
 * HDLC CRC-16/X.25 from spodes-rs, adapted to C11.
 * Frame/deframe for IEC 62056-46, wrapper encode/decode for IEC 62056-47.
 *
 * Control field encoding follows ISO/IEC 13239:
 *   I-frame:  [N(R):3][P/F:1][N(S):3][0]
 *   S-frame:  [N(R):3][P/F:1][modifier:2][01]
 *   U-frame:  [modifier_hi:3][P/F:1][modifier_lo:2][11]
 *
 * U-frame modifier is 5 bits: hi=[7:5], lo=[3:2].
 * Standard modifier values (ISO 13239 §5.5.3, Green Book §8.4.3.1):
 *   SNRM=00100  SABM=00100  DISC=00000  UA=00110
 *   DM=00001    FRMR=01000  UI=00000    XID=10111
 */

#include "transport.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  CRC-16/X.25 (reflected polynomial 0x8408)
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uint16_t crc_table[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf, 0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7, 0x1081, 0x0108, 0x3393,
	0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e, 0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876, 0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af,
	0x4434, 0x55bd, 0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5, 0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c, 0xbdcb,
	0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974, 0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb, 0xce4c, 0xdfc5, 0xed5e, 0xfcd7,
	0x8868, 0x99e1, 0xab7a, 0xbaf3, 0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a, 0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb,
	0xaa72, 0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9, 0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1, 0x7387, 0x620e,
	0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738, 0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70, 0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c,
	0xd3a5, 0xe13e, 0xf0b7, 0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff, 0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e, 0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5, 0x2942, 0x38cb, 0x0a50,
	0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd, 0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134, 0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e,
	0x5cf5, 0x4d7c, 0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3, 0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb, 0xd68d,
	0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232, 0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a, 0xe70e, 0xf687, 0xc41c, 0xd595,
	0xa12a, 0xb0a3, 0x8238, 0x93b1, 0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9, 0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9,
	0x8330, 0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

uint16_t osp_hdlc_fcs16(const uint8_t *data, uint32_t len) {
	uint16_t fcs = 0xFFFF;
	for (uint32_t i = 0; i < len; i++) {
		fcs = (fcs >> 8) ^ crc_table[(fcs ^ data[i]) & 0xFF];
	}
	return fcs ^ 0xFFFF;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  HDLC address
 * ═══════════════════════════════════════════════════════════════════════════ */

void osp_hdlc_address_init(osp_hdlc_address_t *addr, uint32_t value, uint8_t length) {
	memset(addr, 0, sizeof(*addr));
	(void)length;

	if (value == 0) {
		addr->bytes[0] = 0x01;
		addr->length = 1;
		return;
	}

	uint8_t tmp[OSP_HDLC_MAX_ADDR_LEN];
	uint8_t n = 0;
	uint32_t v = value;
	while (v > 0 && n < OSP_HDLC_MAX_ADDR_LEN) {
		tmp[n++] = (uint8_t)(v & 0x7F);
		v >>= 7;
	}

	addr->length = n;
	for (uint8_t i = 0; i < n; i++) {
		uint8_t byte = (uint8_t)(tmp[n - 1 - i] << 1);
		if (i + 1 == n) {
			byte |= 0x01;
		}
		addr->bytes[i] = byte;
	}
}

uint32_t osp_hdlc_address_value(const osp_hdlc_address_t *addr) {
	uint32_t val = 0;
	for (uint8_t i = 0; i < addr->length; i++) {
		val = (val << 7) | ((addr->bytes[i] >> 1) & 0x7F);
	}
	return val;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  HDLC control field encode/decode (ISO/IEC 13239)
 *
 *  I-frame:  [N(R):3][P/F:1][N(S):3][0]
 *  S-frame:  [N(R):3][P/F:1][modifier:2][01]
 *  U-frame:  [modifier_hi:3][P/F:1][modifier_lo:2][11]
 * ═══════════════════════════════════════════════════════════════════════════ */

uint8_t osp_hdlc_control_encode(const osp_hdlc_control_t *ctrl) {
	uint8_t pf = ctrl->poll_final ? 0x10 : 0x00;

	switch (ctrl->type) {
		case OSP_HDLC_TYPE_I:
			return ((ctrl->recv_seq & 0x07) << 5) | pf | ((ctrl->send_seq & 0x07) << 1);

		case OSP_HDLC_TYPE_RR:
			return ((ctrl->recv_seq & 0x07) << 5) | pf | 0x01;
		case OSP_HDLC_TYPE_RNR:
			return ((ctrl->recv_seq & 0x07) << 5) | pf | 0x05;
		case OSP_HDLC_TYPE_REJ:
			return ((ctrl->recv_seq & 0x07) << 5) | pf | 0x09;

		case OSP_HDLC_TYPE_SNRM:
			return (4 << 5) | pf | (0 << 2) | 0x03; /* modifier 100/00 → 0x83|pf */
		case OSP_HDLC_TYPE_SABM:
			return (1 << 5) | pf | (3 << 2) | 0x03; /* modifier 001/11 → 0x23|pf */
		case OSP_HDLC_TYPE_DISC:
			return (2 << 5) | pf | (0 << 2) | 0x03; /* modifier 010/00 → 0x43|pf */
		case OSP_HDLC_TYPE_UA:
			return (3 << 5) | pf | (0 << 2) | 0x03; /* modifier 011/00 → 0x63|pf */
		case OSP_HDLC_TYPE_DM:
			return (0 << 5) | pf | (3 << 2) | 0x03; /* modifier 000/11 → 0x0F|pf */
		case OSP_HDLC_TYPE_FRMR:
			return (4 << 5) | pf | (1 << 2) | 0x03; /* modifier 100/01 → 0x87|pf */
		case OSP_HDLC_TYPE_UI:
			return (0 << 5) | pf | (0 << 2) | 0x03; /* modifier 000/00 → 0x03|pf */
		case OSP_HDLC_TYPE_XID:
			return (5 << 5) | pf | (3 << 2) | 0x03; /* modifier 101/11 → 0xAF|pf */
	}

	return 0x0F;
}

osp_err_t osp_hdlc_control_decode(uint8_t byte, osp_hdlc_control_t *ctrl) {
	if (!ctrl)
		return OSP_ERR_INVALID;

	ctrl->poll_final = (byte >> 4) & 1;
	ctrl->send_seq = 0;
	ctrl->recv_seq = (byte >> 5) & 0x07;

	/* I-frame: bit 0 = 0 */
	if ((byte & 0x01) == 0) {
		ctrl->type = OSP_HDLC_TYPE_I;
		ctrl->send_seq = (byte >> 1) & 0x07;
		return OSP_OK;
	}

	/* S-frame: bits 1:0 = 01 */
	if ((byte & 0x03) == 0x01) {
		switch (byte & 0x0F) {
			case 0x01:
				ctrl->type = OSP_HDLC_TYPE_RR;
				return OSP_OK;
			case 0x05:
				ctrl->type = OSP_HDLC_TYPE_RNR;
				return OSP_OK;
			case 0x09:
				ctrl->type = OSP_HDLC_TYPE_REJ;
				return OSP_OK;
			default:
				return OSP_ERR_UNSUPPORTED;
		}
	}

	/* U-frame: bits 1:0 = 11
	 * Decode via direct byte matching (ISO 13239 standard values).
	 * Mask out P/F bit (0x10) for type identification. */
	switch (byte & ~0x10) {
		case 0x83:
			ctrl->type = OSP_HDLC_TYPE_SNRM;
			return OSP_OK;
		case 0x63:
			ctrl->type = OSP_HDLC_TYPE_UA;
			return OSP_OK;
		case 0x43:
			ctrl->type = OSP_HDLC_TYPE_DISC;
			return OSP_OK;
		case 0x0F:
			ctrl->type = OSP_HDLC_TYPE_DM;
			return OSP_OK;
		case 0x87:
			ctrl->type = OSP_HDLC_TYPE_FRMR;
			return OSP_OK;
		case 0x03:
			ctrl->type = OSP_HDLC_TYPE_UI;
			return OSP_OK;
		case 0xAF:
			ctrl->type = OSP_HDLC_TYPE_XID;
			return OSP_OK;
		default:
			return OSP_ERR_UNSUPPORTED;
	}
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  HDLC frame encode
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint16_t make_frame_length(const osp_hdlc_frame_t *f) {
	/* СТО / IEC 62056-46: Length includes ALL fields except flags (0x7E).
	 * That means: format(2) + dest + src + control + [HCS(2) + info] + FCS(2) */
	uint16_t len = f->destination.length + f->source.length + 1;
	if (f->info_len > 0) {
		len += 2 + f->info_len; /* HCS + info */
	}
	len += 2; /* FCS */
	len += 2; /* Frame format field (2 bytes) */
	return len;
}

osp_err_t osp_hdlc_frame(const osp_hdlc_frame_t *frame, uint8_t *out, uint32_t max_len, uint32_t *out_len) {
	if (!frame || !out || !out_len)
		return OSP_ERR_INVALID;

	uint16_t flen = make_frame_length(frame);
	uint32_t total = 1 + 2 + flen + 1; /* flag + format + flen + flag */
	if (total > max_len)
		return OSP_ERR_NOMEM;

	uint32_t idx = 0;
	out[idx++] = OSP_HDLC_FLAG;

	/* Frame format (type A, no segmentation) */
	out[idx++] = 0xA0 | ((flen >> 8) & 0x07);
	out[idx++] = (uint8_t)(flen & 0xFF);

	/* Destination address */
	memcpy(&out[idx], frame->destination.bytes, frame->destination.length);
	idx += frame->destination.length;

	/* Source address */
	memcpy(&out[idx], frame->source.bytes, frame->source.length);
	idx += frame->source.length;

	/* Control byte */
	out[idx++] = osp_hdlc_control_encode(&frame->control);

	/* HCS + Information field */
	if (frame->info_len > 0) {
		uint16_t hcs = osp_hdlc_fcs16(&out[1], idx - 1);
		out[idx++] = (uint8_t)(hcs & 0xFF);
		out[idx++] = (uint8_t)(hcs >> 8);
		memcpy(&out[idx], frame->info, frame->info_len);
		idx += frame->info_len;
	}

	/* FCS */
	uint16_t fcs = osp_hdlc_fcs16(&out[1], idx - 1);
	out[idx++] = (uint8_t)(fcs & 0xFF);
	out[idx++] = (uint8_t)(fcs >> 8);

	out[idx++] = OSP_HDLC_FLAG;
	*out_len = idx;
	return OSP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  HDLC frame decode
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_hdlc_deframe(const uint8_t *data, uint32_t len, osp_hdlc_frame_t *frame) {
	if (!data || !frame || len < 6)
		return OSP_ERR_INVALID;
	if (data[0] != OSP_HDLC_FLAG || data[len - 1] != OSP_HDLC_FLAG)
		return OSP_ERR_INVALID;

	memset(frame, 0, sizeof(*frame));

	/* Frame format */
	if ((data[1] & 0xF0) != 0xA0)
		return OSP_ERR_INVALID;
	uint16_t flen = ((uint16_t)(data[1] & 0x07) << 8) | data[2];
	if (flen < 4 || (uint32_t)flen + 2 > len)
		return OSP_ERR_INVALID;

	/* FCS verification */
	uint16_t fcs_calc = osp_hdlc_fcs16(&data[1], len - 4);
	uint16_t fcs_recv = (uint16_t)data[len - 3] | ((uint16_t)data[len - 2] << 8);
	if (fcs_calc != fcs_recv)
		return OSP_ERR_INVALID;

	uint32_t idx = 3;

	/* Destination address */
	frame->destination.length = 0;
	while (idx < len - 3 && frame->destination.length < OSP_HDLC_MAX_ADDR_LEN) {
		frame->destination.bytes[frame->destination.length++] = data[idx];
		if (data[idx] & 0x01) {
			idx++;
			break;
		}
		idx++;
	}

	/* Source address */
	frame->source.length = 0;
	while (idx < len - 3 && frame->source.length < OSP_HDLC_MAX_ADDR_LEN) {
		frame->source.bytes[frame->source.length++] = data[idx];
		if (data[idx] & 0x01) {
			idx++;
			break;
		}
		idx++;
	}

	/* Control byte */
	if (idx >= len - 3)
		return OSP_ERR_INVALID;
	osp_err_t r = osp_hdlc_control_decode(data[idx++], &frame->control);
	if (r != OSP_OK)
		return r;

	/* HCS + information field */
	uint32_t fcs_start = len - 3;
	if (idx < fcs_start) {
		uint16_t hcs_recv = (uint16_t)data[idx] | ((uint16_t)data[idx + 1] << 8);
		uint16_t hcs_calc = osp_hdlc_fcs16(&data[1], idx - 1);
		if (hcs_recv != hcs_calc)
			return OSP_ERR_INVALID;
		idx += 2;

		frame->info_len = fcs_start - idx;
		if (frame->info_len > OSP_HDLC_MAX_FRAME_SIZE)
			frame->info_len = OSP_HDLC_MAX_FRAME_SIZE;
		memcpy(frame->info, &data[idx], frame->info_len);
	}

	return OSP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  COSEM Wrapper (IEC 62056-47)
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_wrapper_encode(uint16_t source, uint16_t dest, const uint8_t *apdu, uint32_t apdu_len, uint8_t *out, uint32_t max_len, uint32_t *out_len) {
	if (!apdu || !out || !out_len)
		return OSP_ERR_INVALID;
	if (apdu_len + OSP_WRAPPER_HEADER_SIZE > max_len)
		return OSP_ERR_NOMEM;

	out[0] = 0x00;
	out[1] = (uint8_t)OSP_WRAPPER_VERSION;
	out[2] = (uint8_t)(source >> 8);
	out[3] = (uint8_t)(source & 0xFF);
	out[4] = (uint8_t)(dest >> 8);
	out[5] = (uint8_t)(dest & 0xFF);
	out[6] = (uint8_t)(apdu_len >> 8);
	out[7] = (uint8_t)(apdu_len & 0xFF);
	memcpy(&out[OSP_WRAPPER_HEADER_SIZE], apdu, apdu_len);
	*out_len = OSP_WRAPPER_HEADER_SIZE + apdu_len;
	return OSP_OK;
}

osp_err_t osp_wrapper_decode(const uint8_t *data, uint32_t len, osp_wrapper_header_t *header, const uint8_t **apdu, uint32_t *apdu_len) {
	if (!data || !header || !apdu || !apdu_len)
		return OSP_ERR_INVALID;
	if (len < OSP_WRAPPER_HEADER_SIZE)
		return OSP_ERR_INVALID;

	header->version = ((uint16_t)data[0] << 8) | data[1];
	header->source = ((uint16_t)data[2] << 8) | data[3];
	header->destination = ((uint16_t)data[4] << 8) | data[5];
	header->length = ((uint16_t)data[6] << 8) | data[7];

	if (header->version != OSP_WRAPPER_VERSION)
		return OSP_ERR_INVALID;
	if ((uint32_t)header->length + OSP_WRAPPER_HEADER_SIZE > len)
		return OSP_ERR_INVALID;

	*apdu = &data[OSP_WRAPPER_HEADER_SIZE];
	*apdu_len = header->length;
	return OSP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Transport send/receive APDUs
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_transport_send_apdu(osp_transport_t *t, osp_framing_type_t framing, const uint8_t *apdu, uint32_t apdu_len) {
	if (!t || !t->send)
		return OSP_ERR_INVALID;

	uint8_t framed[OSP_HDLC_MAX_FRAME_SIZE + 32];
	uint32_t framed_len = 0;

	switch (framing) {
		case OSP_FRAMING_WRAPPER: {
			osp_err_t r = osp_wrapper_encode(0, 0, apdu, apdu_len, framed, sizeof(framed), &framed_len);
			if (r != OSP_OK)
				return r;
			break;
		}
		case OSP_FRAMING_HDLC: {
			osp_hdlc_frame_t frame;
			memset(&frame, 0, sizeof(frame));
			osp_hdlc_address_init(&frame.destination, 1, 1);
			osp_hdlc_address_init(&frame.source, 1, 1);
			frame.control.type = OSP_HDLC_TYPE_I;
			frame.control.poll_final = true;
			memcpy(frame.info, apdu, apdu_len > OSP_HDLC_MAX_FRAME_SIZE ? OSP_HDLC_MAX_FRAME_SIZE : apdu_len);
			frame.info_len = apdu_len > OSP_HDLC_MAX_FRAME_SIZE ? OSP_HDLC_MAX_FRAME_SIZE : (uint16_t)apdu_len;
			osp_err_t r = osp_hdlc_frame(&frame, framed, sizeof(framed), &framed_len);
			if (r != OSP_OK)
				return r;
			break;
		}
		case OSP_FRAMING_NONE:
			if (apdu_len > sizeof(framed))
				return OSP_ERR_NOMEM;
			memcpy(framed, apdu, apdu_len);
			framed_len = apdu_len;
			break;
	}

	return t->send(t->ctx, framed, framed_len);
}

osp_err_t osp_transport_recv_apdu(osp_transport_t *t, osp_framing_type_t framing, uint8_t *buf, uint32_t buf_size, uint32_t *apdu_len, uint32_t timeout_ms) {
	if (!t || !t->recv || !buf || !apdu_len)
		return OSP_ERR_INVALID;

	uint8_t raw[OSP_HDLC_MAX_FRAME_SIZE + 32];
	uint32_t raw_len = 0;

	osp_err_t r = t->recv(t->ctx, raw, sizeof(raw), &raw_len, timeout_ms);
	if (r != OSP_OK)
		return r;

	switch (framing) {
		case OSP_FRAMING_WRAPPER: {
			osp_wrapper_header_t hdr;
			const uint8_t *apdu_ptr;
			uint32_t apdu_sz;
			r = osp_wrapper_decode(raw, raw_len, &hdr, &apdu_ptr, &apdu_sz);
			if (r != OSP_OK)
				return r;
			if (apdu_sz > buf_size)
				return OSP_ERR_NOMEM;
			memcpy(buf, apdu_ptr, apdu_sz);
			*apdu_len = apdu_sz;
			return OSP_OK;
		}
		case OSP_FRAMING_HDLC: {
			osp_hdlc_frame_t frame;
			r = osp_hdlc_deframe(raw, raw_len, &frame);
			if (r != OSP_OK)
				return r;
			if (frame.info_len > buf_size)
				return OSP_ERR_NOMEM;
			memcpy(buf, frame.info, frame.info_len);
			*apdu_len = frame.info_len;
			return OSP_OK;
		}
		case OSP_FRAMING_NONE:
			if (raw_len > buf_size)
				return OSP_ERR_NOMEM;
			memcpy(buf, raw, raw_len);
			*apdu_len = raw_len;
			return OSP_OK;
	}

	return OSP_ERR_UNSUPPORTED;
}
