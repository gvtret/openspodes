/**
 * client.c — DLMS/COSEM client session driver
 *
 * Blocking request/response: connect (AARQ→AARE→HLS), get, set, action, release.
 */

#include "client.h"
#include "../service/initiate.h"
#include "../codec/serialize.h"
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

	osp_initiate_request_t ireq;
	osp_initiate_request_default(&ireq);
	osp_buf_t ui;
	osp_buf_init(&ui, aarq.user_info, sizeof(aarq.user_info));
	if (osp_initiate_request_encode(&ireq, &ui) != OSP_OK) {
		return OSP_ERR_INVALID;
	}
	aarq.user_info_len = ui.wr;

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
		act_req.type = OSP_ACTION_NORMAL;
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

static osp_err_t client_recv_get_response(osp_client_t *c, osp_get_response_t *resp) {
	uint32_t rx_len;
	osp_err_t r = osp_transport_recv_apdu(c->transport, c->framing, c->rx_buf, sizeof(c->rx_buf), &rx_len, 5000);
	if (r != OSP_OK) {
		return r;
	}
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, c->rx_buf, rx_len);
	rbuf.wr = rx_len;
	if (osp_get_response_decode(&rbuf, resp) != 0) {
		return OSP_ERR_INVALID;
	}
	return OSP_OK;
}

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

	osp_err_t r = osp_transport_send_apdu(c->transport, c->framing, buf.buf, buf.wr);
	if (r != OSP_OK) {
		return r;
	}

	osp_get_response_t resp;
	r = client_recv_get_response(c, &resp);
	if (r != OSP_OK) {
		return r;
	}

	if (resp.type == OSP_GET_RESP_DATA) {
		*result = resp.data;
		return OSP_OK;
	}
	if (resp.type == OSP_GET_RESP_DATA_ERROR) {
		return OSP_ERR_NOT_FOUND;
	}

	uint8_t acc[OSP_CLIENT_REASSEMBLE_MAX];
	uint32_t acc_len = 0;
	while (resp.type == OSP_GET_RESP_BLOCK || resp.type == OSP_GET_RESP_BLOCK_LAST) {
		if (acc_len + resp.data_block.raw_data_len > sizeof(acc)) {
			return OSP_ERR_NOMEM;
		}
		memcpy(&acc[acc_len], resp.data_block.raw_data, resp.data_block.raw_data_len);
		acc_len += resp.data_block.raw_data_len;
		if (resp.data_block.last_block) {
			break;
		}

		osp_get_request_t next;
		memset(&next, 0, sizeof(next));
		next.type = OSP_GET_WITH_BLOCK;
		next.invoke_id_priority = req.invoke_id_priority;
		next.as.next.block_number = resp.data_block.block_number;

		osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
		if (osp_get_request_encode(&buf, &next) != 0) {
			return OSP_ERR_INVALID;
		}
		r = osp_transport_send_apdu(c->transport, c->framing, buf.buf, buf.wr);
		if (r != OSP_OK) {
			return r;
		}
		r = client_recv_get_response(c, &resp);
		if (r != OSP_OK) {
			return r;
		}
	}

	if (resp.type != OSP_GET_RESP_BLOCK_LAST) {
		return OSP_ERR_INVALID;
	}

	osp_buf_t vbuf;
	osp_buf_init(&vbuf, acc, acc_len);
	vbuf.wr = acc_len;
	return osp_value_read(&vbuf, result) == OSP_OK ? OSP_OK : OSP_ERR_INVALID;
}

/* ── SET ─────────────────────────────────────────────────────────────────── */

static osp_err_t client_recv_set_response(osp_client_t *c, osp_set_response_t *resp) {
	uint32_t rx_len;
	osp_err_t r = osp_transport_recv_apdu(c->transport, c->framing, c->rx_buf, sizeof(c->rx_buf), &rx_len, 5000);
	if (r != OSP_OK) {
		return r;
	}
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, c->rx_buf, rx_len);
	rbuf.wr = rx_len;
	if (osp_set_response_decode(&rbuf, resp) != 0) {
		return OSP_ERR_INVALID;
	}
	return OSP_OK;
}

static osp_err_t client_set_blocks(osp_client_t *c, uint8_t invoke_id_priority, uint16_t class_id, const osp_obis_t *obis,
                                  uint8_t attr_id, const uint8_t *value_bytes, uint32_t value_len) {
	uint32_t offset = 0;
	uint32_t block_number = 1;
	osp_buf_t buf;

	while (offset < value_len) {
		uint32_t chunk = value_len - offset;
		if (chunk > OSP_CLIENT_BLOCK_SIZE) {
			chunk = OSP_CLIENT_BLOCK_SIZE;
		}
		bool last = (offset + chunk >= value_len);

		osp_set_request_t req;
		memset(&req, 0, sizeof(req));
		req.invoke_id_priority = invoke_id_priority;
		if (offset == 0) {
			req.type = OSP_SET_WITH_FIRST_DATABLOCK;
			req.as.first_datablock.attr.class_id = class_id;
			req.as.first_datablock.attr.instance_id = *obis;
			req.as.first_datablock.attr.attribute_id = attr_id;
			req.as.first_datablock.datablock.last_block = last;
			req.as.first_datablock.datablock.block_number = block_number;
			req.as.first_datablock.datablock.raw_data_len = chunk;
			memcpy(req.as.first_datablock.datablock.raw_data, &value_bytes[offset], chunk);
		} else {
			req.type = OSP_SET_WITH_DATABLOCK;
			req.as.datablock.datablock.last_block = last;
			req.as.datablock.datablock.block_number = block_number;
			req.as.datablock.datablock.raw_data_len = chunk;
			memcpy(req.as.datablock.datablock.raw_data, &value_bytes[offset], chunk);
		}

		osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
		if (osp_set_request_encode(&buf, &req) != 0) {
			return OSP_ERR_INVALID;
		}
		osp_err_t r = osp_transport_send_apdu(c->transport, c->framing, buf.buf, buf.wr);
		if (r != OSP_OK) {
			return r;
		}

		osp_set_response_t resp;
		r = client_recv_set_response(c, &resp);
		if (r != OSP_OK) {
			return r;
		}
		if (last) {
			if (resp.type != OSP_SET_RESP_LAST_DATABLOCK || resp.as.last_datablock.result != OSP_DAR_SUCCESS) {
				return OSP_ERR_NOT_FOUND;
			}
		} else if (resp.type != OSP_SET_RESP_DATABLOCK) {
			return OSP_ERR_NOT_FOUND;
		}

		offset += chunk;
		block_number++;
	}
	return OSP_OK;
}

osp_err_t osp_client_set(osp_client_t *c, uint16_t class_id, const osp_obis_t *obis, uint8_t attr_id, const osp_value_t *value) {
	if (!c || !c->associated || !obis || !value) {
		return OSP_ERR_INVALID;
	}

	uint8_t value_bytes[OSP_CLIENT_REASSEMBLE_MAX];
	osp_buf_t w;
	osp_buf_init(&w, value_bytes, sizeof(value_bytes));
	if (osp_value_write(&w, value) != OSP_OK) {
		return OSP_ERR_INVALID;
	}

	uint8_t iidp = OSP_IIDP(++c->invoke_id, 0);
	if (w.wr > OSP_CLIENT_BLOCK_SIZE) {
		return client_set_blocks(c, iidp, class_id, obis, attr_id, value_bytes, w.wr);
	}

	osp_set_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = OSP_SET_NORMAL;
	req.invoke_id_priority = iidp;
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

	osp_err_t r = osp_transport_send_apdu(c->transport, c->framing, buf.buf, buf.wr);
	if (r != OSP_OK) {
		return r;
	}

	osp_set_response_t resp;
	r = client_recv_set_response(c, &resp);
	if (r != OSP_OK) {
		return r;
	}

	return (resp.type == OSP_SET_RESP_NORMAL && resp.as.normal.result == OSP_DAR_SUCCESS) ? OSP_OK : OSP_ERR_NOT_FOUND;
}

/* ── ACTION ──────────────────────────────────────────────────────────────── */

static osp_err_t client_recv_action_response(osp_client_t *c, osp_action_response_t *resp) {
	uint32_t rx_len;
	osp_err_t r = osp_transport_recv_apdu(c->transport, c->framing, c->rx_buf, sizeof(c->rx_buf), &rx_len, 5000);
	if (r != OSP_OK) {
		return r;
	}
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, c->rx_buf, rx_len);
	rbuf.wr = rx_len;
	if (osp_action_response_decode(&rbuf, resp) != 0) {
		return OSP_ERR_INVALID;
	}
	return OSP_OK;
}

static osp_err_t client_action_pblocks(osp_client_t *c, uint8_t invoke_id_priority, uint16_t class_id, const osp_obis_t *obis,
                                      uint8_t method_id, const uint8_t *param_bytes, uint32_t param_len,
                                      osp_action_response_t *out_resp) {
	uint32_t offset = 0;
	uint32_t block_number = 1;
	osp_buf_t buf;

	while (offset < param_len) {
		uint32_t chunk = param_len - offset;
		if (chunk > OSP_CLIENT_BLOCK_SIZE) {
			chunk = OSP_CLIENT_BLOCK_SIZE;
		}
		bool last = (offset + chunk >= param_len);

		osp_action_request_t req;
		memset(&req, 0, sizeof(req));
		req.invoke_id_priority = invoke_id_priority;
		if (offset == 0) {
			req.type = OSP_ACTION_WITH_FIRST_PBLOCK;
			req.as.first_pblock.method.class_id = class_id;
			req.as.first_pblock.method.instance_id = *obis;
			req.as.first_pblock.method.method_id = method_id;
			req.as.first_pblock.pblock.last_block = last;
			req.as.first_pblock.pblock.block_number = block_number;
			req.as.first_pblock.pblock.raw_data_len = chunk;
			memcpy(req.as.first_pblock.pblock.raw_data, &param_bytes[offset], chunk);
		} else {
			req.type = OSP_ACTION_WITH_PBLOCK;
			req.as.pblock.pblock.last_block = last;
			req.as.pblock.pblock.block_number = block_number;
			req.as.pblock.pblock.raw_data_len = chunk;
			memcpy(req.as.pblock.pblock.raw_data, &param_bytes[offset], chunk);
		}

		osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
		if (osp_action_request_encode(&buf, &req) != 0) {
			return OSP_ERR_INVALID;
		}
		osp_err_t r = osp_transport_send_apdu(c->transport, c->framing, buf.buf, buf.wr);
		if (r != OSP_OK) {
			return r;
		}

		r = client_recv_action_response(c, out_resp);
		if (r != OSP_OK) {
			return r;
		}
		if (last) {
			return OSP_OK;
		}
		if (out_resp->type != OSP_ACTION_RESP_NEXT_PBLOCK) {
			return OSP_ERR_NOT_FOUND;
		}

		offset += chunk;
		block_number++;
	}
	return OSP_ERR_INVALID;
}

static osp_err_t client_action_finish_response(osp_client_t *c, uint8_t invoke_id_priority, osp_action_response_t *resp,
                                              osp_value_t *result) {
	if (resp->type == OSP_ACTION_RESP_NORMAL) {
		if (resp->as.normal.items[0].return_data.tag != OSP_TAG_NULL) {
			*result = resp->as.normal.items[0].return_data;
		} else {
			*result = osp_val_null();
		}
		return (resp->as.normal.items[0].result == OSP_DAR_SUCCESS) ? OSP_OK : OSP_ERR_NOT_FOUND;
	}

	uint8_t acc[OSP_CLIENT_REASSEMBLE_MAX];
	uint32_t acc_len = 0;
	while (resp->type == OSP_ACTION_RESP_WITH_PBLOCK) {
		if (acc_len + resp->as.pblock.pblock.raw_data_len > sizeof(acc)) {
			return OSP_ERR_NOMEM;
		}
		memcpy(&acc[acc_len], resp->as.pblock.pblock.raw_data, resp->as.pblock.pblock.raw_data_len);
		acc_len += resp->as.pblock.pblock.raw_data_len;
		if (resp->as.pblock.pblock.last_block) {
			break;
		}

		osp_action_request_t next;
		memset(&next, 0, sizeof(next));
		next.type = OSP_ACTION_NEXT_PBLOCK;
		next.invoke_id_priority = invoke_id_priority;
		next.as.next_pblock.block_number = resp->as.pblock.pblock.block_number;

		osp_buf_t buf;
		osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
		if (osp_action_request_encode(&buf, &next) != 0) {
			return OSP_ERR_INVALID;
		}
		osp_err_t r = osp_transport_send_apdu(c->transport, c->framing, buf.buf, buf.wr);
		if (r != OSP_OK) {
			return r;
		}
		r = client_recv_action_response(c, resp);
		if (r != OSP_OK) {
			return r;
		}
	}

	osp_buf_t vbuf;
	osp_buf_init(&vbuf, acc, acc_len);
	vbuf.wr = acc_len;
	if (osp_value_read(&vbuf, result) != OSP_OK) {
		return OSP_ERR_INVALID;
	}
	return OSP_OK;
}

osp_err_t osp_client_action(osp_client_t *c, uint16_t class_id, const osp_obis_t *obis, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (!c || !c->associated || !obis || !result) {
		return OSP_ERR_INVALID;
	}

	uint8_t param_bytes[OSP_CLIENT_REASSEMBLE_MAX];
	uint32_t param_len = 0;
	if (param && param->tag != OSP_TAG_NULL) {
		osp_buf_t w;
		osp_buf_init(&w, param_bytes, sizeof(param_bytes));
		if (osp_value_write(&w, param) != OSP_OK) {
			return OSP_ERR_INVALID;
		}
		param_len = w.wr;
	}

	uint8_t iidp = OSP_IIDP(++c->invoke_id, 0);
	osp_action_response_t resp;
	osp_err_t r;

	if (param_len > OSP_CLIENT_BLOCK_SIZE) {
		r = client_action_pblocks(c, iidp, class_id, obis, method_id, param_bytes, param_len, &resp);
		if (r != OSP_OK) {
			return r;
		}
		return client_action_finish_response(c, iidp, &resp, result);
	}

	osp_action_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = OSP_ACTION_NORMAL;
	req.invoke_id_priority = iidp;
	req.as.normal.items[0].method.class_id = class_id;
	req.as.normal.items[0].method.instance_id = *obis;
	req.as.normal.items[0].method.method_id = method_id;
	req.as.normal.items[0].data = param ? *param : osp_val_null();

	osp_buf_t buf;
	osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
	if (osp_action_request_encode(&buf, &req) != 0) {
		return OSP_ERR_INVALID;
	}

	r = osp_transport_send_apdu(c->transport, c->framing, buf.buf, buf.wr);
	if (r != OSP_OK) {
		return r;
	}

	r = client_recv_action_response(c, &resp);
	if (r != OSP_OK) {
		return r;
	}
	return client_action_finish_response(c, iidp, &resp, result);
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
	if (osp_rlrq_encode(&rlrq, &buf) != 0) {
		return OSP_ERR_INVALID;
	}

	uint32_t rx_len;
	osp_err_t r = osp_transport_send_apdu(c->transport, c->framing, buf.buf, buf.wr);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_transport_recv_apdu(c->transport, c->framing, c->rx_buf, sizeof(c->rx_buf), &rx_len, 2000);
	if (r != OSP_OK) {
		return r;
	}

	osp_buf_t rbuf;
	osp_buf_init(&rbuf, c->rx_buf, rx_len);
	rbuf.wr = rx_len;
	osp_rlrq_t rlre;
	if (rx_len == 0 || osp_rlre_decode(&rbuf, &rlre) != 0) {
		return OSP_ERR_INVALID;
	}

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
