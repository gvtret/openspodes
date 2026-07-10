#include "../service/service.h"
/**
 * server.c — DLMS/COSEM server request dispatcher
 */

#include "server.h"
#include <string.h>
#include <stdio.h>

osp_err_t osp_server_init(osp_server_t *s, osp_transport_t *transport, osp_framing_type_t framing) {
	if (!s || !transport) {
		return OSP_ERR_INVALID;
	}
	memset(s, 0, sizeof(*s));
	s->transport = transport;
	s->framing = framing;
	s->associated = false;
	osp_dispatcher_init(&s->dispatcher);
	return OSP_OK;
}

osp_err_t osp_server_register(osp_server_t *s, const osp_ic_class_t *cls, void *instance) {
	if (!s || !cls || !instance) {
		return OSP_ERR_INVALID;
	}
	return osp_dispatcher_register(&s->dispatcher, cls, instance);
}

void osp_server_set_security(osp_server_t *s, const osp_sec_context_t *sec) {
	if (s && sec) {
		s->security = *sec;
	}
}

/* ── Send response ───────────────────────────────────────────────────────── */

static osp_err_t server_send(osp_server_t *s, const uint8_t *data, uint32_t len) {
	return osp_transport_send_apdu(s->transport, s->framing, data, len);
}

/* ── Handle AARQ ─────────────────────────────────────────────────────────── */

static osp_err_t handle_aarq(osp_server_t *s, const osp_aarq_t *aarq) {
	osp_aare_t aare;
	memset(&aare, 0, sizeof(aare));

	/* Accept if mechanism is lowest or LLS, or if we have matching keys */
	if (aarq->mechanism == OSP_MECH_LOWEST || aarq->mechanism == OSP_MECH_LLS) {
		aare.result = OSP_RESULT_ACCEPTED;
		s->associated = true;
	} else if (aarq->mechanism == OSP_MECH_HLS_GMAC) {
		/* Store CtoS challenge for pass 3 verification */
		memcpy(s->security.ctos, aarq->calling_auth_value, aarq->calling_auth_value_len);
		s->security.ctos_len = aarq->calling_auth_value_len;

		/* Generate StoC challenge */
		s->security.stoc_len = 8;
		for (uint8_t i = 0; i < 8; i++) {
			s->security.stoc[i] = (uint8_t)(i + 0x41);
		}
		memcpy(aare.responding_auth_value, s->security.stoc, 8);
		aare.responding_auth_value_len = 8;
		aare.result = OSP_RESULT_ACCEPTED;
		s->associated = true;
	} else {
		aare.result = OSP_RESULT_REJECTED_PERMANENT;
	}

	aare.mechanism = aarq->mechanism;
	aare.responding_ap_title_len = OSP_SEC_SYSTEM_TITLE_SIZE;
	memcpy(aare.responding_ap_title, s->security.system_title, OSP_SEC_SYSTEM_TITLE_SIZE);

	/* Build response: AARE + xDLMS InitiateResponse */
	aare.user_info[0] = 0x01; /* InitiateResponse */
	aare.user_info[1] = 0x01;
	aare.user_info_len = 2;

	osp_buf_t buf;
	osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
	if (osp_aare_encode(&aare, &buf) != 0) {
		return OSP_ERR_INVALID;
	}

	return server_send(s, buf.buf, buf.wr);
}

/* ── Handle GET ──────────────────────────────────────────────────────────── */

static osp_err_t handle_get(osp_server_t *s, const osp_get_request_t *req) {
	osp_get_response_t resp;
	memset(&resp, 0, sizeof(resp));
	resp.invoke_id_priority = req->invoke_id_priority;

	osp_value_t result;
	osp_err_t r = osp_dispatcher_get(&s->dispatcher, req->as.normal.attr.class_id, &req->as.normal.attr.instance_id, req->as.normal.attr.attribute_id, &result);
	if (r == OSP_OK) {
		resp.type = OSP_GET_RESP_DATA;
		resp.data = result;
	} else {
		resp.type = OSP_GET_RESP_DATA_ERROR;
		resp.data_access_result = OSP_DAR_OBJECT_UNDEFINED;
	}

	osp_buf_t buf;
	osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
	osp_get_response_encode(&buf, &resp);
	return server_send(s, buf.buf, buf.wr);
}

/* ── Handle SET ──────────────────────────────────────────────────────────── */

static osp_err_t handle_set(osp_server_t *s, const osp_set_request_t *req) {
	osp_set_response_t resp;
	memset(&resp, 0, sizeof(resp));
	resp.invoke_id_priority = req->invoke_id_priority;
	resp.type = OSP_GET_NORMAL;

	osp_err_t r = osp_dispatcher_set(
	    &s->dispatcher, req->as.normal.items[0].attr.class_id, &req->as.normal.items[0].attr.instance_id, req->as.normal.items[0].attr.attribute_id,
	    &req->as.normal.items[0].data
	);
	resp.as.normal.result = (r == OSP_OK) ? OSP_DAR_SUCCESS : OSP_DAR_OBJECT_UNDEFINED;

	osp_buf_t buf;
	osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
	osp_set_response_encode(&buf, &resp);
	return server_send(s, buf.buf, buf.wr);
}

/* ── Handle ACTION ───────────────────────────────────────────────────────── */

static osp_err_t handle_action(osp_server_t *s, const osp_action_request_t *req) {
	osp_action_response_t resp;
	memset(&resp, 0, sizeof(resp));
	resp.invoke_id_priority = req->invoke_id_priority;
	resp.type = OSP_GET_NORMAL;

	osp_value_t result;
	osp_err_t r = osp_dispatcher_action(
	    &s->dispatcher, req->as.normal.items[0].method.class_id, &req->as.normal.items[0].method.instance_id, req->as.normal.items[0].method.method_id,
	    &req->as.normal.items[0].data, &result
	);
	resp.as.normal.items[0].result = (r == OSP_OK) ? OSP_DAR_SUCCESS : OSP_DAR_OBJECT_UNDEFINED;
	resp.as.normal.items[0].return_data = result;
	resp.as.normal.item_count = 1;

	osp_buf_t buf;
	osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
	osp_action_response_encode(&buf, &resp);
	return server_send(s, buf.buf, buf.wr);
}

/* ── Handle HLS pass 3 (from client) ────────────────────────────────────── */

static osp_err_t handle_hls_pass3(osp_server_t *s, const osp_action_request_t *req) {
	/* Verify client's f(StoC) */
	if (req->as.normal.items[0].data.tag == OSP_TAG_OCTETSTRING && req->as.normal.items[0].data.as.octetstring.len >= 17) {
		if (osp_hls_pass3_verify(&s->security, req->as.normal.items[0].data.as.octetstring.data, 17) != 0) {
			/* Auth failed — send error response */
			osp_action_response_t resp;
			memset(&resp, 0, sizeof(resp));
			resp.invoke_id_priority = req->invoke_id_priority;
			resp.type = OSP_GET_NORMAL;
			resp.as.normal.items[0].result = OSP_DAR_AUTHORIZATION_FAILURE;
			resp.as.normal.item_count = 1;

			osp_buf_t buf;
			osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
			osp_action_response_encode(&buf, &resp);
			return server_send(s, buf.buf, buf.wr);
		}

		/* Build pass 4: f(CtoS) */
		uint8_t f_ctos[17];
		if (osp_hls_pass4_build(&s->security, f_ctos, sizeof(f_ctos)) != 17) {
			return OSP_ERR_INVALID;
		}

		/* Send ACTION response with f(CtoS) */
		osp_action_response_t resp;
		memset(&resp, 0, sizeof(resp));
		resp.invoke_id_priority = req->invoke_id_priority;
		resp.type = OSP_GET_NORMAL;
		resp.as.normal.items[0].result = OSP_DAR_SUCCESS;
		resp.as.normal.items[0].return_data.tag = OSP_TAG_OCTETSTRING;
		memcpy(resp.as.normal.items[0].return_data.as.octetstring.data, f_ctos, 17);
		resp.as.normal.items[0].return_data.as.octetstring.len = 17;
		resp.as.normal.item_count = 1;

		osp_buf_t buf;
		osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
		osp_action_response_encode(&buf, &resp);
		return server_send(s, buf.buf, buf.wr);
	}

	return OSP_ERR_INVALID;
}

/* ── Accept one request ──────────────────────────────────────────────────── */

osp_err_t osp_server_accept(osp_server_t *s, uint32_t timeout_ms) {
	if (!s || !s->transport) {
		return OSP_ERR_INVALID;
	}

	/* Receive raw APDU */
	uint32_t rx_len = 0;
	osp_err_t r = osp_transport_recv_apdu(s->transport, s->framing, s->rx_buf, sizeof(s->rx_buf), &rx_len, timeout_ms);
	if (r != OSP_OK) {
		return r;
	}

	if (rx_len == 0) {
		return OSP_ERR_INVALID;
	}

	uint8_t tag = s->rx_buf[0];

	/* ACSE messages (BER-encoded) */
	if (tag == 0x60 || tag == 0x61) {
		osp_aarq_t aarq;
		osp_buf_t buf;
		osp_buf_init(&buf, s->rx_buf, rx_len);
		buf.wr = rx_len; /* data already received */

		if (osp_aarq_decode(&buf, &aarq) != 0) {
			return OSP_ERR_INVALID;
		}
		return handle_aarq(s, &aarq);
	}

	if (tag == 0x62) {
		/* Client release request — send RLRE */
		osp_rlrq_t rlre;
		rlre.reason = 0;
		osp_buf_t buf;
		osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
		osp_rlre_encode(&rlre, &buf);
		s->associated = false;
		return server_send(s, buf.buf, buf.wr);
	}

	if (!s->associated) {
		return OSP_ERR_INVALID;
	}

	/* xDLMS service messages (A-XDR) */
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, s->rx_buf, rx_len);
	rbuf.wr = rx_len; /* data already received */

	switch (tag) {
		case OSP_TAG_GET_REQUEST: {
			osp_get_request_t req;
			if (osp_get_request_decode(&rbuf, &req) != 0) {
				return OSP_ERR_INVALID;
			}
			return handle_get(s, &req);
		}

		case OSP_TAG_SET_REQUEST: {
			osp_set_request_t req;
			if (osp_set_request_decode(&rbuf, &req) != 0) {
				return OSP_ERR_INVALID;
			}
			return handle_set(s, &req);
		}

		case OSP_TAG_ACTION_REQUEST: {
			osp_action_request_t req;
			if (osp_action_request_decode(&rbuf, &req) != 0) {
				return OSP_ERR_INVALID;
			}

			/* Check if this is HLS pass 3 (ACTION on class 15, method 1) */
			if (req.as.normal.items[0].method.class_id == 15 && req.as.normal.items[0].method.method_id == 1) {
				return handle_hls_pass3(s, &req);
			}
			return handle_action(s, &req);
		}

		default:
			return OSP_ERR_UNSUPPORTED;
	}
}

/* ── Run server loop ─────────────────────────────────────────────────────── */

osp_err_t osp_server_run(osp_server_t *s, uint32_t timeout_ms) {
	osp_err_t r;
	while (true) {
		r = osp_server_accept(s, timeout_ms);
		if (r == OSP_ERR_TIMEOUT) {
			continue; /* no data, try again */
		}
		if (r == OSP_ERR_INVALID && !s->associated) {
			continue; /* not associated yet */
		}
		if (r != OSP_OK) {
			break; /* transport error or release */
		}
	}
	return r;
}
