/**
 * hdlc_session.c — HDLC link-layer session (IEC 62056-46)
 *
 * Implements SNRM/UA connection setup, I-frame send/receive with
 * N(S)/N(R) tracking, LLC header handling, and DISC/DM teardown.
 */

#include "hdlc_session.h"
#include <string.h>
#include <time.h>

/* LLC header constants (IEC 62056-47) */
static const uint8_t LLC_COMMAND[3] = {0xE6, 0xE6, 0x00};  /* client → server */
static const uint8_t LLC_RESPONSE[3] = {0xE6, 0xE7, 0x00}; /* server → client */

static osp_err_t session_send_frame(osp_hdlc_session_t *s, const osp_hdlc_frame_t *frame);
static osp_err_t session_send_frmr(osp_hdlc_session_t *s, const osp_hdlc_address_t *dst,
                                   uint8_t rejected_ctrl, uint8_t reasons);

/* ISO 13239 FRMR reason bits (info octet 3, bit1=w … bit4=z) */
#define OSP_FRMR_W 0x01 /* undefined / not implemented */
#define OSP_FRMR_X 0x02 /* info field not permitted (with W) */
#define OSP_FRMR_Y 0x04 /* info field too long */
#define OSP_FRMR_Z 0x08 /* invalid N(R) */

#define SESSION_POLL_CHUNK_MS 60000u /* recv slice when waiting forever */

static uint32_t session_now_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

static uint32_t session_deadline_ms(uint32_t frame_timeout_ms) {
	if (frame_timeout_ms == OSP_HDLC_WAIT_FOREVER_MS)
		return UINT32_MAX;
	return session_now_ms() + frame_timeout_ms;
}

static uint32_t session_remaining_ms(uint32_t deadline) {
	if (deadline == UINT32_MAX)
		return UINT32_MAX;
	uint32_t now = session_now_ms();
	return now >= deadline ? 0 : deadline - now;
}

static uint32_t session_poll_wait_ms(uint32_t wait_ms) {
	if (wait_ms == UINT32_MAX)
		return SESSION_POLL_CHUNK_MS;
	return wait_ms;
}

uint16_t osp_hdlc_normalize_inter_octet_ms(uint16_t ms) {
	if (ms == 0)
		return OSP_HDLC_INTER_OCTET_DEFAULT_MS;
	if (ms < OSP_HDLC_INTER_OCTET_MIN_MS)
		return OSP_HDLC_INTER_OCTET_MIN_MS;
	if (ms > OSP_HDLC_INTER_OCTET_MAX_MS)
		return OSP_HDLC_INTER_OCTET_MAX_MS;
	return ms;
}

uint16_t osp_hdlc_normalize_inactivity_s(uint16_t seconds) {
	if (seconds > OSP_HDLC_INACTIVITY_MAX_S)
		return OSP_HDLC_INACTIVITY_MAX_S;
	return seconds;
}

uint32_t osp_hdlc_inactivity_wait_ms(uint16_t inactivity_s) {
	inactivity_s = osp_hdlc_normalize_inactivity_s(inactivity_s);
	if (inactivity_s == 0)
		return OSP_HDLC_WAIT_FOREVER_MS;
	return (uint32_t)inactivity_s * 1000u;
}

static void session_notify_timeout(osp_hdlc_session_t *s, osp_hdlc_timeout_kind_t kind, uint32_t param) {
	if (s && s->on_timeout)
		s->on_timeout(s->timeout_ctx, kind, param);
}

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

static void session_rx_consume(osp_hdlc_session_t *s, uint32_t consumed, uint32_t *total) {
	if (*total > consumed) {
		uint32_t rest = *total - consumed;
		memmove(s->rx_buf, s->rx_buf + consumed, rest);
		s->rx_pending_len = rest;
		*total = rest;
	} else {
		s->rx_pending_len = 0;
		*total = 0;
	}
}

static osp_err_t session_recv_frame(osp_hdlc_session_t *s, osp_hdlc_frame_t *frame, uint32_t frame_timeout_ms) {
	uint32_t total = s->rx_pending_len;
	uint16_t inter_ms = osp_hdlc_normalize_inter_octet_ms(s->inter_octet_timeout_ms);
	uint32_t deadline = session_deadline_ms(frame_timeout_ms);

	for (;;) {
		/* Parse complete frames from data already buffered (incl. rx_pending). */
		for (;;) {
			uint32_t start = 0;
			while (start < total && s->rx_buf[start] != OSP_HDLC_FLAG)
				start++;
			if (start > 0) {
				memmove(s->rx_buf, s->rx_buf + start, total - start);
				total -= start;
			}
			if (total < 3)
				break;

			if ((s->rx_buf[1] & 0xF0) != 0xA0) {
				memmove(s->rx_buf, s->rx_buf + 1, total - 1);
				total -= 1;
				continue;
			}

			uint16_t flen = ((uint16_t)(s->rx_buf[1] & 0x07) << 8) | s->rx_buf[2];
			uint32_t need = 1u + (uint32_t)flen + 1u;
			if (need > sizeof(s->rx_buf)) {
				memmove(s->rx_buf, s->rx_buf + 1, total - 1);
				total -= 1;
				continue;
			}
			if (total < need)
				break;

			if (s->rx_buf[need - 1] != OSP_HDLC_FLAG) {
				memmove(s->rx_buf, s->rx_buf + 1, total - 1);
				total -= 1;
				continue;
			}

			osp_err_t dr = osp_hdlc_deframe(s->rx_buf, need, frame);
			session_rx_consume(s, need, &total);
			if (dr == OSP_ERR_UNSUPPORTED) {
				if (s->state == OSP_HDLC_STATE_CONNECTED)
					session_send_frmr(s, &frame->source, frame->control_raw, OSP_FRMR_W);
				return OSP_ERR_TIMEOUT;
			}
			return dr;
		}

		if (total >= sizeof(s->rx_buf))
			return OSP_ERR_NOMEM;

		uint32_t remaining = session_remaining_ms(deadline);
		if (frame_timeout_ms != OSP_HDLC_WAIT_FOREVER_MS && remaining == 0) {
			s->rx_pending_len = total;
			session_notify_timeout(s, OSP_HDLC_TIMEOUT_INTER_FRAME, s->inactivity_timeout_s);
			return OSP_ERR_TIMEOUT;
		}

		/* Межсимвольный: пауза между октетами внутри кадра; межкадровый: ожидание начала кадра. */
		uint32_t wait_ms = total > 0 ? inter_ms : remaining;
		wait_ms = session_poll_wait_ms(wait_ms);

		s->rx_pending_len = total;
		uint32_t n = 0;
		osp_err_t r = s->transport->recv(s->transport->ctx, s->rx_buf + total,
		                                 (uint32_t)(sizeof(s->rx_buf) - total), &n, wait_ms);
		if (r == OSP_ERR_TIMEOUT) {
			if (total > 0) {
				/* Межсимвольный истёк: неполный кадр считается завершённым, отбрасываем. */
				session_notify_timeout(s, OSP_HDLC_TIMEOUT_INTER_OCTET, inter_ms);
				s->rx_pending_len = 0;
				total = 0;
				continue;
			}
			if (frame_timeout_ms == OSP_HDLC_WAIT_FOREVER_MS)
				continue;
			s->rx_pending_len = 0;
			session_notify_timeout(s, OSP_HDLC_TIMEOUT_INTER_FRAME, s->inactivity_timeout_s);
			return OSP_ERR_TIMEOUT;
		}
		if (r != OSP_OK)
			return r;
		if (n == 0)
			return OSP_ERR_IO;

		total += n;
	}
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

	/* Parameter 5: max info field length tx (always 2 bytes — etalon UA style) */
	out[idx++] = 0x05;
	out[idx++] = 0x02;
	out[idx++] = (uint8_t)((p->max_info_tx >> 8) & 0xFF);
	out[idx++] = (uint8_t)(p->max_info_tx & 0xFF);

	/* Parameter 6: max info field length rx (always 2 bytes) */
	out[idx++] = 0x06;
	out[idx++] = 0x02;
	out[idx++] = (uint8_t)((p->max_info_rx >> 8) & 0xFF);
	out[idx++] = (uint8_t)(p->max_info_rx & 0xFF);

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

static void session_set_state(osp_hdlc_session_t *s, osp_hdlc_state_t new_state) {
	if (!s || s->state == new_state)
		return;
	osp_hdlc_state_t old = s->state;
	s->state = new_state;
	if (s->on_state_change)
		s->on_state_change(s->state_change_ctx, old, new_state);
}

static void session_enter_ndm(osp_hdlc_session_t *s) {
	session_set_state(s, OSP_HDLC_STATE_IDLE);
	s->send_seq = 0;
	s->recv_seq = 0;
	s->has_pending_retransmit = false;
	s->reassembly_len = 0;
	s->rx_pending_len = 0;
}

static void session_reset_xid(osp_hdlc_session_t *s) {
	s->xid = s->xid_configured;
}

static void negotiate_xid(osp_hdlc_session_t *s, const uint8_t *info, uint16_t info_len) {
	if (info_len == 0)
		return;
	osp_hdlc_xid_params_t peer_xid = {0};
	decode_xid_params(info, info_len, &peer_xid);
	if (peer_xid.max_info_tx > 0 && peer_xid.max_info_tx < s->xid.max_info_rx)
		s->xid.max_info_rx = peer_xid.max_info_tx;
	if (peer_xid.max_info_rx > 0 && peer_xid.max_info_rx < s->xid.max_info_tx)
		s->xid.max_info_tx = peer_xid.max_info_rx;
	if (peer_xid.window_tx > 0 && peer_xid.window_tx < s->xid.window_rx)
		s->xid.window_rx = (uint8_t)peer_xid.window_tx;
	if (peer_xid.window_rx > 0 && peer_xid.window_rx < s->xid.window_tx)
		s->xid.window_tx = (uint8_t)peer_xid.window_rx;
}

static osp_err_t session_send_frmr(osp_hdlc_session_t *s, const osp_hdlc_address_t *dst,
                                   uint8_t rejected_ctrl, uint8_t reasons) {
	osp_hdlc_frame_t frmr;
	memset(&frmr, 0, sizeof(frmr));
	frmr.destination = *dst;
	frmr.source = s->is_client ? s->client_addr : s->server_addr;
	frmr.control.type = OSP_HDLC_TYPE_FRMR;
	frmr.control.poll_final = true;
	/* Modulo-8 FRMR info (ISO 13239 fig. 18a): ctrl | V(S)|C/R|V(R) | wxyz */
	frmr.info[0] = rejected_ctrl;
	frmr.info[1] = (uint8_t)((s->send_seq & 0x07) | ((s->recv_seq & 0x07) << 5));
	frmr.info[2] = (uint8_t)(reasons & 0x0F);
	frmr.info_len = 3;
	return session_send_frame(s, &frmr);
}

static bool frame_allows_info(osp_hdlc_frame_type_t type) {
	switch (type) {
		case OSP_HDLC_TYPE_I:
		case OSP_HDLC_TYPE_UI:
		case OSP_HDLC_TYPE_XID:
		case OSP_HDLC_TYPE_SNRM:
		case OSP_HDLC_TYPE_UA:
		case OSP_HDLC_TYPE_DM:
		case OSP_HDLC_TYPE_FRMR:
			return true;
		default:
			return false; /* RR/RNR/REJ/DISC/… */
	}
}

static osp_err_t session_send_rr(osp_hdlc_session_t *s, const osp_hdlc_address_t *dst) {
	osp_hdlc_frame_t rr;
	memset(&rr, 0, sizeof(rr));
	rr.destination = *dst;
	rr.source = s->is_client ? s->client_addr : s->server_addr;
	rr.control.type = OSP_HDLC_TYPE_RR;
	rr.control.recv_seq = s->recv_seq;
	rr.control.poll_final = true;
	return session_send_frame(s, &rr);
}

static osp_err_t session_send_dm(osp_hdlc_session_t *s, const osp_hdlc_address_t *dst) {
	osp_hdlc_frame_t dm;
	memset(&dm, 0, sizeof(dm));
	dm.destination = *dst;
	dm.source = s->server_addr;
	dm.control.type = OSP_HDLC_TYPE_DM;
	dm.control.poll_final = true;
	return session_send_frame(s, &dm);
}

static osp_err_t session_send_ua_xid(osp_hdlc_session_t *s, const osp_hdlc_address_t *dst) {
	uint8_t xid_buf[32];
	uint16_t xid_len = encode_xid_params(&s->xid, xid_buf);
	osp_hdlc_frame_t ua;
	memset(&ua, 0, sizeof(ua));
	ua.destination = *dst;
	ua.source = s->server_addr;
	ua.control.type = OSP_HDLC_TYPE_UA;
	ua.control.poll_final = true;
	memcpy(ua.info, xid_buf, xid_len);
	ua.info_len = xid_len;
	return session_send_frame(s, &ua);
}

/* DISC: NRM (CONNECTED) → UA + NDM; NDM (IDLE) → DM, stay NDM. */
static osp_err_t session_handle_disc(osp_hdlc_session_t *s, const osp_hdlc_address_t *src) {
	if (s->state == OSP_HDLC_STATE_CONNECTED) {
		osp_err_t r = session_send_ua_xid(s, src);
		if (r != OSP_OK)
			return r;
		session_enter_ndm(s);
		return OSP_ERR_DISCONNECTED;
	}
	return session_send_dm(s, src);
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
	session_set_state(s, OSP_HDLC_STATE_IDLE);
	osp_hdlc_address_init(&s->client_addr, client_addr, client_addr_len);
	osp_hdlc_address_init(&s->server_addr, server_addr, server_addr_len);
	/* Default XID parameters */
	s->xid_configured.max_info_tx = 1280;
	s->xid_configured.max_info_rx = 1280;
	s->xid_configured.window_tx = 1;
	s->xid_configured.window_rx = 1;
	s->xid = s->xid_configured;
	s->inter_octet_timeout_ms = OSP_HDLC_INTER_OCTET_DEFAULT_MS;
	s->inactivity_timeout_s = 120;
	/* Default retransmission: 3 retries */
	s->max_retransmits = 3;
}

void osp_hdlc_session_init_server(osp_hdlc_session_t *s, osp_transport_t *t,
                                   uint32_t server_addr, uint8_t server_addr_len,
                                   uint32_t client_addr, uint8_t client_addr_len) {
	memset(s, 0, sizeof(*s));
	s->transport = t;
	s->is_client = false;
	session_set_state(s, OSP_HDLC_STATE_IDLE);
	osp_hdlc_address_init(&s->server_addr, server_addr, server_addr_len);
	osp_hdlc_address_init(&s->client_addr, client_addr, client_addr_len);
	s->xid_configured.max_info_tx = 512;
	s->xid_configured.max_info_rx = 512;
	s->xid_configured.window_tx = 1;
	s->xid_configured.window_rx = 1;
	s->xid = s->xid_configured;
	s->inter_octet_timeout_ms = OSP_HDLC_INTER_OCTET_DEFAULT_MS;
	s->inactivity_timeout_s = 120;
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
		session_set_state(s, OSP_HDLC_STATE_CONNECTING);

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
			session_set_state(s, OSP_HDLC_STATE_IDLE);
			return r;
		}

		if (ua.control.type != OSP_HDLC_TYPE_UA) {
			session_set_state(s, OSP_HDLC_STATE_IDLE);
			return OSP_ERR_INVALID;
		}

		/* Decode XID parameters from UA info field */
		if (ua.info_len > 0) {
			negotiate_xid(s, ua.info, ua.info_len);
		}

		session_set_state(s, OSP_HDLC_STATE_CONNECTED);
		s->send_seq = 0;
		s->recv_seq = 0;
		s->reassembly_len = 0;
		return OSP_OK;

	} else {
		/* Server in NDM: SNRM→UA; DISC→DM (stay in NDM); I/UI ignored. */
		session_set_state(s, OSP_HDLC_STATE_IDLE);

		for (;;) {
			osp_hdlc_frame_t frame;
			r = session_recv_frame(s, &frame, timeout_ms);
			if (r == OSP_ERR_TIMEOUT)
				return OSP_ERR_TIMEOUT;
			/* Bad/partial frame in NDM: resync and keep waiting (DISC may follow). */
			if (r == OSP_ERR_INVALID)
				continue;
			if (r != OSP_OK)
				return r;

			if (frame.control.type == OSP_HDLC_TYPE_SNRM) {
				if (osp_hdlc_address_value(&frame.destination) != osp_hdlc_address_value(&s->server_addr))
					continue;

				s->received_client_addr = frame.source;
				s->client_addr = frame.source;
				session_reset_xid(s);
				negotiate_xid(s, frame.info, frame.info_len);

				r = session_send_ua_xid(s, &frame.source);
				if (r != OSP_OK)
					return r;

				session_set_state(s, OSP_HDLC_STATE_CONNECTED);
				s->send_seq = 0;
				s->recv_seq = 0;
				s->reassembly_len = 0;
				s->has_pending_retransmit = false;
				return OSP_OK;
			}

			if (frame.control.type == OSP_HDLC_TYPE_DISC) {
				r = session_handle_disc(s, &frame.source);
				if (r == OSP_ERR_DISCONNECTED)
					return r;
				if (r != OSP_OK)
					return r;
				continue;
			}

			/* Other commands in NDM (I/UI/…): no mandatory response. */
		}
	}
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Disconnect: DISC → UA/DM
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_err_t osp_hdlc_session_disconnect(osp_hdlc_session_t *s, uint32_t timeout_ms) {
	if (!s || !s->transport || s->state != OSP_HDLC_STATE_CONNECTED)
		return OSP_ERR_INVALID;

	session_set_state(s, OSP_HDLC_STATE_DISCONNECTING);

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
		session_set_state(s, OSP_HDLC_STATE_IDLE);
		return r;
	}

	/* Wait for UA or DM */
	osp_hdlc_frame_t resp;
	r = session_recv_frame(s, &resp, timeout_ms);
	if (r != OSP_OK) {
		session_set_state(s, OSP_HDLC_STATE_IDLE);
		return r;
	}

	if (resp.control.type == OSP_HDLC_TYPE_UA || resp.control.type == OSP_HDLC_TYPE_DM) {
		session_set_state(s, OSP_HDLC_STATE_IDLE);
		s->transport->close(s->transport->ctx);
		return OSP_OK;
	}

	session_set_state(s, OSP_HDLC_STATE_IDLE);
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

	/* Info field present when control format forbids it → FRMR (W+X). */
	if (frame.info_len > 0 && !frame_allows_info(frame.control.type)) {
		session_send_frmr(s, &frame.source, frame.control_raw, (uint8_t)(OSP_FRMR_W | OSP_FRMR_X));
		return OSP_ERR_TIMEOUT;
	}

	if (frame.control.type != OSP_HDLC_TYPE_I) {
		/* UI: no mandatory reply; drop (may carry info within max). */
		if (frame.control.type == OSP_HDLC_TYPE_UI) {
			uint16_t max_rx = s->xid.max_info_rx > 0 ? s->xid.max_info_rx : OSP_HDLC_MAX_FRAME_SIZE;
			if (frame.info_len > max_rx)
				session_send_frmr(s, &frame.source, frame.control_raw, OSP_FRMR_Y);
			return OSP_ERR_TIMEOUT;
		}
		/* SNRM while NRM: reset XID from configured ceiling, UA+XID, seq=0. */
		if (frame.control.type == OSP_HDLC_TYPE_SNRM) {
			if (osp_hdlc_address_value(&frame.destination) != osp_hdlc_address_value(&s->server_addr)) {
				return OSP_ERR_TIMEOUT; /* not for us — keep waiting */
			}
			s->received_client_addr = frame.source;
			s->client_addr = frame.source;
			session_reset_xid(s);
			negotiate_xid(s, frame.info, frame.info_len);
			session_send_ua_xid(s, &frame.source);
			s->send_seq = 0;
			s->recv_seq = 0;
			s->has_pending_retransmit = false;
			s->reassembly_len = 0;
			return OSP_ERR_TIMEOUT; /* Re-wait for APDU */
		}
		/* DISC in NRM → UA, drop to NDM (keep transport; new SNRM may follow). */
		if (frame.control.type == OSP_HDLC_TYPE_DISC)
			return session_handle_disc(s, &frame.source);
		/* RR / RNR: invalid N(R) → FRMR (Z). */
		if (frame.control.type == OSP_HDLC_TYPE_RR || frame.control.type == OSP_HDLC_TYPE_RNR) {
			if (frame.control.recv_seq != s->send_seq) {
				session_send_frmr(s, &frame.source, frame.control_raw, OSP_FRMR_Z);
				return OSP_ERR_TIMEOUT;
			}
			if (frame.control.type == OSP_HDLC_TYPE_RR)
				s->has_pending_retransmit = false;
			return OSP_ERR_TIMEOUT;
		}
		if (frame.control.type == OSP_HDLC_TYPE_REJ) {
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

	/* I-frame: info longer than negotiated max_info_rx → FRMR (Y). */
	uint16_t max_rx = s->xid.max_info_rx > 0 ? s->xid.max_info_rx : OSP_HDLC_MAX_FRAME_SIZE;
	if (frame.info_len > max_rx) {
		session_send_frmr(s, &frame.source, frame.control_raw, OSP_FRMR_Y);
		return OSP_ERR_TIMEOUT;
	}

	/* Invalid N(R) in I-frame → FRMR (Z). */
	if (frame.control.recv_seq != s->send_seq) {
		session_send_frmr(s, &frame.source, frame.control_raw, OSP_FRMR_Z);
		return OSP_ERR_TIMEOUT;
	}

	/* Wrong N(S): RR with current N(R), do not advance (etalon / HDLC_INFO_N3). */
	if (frame.control.send_seq != s->recv_seq) {
		session_send_rr(s, &frame.source);
		return OSP_ERR_TIMEOUT;
	}

	/* Append segment to MSDU reassembly buffer */
	if ((uint32_t)s->reassembly_len + frame.info_len > sizeof(s->reassembly)) {
		session_send_frmr(s, &frame.source, frame.control_raw, OSP_FRMR_Y);
		s->reassembly_len = 0;
		return OSP_ERR_TIMEOUT;
	}
	memcpy(s->reassembly + s->reassembly_len, frame.info, frame.info_len);
	s->reassembly_len = (uint16_t)(s->reassembly_len + frame.info_len);
	s->recv_seq = (frame.control.send_seq + 1) & 0x07;

	/* More segments follow — RR and wait for next. */
	if (frame.segmented) {
		session_send_rr(s, &frame.source);
		return OSP_ERR_TIMEOUT;
	}

	/* Final segment: deliver complete MSDU */
	const uint8_t *msdu = s->reassembly;
	uint16_t msdu_len = s->reassembly_len;
	s->reassembly_len = 0;

	if (msdu_len < 3) {
		return OSP_ERR_INVALID;
	}
	if (msdu[0] == 0xE6 && (msdu[1] == 0xE6 || msdu[1] == 0xE7) && msdu[2] == 0x00) {
		msdu += 3;
		msdu_len = (uint16_t)(msdu_len - 3);
	}

	if (msdu_len > buf_size)
		return OSP_ERR_NOMEM;
	memcpy(buf, msdu, msdu_len);
	*apdu_len = msdu_len;
	return OSP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  State query
 * ═══════════════════════════════════════════════════════════════════════════ */

osp_hdlc_state_t osp_hdlc_session_state(const osp_hdlc_session_t *s) {
	return s ? s->state : OSP_HDLC_STATE_IDLE;
}

const char *osp_hdlc_state_name(osp_hdlc_state_t state) {
	switch (state) {
		case OSP_HDLC_STATE_IDLE:
			return "NDM";
		case OSP_HDLC_STATE_CONNECTING:
			return "CONNECTING";
		case OSP_HDLC_STATE_CONNECTED:
			return "NRM";
		case OSP_HDLC_STATE_DISCONNECTING:
			return "DISCONNECTING";
		default:
			return "?";
	}
}

void osp_hdlc_session_set_state_callback(osp_hdlc_session_t *s, osp_hdlc_state_fn cb, void *ctx) {
	if (!s)
		return;
	s->on_state_change = cb;
	s->state_change_ctx = ctx;
}

void osp_hdlc_session_set_timeout_callback(osp_hdlc_session_t *s, osp_hdlc_timeout_fn cb, void *ctx) {
	if (!s)
		return;
	s->on_timeout = cb;
	s->timeout_ctx = ctx;
}

void osp_hdlc_session_enter_ndm(osp_hdlc_session_t *s) {
	if (s)
		session_enter_ndm(s);
}
