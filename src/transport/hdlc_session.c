/**
 * hdlc_session.c — HDLC link-layer session (IEC 62056-46)
 *
 * Implements SNRM/UA connection setup, I-frame send/receive with
 * N(S)/N(R) tracking, LLC header handling, and DISC/DM teardown.
 */

#include "hdlc_session.h"
#include <string.h>

/* LLC header constants (IEC 62056-47) */
static const uint8_t LLC_COMMAND[3] = {0xE6, 0xE6, 0x00};  /* client → server */
static const uint8_t LLC_RESPONSE[3] = {0xE6, 0xE7, 0x00}; /* server → client */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Low-level: send/receive a single raw HDLC frame
 * ═══════════════════════════════════════════════════════════════════════════ */

static osp_err_t session_send_frame(osp_hdlc_session_t *s, const osp_hdlc_frame_t *frame) {
	uint32_t len = 0;
	osp_err_t r = osp_hdlc_frame(frame, s->tx_buf, sizeof(s->tx_buf), &len);
	if (r != OSP_OK)
		return r;
	return s->transport->send(s->transport->ctx, s->tx_buf, len);
}

static osp_err_t session_recv_frame(osp_hdlc_session_t *s, osp_hdlc_frame_t *frame, uint32_t timeout_ms) {
	uint32_t raw_len = 0;
	osp_err_t r = s->transport->recv(s->transport->ctx, s->rx_buf, sizeof(s->rx_buf), &raw_len, timeout_ms);
	if (r != OSP_OK)
		return r;
	return osp_hdlc_deframe(s->rx_buf, raw_len, frame);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  XID parameter encoding/decoding (IEC 62056-46 §6.4.4.4.3.2)
 *
 *  XID info field format:
 *    81 80           — format ID + group ID
 *    05 01 <val>     — max info field length tx
 *    06 01 <val>     — max info field length rx
 *    07 04 <4 bytes> — window size tx
 *    08 04 <4 bytes> — window size rx
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint16_t encode_xid_params(const osp_hdlc_xid_params_t *p, uint8_t *out) {
	uint16_t idx = 0;

	/* Format ID + Group ID */
	out[idx++] = 0x81;
	out[idx++] = 0x80;

	/* Group length placeholder — filled after parameters */
	uint16_t group_len_pos = idx++;
	out[group_len_pos] = 0; /* placeholder */

	/* Parameter 5: max info field length tx (1 or 2 bytes, big-endian) */
	out[idx++] = 0x05;
	if (p->max_info_tx > 255) {
		out[idx++] = 0x02;
		out[idx++] = (uint8_t)((p->max_info_tx >> 8) & 0xFF);
		out[idx++] = (uint8_t)(p->max_info_tx & 0xFF);
	} else {
		out[idx++] = 0x01;
		out[idx++] = (uint8_t)(p->max_info_tx & 0xFF);
	}

	/* Parameter 6: max info field length rx (1 or 2 bytes, big-endian) */
	out[idx++] = 0x06;
	if (p->max_info_rx > 255) {
		out[idx++] = 0x02;
		out[idx++] = (uint8_t)((p->max_info_rx >> 8) & 0xFF);
		out[idx++] = (uint8_t)(p->max_info_rx & 0xFF);
	} else {
		out[idx++] = 0x01;
		out[idx++] = (uint8_t)(p->max_info_rx & 0xFF);
	}

	/* Parameter 7: window size tx (4 bytes) */
	out[idx++] = 0x07;
	out[idx++] = 0x04;
	out[idx++] = 0x00;
	out[idx++] = 0x00;
	out[idx++] = 0x00;
	out[idx++] = (uint8_t)(p->window_tx & 0xFF);

	/* Parameter 8: window size rx (4 bytes) */
	out[idx++] = 0x08;
	out[idx++] = 0x04;
	out[idx++] = 0x00;
	out[idx++] = 0x00;
	out[idx++] = 0x00;
	out[idx++] = (uint8_t)(p->window_rx & 0xFF);

	/* Fill group length */
	out[group_len_pos] = (uint8_t)(idx - group_len_pos - 1);

	return idx;
}

static void decode_xid_params(const uint8_t *data, uint16_t len, osp_hdlc_xid_params_t *p) {
	uint16_t idx = 0;

	/* Skip format ID (81) + group ID (80) + group length */
	if (len >= 3 && data[0] == 0x81 && data[1] == 0x80) {
		idx = 3; /* skip 81 80 + group_length byte */
	}

	while (idx + 2 <= len) {
		uint8_t param_id = data[idx];
		uint8_t param_len = data[idx + 1];
		idx += 2;

		if (idx + param_len > len)
			break;

		switch (param_id) {
			case 0x05: /* max info tx — 1 or 2 bytes, big-endian */
				if (param_len == 2)
					p->max_info_tx = ((uint16_t)data[idx] << 8) | data[idx + 1];
				else if (param_len >= 1)
					p->max_info_tx = data[idx];
				break;
			case 0x06: /* max info rx — 1 or 2 bytes, big-endian */
				if (param_len == 2)
					p->max_info_rx = ((uint16_t)data[idx] << 8) | data[idx + 1];
				else if (param_len >= 1)
					p->max_info_rx = data[idx];
				break;
			case 0x07: /* window tx — 4 bytes */
				if (param_len >= 4)
					p->window_tx = ((uint32_t)data[idx] << 24) | ((uint32_t)data[idx + 1] << 16) | ((uint32_t)data[idx + 2] << 8) | data[idx + 3];
				break;
			case 0x08: /* window rx — 4 bytes */
				if (param_len >= 4)
					p->window_rx = ((uint32_t)data[idx] << 24) | ((uint32_t)data[idx + 1] << 16) | ((uint32_t)data[idx + 2] << 8) | data[idx + 3];
				break;
		}

		idx += param_len;
	}
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Init
 * ═══════════════════════════════════════════════════════════════════════════ */

void osp_hdlc_session_init_client(osp_hdlc_session_t *s, osp_transport_t *t,
                                   uint32_t client_addr, uint8_t client_addr_len,
                                   uint32_t server_addr, uint8_t server_addr_len) {
	memset(s, 0, sizeof(*s));
	s->transport = t;
	s->is_client = true;
	s->state = OSP_HDLC_STATE_IDLE;
	osp_hdlc_address_init(&s->client_addr, client_addr, client_addr_len);
	osp_hdlc_address_init(&s->server_addr, server_addr, server_addr_len);
	/* Default XID parameters */
	s->xid.max_info_tx = 1280;
	s->xid.max_info_rx = 1280;
	s->xid.window_tx = 1;
	s->xid.window_rx = 1;
	/* Default retransmission: 3 retries */
	s->max_retransmits = 3;
}

void osp_hdlc_session_init_server(osp_hdlc_session_t *s, osp_transport_t *t,
                                   uint32_t server_addr, uint8_t server_addr_len,
                                   uint32_t client_addr, uint8_t client_addr_len) {
	memset(s, 0, sizeof(*s));
	s->transport = t;
	s->is_client = false;
	s->state = OSP_HDLC_STATE_IDLE;
	osp_hdlc_address_init(&s->server_addr, server_addr, server_addr_len);
	osp_hdlc_address_init(&s->client_addr, client_addr, client_addr_len);
	s->xid.max_info_tx = 1280;
	s->xid.max_info_rx = 1280;
	s->xid.window_tx = 1;
	s->xid.window_rx = 1;
	s->max_retransmits = 3;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Connect: SNRM → UA
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_hdlc_session_connect(osp_hdlc_session_t *s, uint32_t timeout_ms) {
	if (!s || !s->transport)
		return OSP_ERR_INVALID;

	/* Open transport */
	osp_err_t r = s->transport->open(s->transport->ctx);
	if (r != OSP_OK)
		return r;

	if (s->is_client) {
		/* Client: send SNRM with XID parameters, wait for UA */
		s->state = OSP_HDLC_STATE_CONNECTING;

		/* Build XID info field */
		uint8_t xid_buf[32];
		uint16_t xid_len = encode_xid_params(&s->xid, xid_buf);

		/* Build SNRM frame */
		osp_hdlc_frame_t snrm;
		memset(&snrm, 0, sizeof(snrm));
		snrm.destination = s->server_addr;
		snrm.source = s->client_addr;
		snrm.control.type = OSP_HDLC_TYPE_SNRM;
		snrm.control.poll_final = true;
		memcpy(snrm.info, xid_buf, xid_len);
		snrm.info_len = xid_len;

		r = session_send_frame(s, &snrm);
		if (r != OSP_OK)
			return r;

		/* Wait for UA */
		osp_hdlc_frame_t ua;
		r = session_recv_frame(s, &ua, timeout_ms);
		if (r != OSP_OK) {
			s->state = OSP_HDLC_STATE_IDLE;
			return r;
		}

		if (ua.control.type != OSP_HDLC_TYPE_UA) {
			s->state = OSP_HDLC_STATE_IDLE;
			return OSP_ERR_INVALID;
		}

		/* Decode XID parameters from UA info field */
		if (ua.info_len > 0) {
			osp_hdlc_xid_params_t peer_xid = {0};
			decode_xid_params(ua.info, ua.info_len, &peer_xid);
			/* Negotiate: take the minimum of each parameter */
			if (peer_xid.max_info_tx > 0 && peer_xid.max_info_tx < s->xid.max_info_rx)
				s->xid.max_info_rx = peer_xid.max_info_tx;
			if (peer_xid.max_info_rx > 0 && peer_xid.max_info_rx < s->xid.max_info_tx)
				s->xid.max_info_tx = peer_xid.max_info_rx;
			if (peer_xid.window_tx > 0 && peer_xid.window_tx < s->xid.window_rx)
				s->xid.window_rx = peer_xid.window_tx;
			if (peer_xid.window_rx > 0 && peer_xid.window_rx < s->xid.window_tx)
				s->xid.window_tx = peer_xid.window_rx;
		}

		s->state = OSP_HDLC_STATE_CONNECTED;
		s->send_seq = 0;
		s->recv_seq = 0;
		return OSP_OK;

	} else {
		/* Server: wait for SNRM, send UA */
		osp_hdlc_frame_t snrm;
		r = session_recv_frame(s, &snrm, timeout_ms);
		if (r != OSP_OK)
			return r;

		if (snrm.control.type != OSP_HDLC_TYPE_SNRM) {
			return OSP_ERR_INVALID;
		}

		/* Store actual client address from SNRM */
		s->received_client_addr = snrm.source;
		/* Update server address to match SNRM destination (for I-frame responses) */
		s->server_addr = snrm.destination;

		/* Decode client XID parameters */
		if (snrm.info_len > 0) {
			osp_hdlc_xid_params_t peer_xid = {0};
			decode_xid_params(snrm.info, snrm.info_len, &peer_xid);
			if (peer_xid.max_info_tx > 0 && peer_xid.max_info_tx < s->xid.max_info_rx)
				s->xid.max_info_rx = peer_xid.max_info_tx;
			if (peer_xid.max_info_rx > 0 && peer_xid.max_info_rx < s->xid.max_info_tx)
				s->xid.max_info_tx = peer_xid.max_info_rx;
			if (peer_xid.window_tx > 0 && peer_xid.window_tx < s->xid.window_rx)
				s->xid.window_rx = peer_xid.window_tx;
			if (peer_xid.window_rx > 0 && peer_xid.window_rx < s->xid.window_tx)
				s->xid.window_tx = peer_xid.window_rx;
		}

		/* Send UA with our XID parameters */
		uint8_t xid_buf[32];
		uint16_t xid_len = encode_xid_params(&s->xid, xid_buf);

		osp_hdlc_frame_t ua;
		memset(&ua, 0, sizeof(ua));
		ua.destination = snrm.source;
		ua.source = snrm.destination;
		ua.control.type = OSP_HDLC_TYPE_UA;
		ua.control.poll_final = true;
		memcpy(ua.info, xid_buf, xid_len);
		ua.info_len = xid_len;

		r = session_send_frame(s, &ua);
		if (r != OSP_OK)
			return r;

		s->state = OSP_HDLC_STATE_CONNECTED;
		s->send_seq = 0;
		s->recv_seq = 0;
		return OSP_OK;
	}
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Disconnect: DISC → UA/DM
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_hdlc_session_disconnect(osp_hdlc_session_t *s, uint32_t timeout_ms) {
	if (!s || !s->transport || s->state != OSP_HDLC_STATE_CONNECTED)
		return OSP_ERR_INVALID;

	s->state = OSP_HDLC_STATE_DISCONNECTING;

	osp_hdlc_frame_t disc;
	memset(&disc, 0, sizeof(disc));
	if (s->is_client) {
		disc.destination = s->server_addr;
		disc.source = s->client_addr;
	} else {
		disc.destination = s->client_addr;
		disc.source = s->server_addr;
	}
	disc.control.type = OSP_HDLC_TYPE_DISC;
	disc.control.poll_final = true;

	osp_err_t r = session_send_frame(s, &disc);
	if (r != OSP_OK) {
		s->state = OSP_HDLC_STATE_IDLE;
		return r;
	}

	/* Wait for UA or DM */
	osp_hdlc_frame_t resp;
	r = session_recv_frame(s, &resp, timeout_ms);
	if (r != OSP_OK) {
		s->state = OSP_HDLC_STATE_IDLE;
		return r;
	}

	if (resp.control.type == OSP_HDLC_TYPE_UA || resp.control.type == OSP_HDLC_TYPE_DM) {
		s->state = OSP_HDLC_STATE_IDLE;
		s->transport->close(s->transport->ctx);
		return OSP_OK;
	}

	s->state = OSP_HDLC_STATE_IDLE;
	return OSP_ERR_INVALID;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Send APDU: prepend LLC header, wrap in I-frame with N(S)/N(R)
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_hdlc_session_send_apdu(osp_hdlc_session_t *s, const uint8_t *apdu, uint32_t apdu_len) {
	if (!s || s->state != OSP_HDLC_STATE_CONNECTED || !apdu)
		return OSP_ERR_INVALID;

	/* LLC header + APDU must fit negotiated max_info_tx and frame buffer */
	const uint8_t *llc = s->is_client ? LLC_COMMAND : LLC_RESPONSE;
	uint32_t info_len = 3 + apdu_len;
	uint32_t max_info = s->xid.max_info_tx > 0 ? s->xid.max_info_tx : OSP_HDLC_MAX_FRAME_SIZE;
	if (max_info > OSP_HDLC_MAX_FRAME_SIZE) {
		max_info = OSP_HDLC_MAX_FRAME_SIZE;
	}
	if (info_len > max_info)
		return OSP_ERR_NOMEM;

	osp_hdlc_frame_t frame;
	memset(&frame, 0, sizeof(frame));
	if (s->is_client) {
		frame.destination = s->server_addr;
		frame.source = s->client_addr;
	} else {
		frame.destination = s->client_addr;
		frame.source = s->server_addr;
	}
	frame.control.type = OSP_HDLC_TYPE_I;
	frame.control.send_seq = s->send_seq;
	frame.control.recv_seq = s->recv_seq;
	frame.control.poll_final = true;

	memcpy(frame.info, llc, 3);
	memcpy(frame.info + 3, apdu, apdu_len);
	frame.info_len = info_len;

	osp_err_t r = session_send_frame(s, &frame);
	if (r != OSP_OK)
		return r;

	/* Save frame for potential retransmission on REJ */
	if (info_len <= OSP_HDLC_MAX_FRAME_SIZE) {
		memcpy(s->last_sent_info, frame.info, info_len);
		s->last_sent_info_len = info_len;
		s->last_sent_seq = s->send_seq;
		s->has_pending_retransmit = true;
	}

	s->send_seq = (s->send_seq + 1) & 0x07;
	return OSP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Receive APDU: deframe I-frame, strip LLC, update N(R)
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_hdlc_session_recv_apdu(osp_hdlc_session_t *s, uint8_t *buf, uint32_t buf_size,
                                      uint32_t *apdu_len, uint32_t timeout_ms) {
	if (!s || s->state != OSP_HDLC_STATE_CONNECTED || !buf || !apdu_len)
		return OSP_ERR_INVALID;

	osp_hdlc_frame_t frame;
	osp_err_t r = session_recv_frame(s, &frame, timeout_ms);
	if (r != OSP_OK)
		return r;

	if (frame.control.type != OSP_HDLC_TYPE_I) {
		/* Handle retransmitted SNRM — re-send UA */
		if (frame.control.type == OSP_HDLC_TYPE_SNRM) {
			osp_hdlc_frame_t ua;
			memset(&ua, 0, sizeof(ua));
			ua.destination = frame.source;
			ua.source = frame.destination;
			ua.control.type = OSP_HDLC_TYPE_UA;
			ua.control.poll_final = true;
			session_send_frame(s, &ua);
			return OSP_ERR_TIMEOUT; /* Re-wait for APDU */
		}
		/* Handle DISC — send UA, then signal disconnect */
		if (frame.control.type == OSP_HDLC_TYPE_DISC) {
			osp_hdlc_frame_t ua;
			memset(&ua, 0, sizeof(ua));
			ua.destination = frame.source;
			ua.source = frame.destination;
			ua.control.type = OSP_HDLC_TYPE_UA;
			ua.control.poll_final = true;
			session_send_frame(s, &ua);
			s->state = OSP_HDLC_STATE_IDLE;
			return OSP_ERR_DISCONNECTED;
		}
		/* Handle S-frames (RR/RNR/REJ) */
		if (frame.control.type == OSP_HDLC_TYPE_RR) {
			/* Acknowledgment — clear retransmission buffer */
			s->has_pending_retransmit = false;
			return OSP_ERR_TIMEOUT; /* No data frame received */
		}
		if (frame.control.type == OSP_HDLC_TYPE_REJ) {
			/* REJ received — retransmit the last sent frame */
			if (s->has_pending_retransmit && s->max_retransmits > 0) {
				osp_hdlc_frame_t retransmit;
				memset(&retransmit, 0, sizeof(retransmit));
				if (s->is_client) {
					retransmit.destination = s->server_addr;
					retransmit.source = s->client_addr;
				} else {
					retransmit.destination = s->client_addr;
					retransmit.source = s->server_addr;
				}
				retransmit.control.type = OSP_HDLC_TYPE_I;
				retransmit.control.send_seq = s->last_sent_seq;
				retransmit.control.recv_seq = s->recv_seq;
				retransmit.control.poll_final = true;
				memcpy(retransmit.info, s->last_sent_info, s->last_sent_info_len);
				retransmit.info_len = s->last_sent_info_len;
				session_send_frame(s, &retransmit);
				s->has_pending_retransmit = false;
			}
			return OSP_ERR_TIMEOUT;
		}
		return OSP_ERR_INVALID;
	}

	/* Check for N(S) mismatch — send REJ if out of order */
	if (frame.control.send_seq != s->recv_seq) {
		/* Send REJ requesting correct sequence */
		osp_hdlc_frame_t rej;
		memset(&rej, 0, sizeof(rej));
		if (s->is_client) {
			rej.destination = s->server_addr;
			rej.source = s->client_addr;
		} else {
			rej.destination = s->client_addr;
			rej.source = s->server_addr;
		}
		rej.control.type = OSP_HDLC_TYPE_REJ;
		rej.control.recv_seq = s->recv_seq;
		rej.control.poll_final = true;
		session_send_frame(s, &rej);
		return OSP_ERR_INVALID;
	}

	/* Update N(R): acknowledge the received frame */
	s->recv_seq = (frame.control.send_seq + 1) & 0x07;

	/* Strip LLC header (3 bytes: E6 E6 00 or E6 E7 00) */
	if (frame.info_len < 3) {
		return OSP_ERR_INVALID;
	}
	if (frame.info[0] != 0xE6 || (frame.info[1] != 0xE6 && frame.info[1] != 0xE7) || frame.info[2] != 0x00) {
		/* No LLC header — return raw info field */
		if (frame.info_len > buf_size)
			return OSP_ERR_NOMEM;
		memcpy(buf, frame.info, frame.info_len);
		*apdu_len = frame.info_len;
		return OSP_OK;
	}

	uint32_t apdu_size = frame.info_len - 3;
	if (apdu_size > buf_size)
		return OSP_ERR_NOMEM;
	memcpy(buf, frame.info + 3, apdu_size);
	*apdu_len = apdu_size;
	return OSP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  State query
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_hdlc_state_t osp_hdlc_session_state(const osp_hdlc_session_t *s) {
	return s ? s->state : OSP_HDLC_STATE_IDLE;
}
