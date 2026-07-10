/**
 * client.c — DLMS/COSEM client session driver
 *
 * Blocking request/response: connect (AARQ→AARE→HLS), get, set, action, release.
 */

#include "client.h"
#include <string.h>

osp_err_t osp_client_init(osp_client_t *c, osp_transport_t *transport, osp_framing_type_t framing) {
	if (!c || !transport) {
		return OSP_ERR_INVALID;
	}

	memset(c, 0, sizeof(*c));
	c->transport = transport;
	c->framing = framing;
	c->associated = false;
	c->invoke_id = 0;
	return OSP_OK;
}

void osp_client_set_security(osp_client_t *c, const osp_sec_context_t *sec) {
	if (c && sec) {
		c->security = *sec;
	}
}

/* ── Internal: send APDU and receive response ────────────────────────────── */

static osp_err_t client_send_recv(osp_client_t *c, const uint8_t *tx, uint32_t tx_len, uint8_t *rx, uint32_t rx_size, uint32_t *rx_len) {
	osp_err_t r;

	/* Send framed APDU */
	r = osp_transport_send_apdu(c->transport, c->framing, tx, tx_len);
	if (r != OSP_OK) {
		return r;
	}

	/* Receive framed APDU */
	r = osp_transport_recv_apdu(c->transport, c->framing, rx, rx_size, rx_len, 5000);
	return r;
}

/* ── Connect: AARQ → AARE → HLS pass3/4 ────────────────────────────────── */

osp_err_t osp_client_connect(osp_client_t *c, uint32_t timeout_ms) {
	if (!c || !c->transport) {
		return OSP_ERR_INVALID;
	}

	(void)timeout_ms;

	/* Open transport */
	osp_err_t r = c->transport->open(c->transport->ctx);
	if (r != OSP_OK) {
		return r;
	}

	/* Build AARQ */
	osp_aarq_t aarq;
	memset(&aarq, 0, sizeof(aarq));
	aarq.application_context = OSP_CTX_LN;
	aarq.mechanism = (uint8_t)c->security.mechanism;
	memcpy(aarq.calling_ap_title, c->security.system_title, OSP_SEC_SYSTEM_TITLE_SIZE);
	aarq.calling_ap_title_len = OSP_SEC_SYSTEM_TITLE_SIZE;

	/* Generate CtoS challenge */
	aarq.calling_auth_value_len = 8;
	for (uint8_t i = 0; i < 8; i++) {
		aarq.calling_auth_value[i] = (uint8_t)(i + 0x30);
	}
	memcpy(c->security.ctos, aarq.calling_auth_value, 8);
	c->security.ctos_len = 8;

	/* Build xDLMS InitiateRequest (minimal: conformance + PDU size) */
	aarq.user_info[0] = 0x00; /* InitiateRequest tag */
	aarq.user_info[1] = 0x01; /* type: normal */
	aarq.user_info_len = 2;

	/* Encode AARQ */
	osp_buf_t buf;
	osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
	if (osp_aarq_encode(&aarq, &buf) != 0) {
		return OSP_ERR_INVALID;
	}

	/* Send and receive */
	uint32_t rx_len;
	r = client_send_recv(c, buf.buf, buf.wr, c->rx_buf, sizeof(c->rx_buf), &rx_len);
	if (r != OSP_OK) {
		return r;
	}

	/* Decode AARE */
	osp_aare_t aare;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, c->rx_buf, rx_len);
	rbuf.wr = rx_len; /* data already received */
	if (osp_aare_decode(&rbuf, &aare) != 0) {
		return OSP_ERR_INVALID;
	}

	if (aare.result != OSP_RESULT_ACCEPTED) {
		return OSP_ERR_SECURITY;
	}

	/* Store StoC challenge */
	if (aare.responding_auth_value_len > 0) {
		memcpy(c->security.stoc, aare.responding_auth_value, aare.responding_auth_value_len);
		c->security.stoc_len = aare.responding_auth_value_len;
	}

	/* HLS pass 3/4 for GMAC */
	if (c->security.mechanism == OSP_MECH_HLS_GMAC) {
		uint8_t f_stoc[17];
		if (osp_hls_pass3_build(&c->security, f_stoc, sizeof(f_stoc)) != 17) {
			return OSP_ERR_SECURITY;
		}

		/* Send pass 3 via ACTION on Association LN (class 15, method 1) */
		osp_action_request_t act_req;
		memset(&act_req, 0, sizeof(act_req));
		act_req.type = OSP_GET_NORMAL;
		act_req.invoke_id_priority = OSP_IIDP(++c->invoke_id, 0);
		act_req.as.normal.items[0].method.class_id = 15;
		osp_obis_t asso_ln = {0, 0, 40, 0, 0, 255};
		act_req.as.normal.items[0].method.instance_id = asso_ln;
		act_req.as.normal.items[0].method.method_id = 1;
		act_req.as.normal.items[0].data.tag = OSP_TAG_OCTETSTRING;
		memcpy(act_req.as.normal.items[0].data.as.octetstring.data, f_stoc, 17);
		act_req.as.normal.items[0].data.as.octetstring.len = 17;

		osp_buf_t act_buf;
		osp_buf_init(&act_buf, c->tx_buf, sizeof(c->tx_buf));
		if (osp_action_request_encode(&act_buf, &act_req) != 0) {
			return OSP_ERR_INVALID;
		}

		uint32_t act_rx_len;
		r = client_send_recv(c, act_buf.buf, act_buf.wr, c->rx_buf, sizeof(c->rx_buf), &act_rx_len);
		if (r != OSP_OK) {
			return r;
		}

		/* Decode ACTION response and verify pass 4 */
		osp_action_response_t act_resp;
		osp_buf_t arbuf;
		osp_buf_init(&arbuf, c->rx_buf, act_rx_len);
		arbuf.wr = act_rx_len; /* data already received */
		if (osp_action_response_decode(&arbuf, &act_resp) != 0) {
			return OSP_ERR_INVALID;
		}

		if (act_resp.as.normal.items[0].return_data.tag == OSP_TAG_OCTETSTRING && act_resp.as.normal.items[0].return_data.as.octetstring.len >= 17) {
			if (osp_hls_pass4_verify(&c->security, act_resp.as.normal.items[0].return_data.as.octetstring.data, 17) != 0) {
				return OSP_ERR_SECURITY;
			}
		} else {
			return OSP_ERR_INVALID;
		}
	}

	c->associated = true;
	return OSP_OK;
}

/* ── GET ─────────────────────────────────────────────────────────────────── */

osp_err_t osp_client_get(osp_client_t *c, uint16_t class_id, const osp_obis_t *obis, uint8_t attr_id, osp_value_t *result) {
	if (!c || !c->associated || !obis || !result) {
		return OSP_ERR_INVALID;
	}

	osp_get_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = OSP_GET_NORMAL;
	req.invoke_id_priority = OSP_IIDP(++c->invoke_id, 0);
	req.as.normal.attr.class_id = class_id;
	req.as.normal.attr.instance_id = *obis;
	req.as.normal.attr.attribute_id = attr_id;

	osp_buf_t buf;
	osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
	if (osp_get_request_encode(&buf, &req) != 0) {
		return OSP_ERR_INVALID;
	}

	uint32_t rx_len;
	osp_err_t r = client_send_recv(c, buf.buf, buf.wr, c->rx_buf, sizeof(c->rx_buf), &rx_len);
	if (r != OSP_OK) {
		return r;
	}

	osp_get_response_t resp;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, c->rx_buf, rx_len);
	rbuf.wr = rx_len; /* data already received */
	if (osp_get_response_decode(&rbuf, &resp) != 0) {
		return OSP_ERR_INVALID;
	}

	if (resp.type == OSP_GET_RESP_DATA) {
		*result = resp.data;
		return OSP_OK;
	}
	return OSP_ERR_NOT_FOUND;
}

/* ── SET ─────────────────────────────────────────────────────────────────── */

osp_err_t osp_client_set(osp_client_t *c, uint16_t class_id, const osp_obis_t *obis, uint8_t attr_id, const osp_value_t *value) {
	if (!c || !c->associated || !obis || !value) {
		return OSP_ERR_INVALID;
	}

	osp_set_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = OSP_GET_NORMAL;
	req.invoke_id_priority = OSP_IIDP(++c->invoke_id, 0);
	req.as.normal.items[0].attr.class_id = class_id;
	req.as.normal.items[0].attr.instance_id = *obis;
	req.as.normal.items[0].attr.attribute_id = attr_id;
	req.as.normal.items[0].data = *value;
	req.as.normal.item_count = 1;

	osp_buf_t buf;
	osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
	if (osp_set_request_encode(&buf, &req) != 0) {
		return OSP_ERR_INVALID;
	}

	uint32_t rx_len;
	osp_err_t r = client_send_recv(c, buf.buf, buf.wr, c->rx_buf, sizeof(c->rx_buf), &rx_len);
	if (r != OSP_OK) {
		return r;
	}

	osp_set_response_t resp;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, c->rx_buf, rx_len);
	rbuf.wr = rx_len; /* data already received */
	if (osp_set_response_decode(&rbuf, &resp) != 0) {
		return OSP_ERR_INVALID;
	}

	return (resp.as.normal.result == OSP_DAR_SUCCESS) ? OSP_OK : OSP_ERR_NOT_FOUND;
}

/* ── ACTION ──────────────────────────────────────────────────────────────── */

osp_err_t osp_client_action(osp_client_t *c, uint16_t class_id, const osp_obis_t *obis, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (!c || !c->associated || !obis || !result) {
		return OSP_ERR_INVALID;
	}

	osp_action_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = OSP_GET_NORMAL;
	req.invoke_id_priority = OSP_IIDP(++c->invoke_id, 0);
	req.as.normal.items[0].method.class_id = class_id;
	req.as.normal.items[0].method.instance_id = *obis;
	req.as.normal.items[0].method.method_id = method_id;
	req.as.normal.items[0].data = param ? *param : osp_val_null();

	osp_buf_t buf;
	osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
	if (osp_action_request_encode(&buf, &req) != 0) {
		return OSP_ERR_INVALID;
	}

	uint32_t rx_len;
	osp_err_t r = client_send_recv(c, buf.buf, buf.wr, c->rx_buf, sizeof(c->rx_buf), &rx_len);
	if (r != OSP_OK) {
		return r;
	}

	osp_action_response_t resp;
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, c->rx_buf, rx_len);
	rbuf.wr = rx_len; /* data already received */
	if (osp_action_response_decode(&rbuf, &resp) != 0) {
		return OSP_ERR_INVALID;
	}

	if (resp.as.normal.items[0].return_data.tag != OSP_TAG_NULL) {
		*result = resp.as.normal.items[0].return_data;
	} else {
		*result = osp_val_null();
	}
	return (resp.as.normal.items[0].result == OSP_DAR_SUCCESS) ? OSP_OK : OSP_ERR_NOT_FOUND;
}

/* ── Release ─────────────────────────────────────────────────────────────── */

osp_err_t osp_client_release(osp_client_t *c) {
	if (!c || !c->transport) {
		return OSP_ERR_INVALID;
	}

	osp_rlrq_t rlrq;
	rlrq.reason = 0; /* normal */
	osp_buf_t buf;
	osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
	osp_rlrq_encode(&rlrq, &buf);

	uint32_t rx_len;
	osp_transport_send_apdu(c->transport, c->framing, buf.buf, buf.wr);
	osp_transport_recv_apdu(c->transport, c->framing, c->rx_buf, sizeof(c->rx_buf), &rx_len, 2000);

	c->associated = false;
	return OSP_OK;
}

/* ── Disconnect ──────────────────────────────────────────────────────────── */

osp_err_t osp_client_disconnect(osp_client_t *c) {
	if (!c || !c->transport) {
		return OSP_ERR_INVALID;
	}

	if (c->associated) {
		osp_client_release(c);
	}

	c->transport->close(c->transport->ctx);
	return OSP_OK;
}
