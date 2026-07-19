/**
 * client.c — DLMS/COSEM client session driver
 *
 * Blocking request/response: connect (AARQ→AARE→HLS), get, set, action, release.
 */

#include "client.h"
#include "../service/initiate.h"
#include "../service/notification.h"
#include "../service/gbt.h"
#include "../codec/serialize.h"
#include "../transport/hdlc_session.h"
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
	c->hdlc_active = false;

	if (framing == OSP_FRAMING_HDLC) {
		osp_hdlc_session_init_client(&c->hdlc, transport, 1, 1, 1, 1);
	}

	return OSP_OK;
}

void osp_client_set_security(osp_client_t *c, const osp_sec_context_t *sec) {
	if (c && sec) {
		c->security = *sec;
	}
}

void osp_client_set_hdlc_addresses(osp_client_t *c, uint32_t client_addr, uint8_t client_addr_len,
                                    uint32_t server_addr, uint8_t server_addr_len) {
	if (!c || c->framing != OSP_FRAMING_HDLC)
		return;
	osp_hdlc_session_init_client(&c->hdlc, c->transport, client_addr, client_addr_len, server_addr, server_addr_len);
}

void osp_client_enable_gbt(osp_client_t *c, uint32_t block_size) {
	if (!c) {
		return;
	}
	c->gbt_enabled = true;
	c->gbt_block_size = block_size > OSP_GBT_HEADER_MAX ? block_size : OSP_GBT_DEFAULT_BLOCK_SIZE;
	c->gbt_window = 0;
}

void osp_client_set_gbt_window(osp_client_t *c, uint8_t window) {
	if (!c) {
		return;
	}
	c->gbt_window = window & OSP_GBT_WINDOW_MASK;
}

void osp_client_set_gbt_streaming(osp_client_t *c, bool enabled) {
	if (c) {
		c->gbt_streaming = enabled;
	}
}

void osp_client_set_ciphering(osp_client_t *c, const osp_sec_context_t *tx, const osp_sec_context_t *rx) {
	if (!c || !tx || !rx) {
		return;
	}
	c->cipher_tx = *tx;
	c->cipher_rx = *rx;
	c->ciphering_enabled = true;
}

void osp_client_set_dedicated_key(osp_client_t *c, const uint8_t *key, uint8_t key_len) {
	if (!c || !key || key_len == 0 || key_len > OSP_INITIATE_DEDICATED_KEY_MAX) {
		return;
	}
	memcpy(c->dedicated_key, key, key_len);
	c->dedicated_key_len = key_len;
	c->use_dedicated_key = true;
}

static bool client_apdu_use_ciphering(const osp_client_t *c, const uint8_t *data, uint32_t len) {
	return c->ciphering_enabled && len > 0 && osp_gbt_applies_to_apdu(data, len);
}

static uint32_t client_gbt_payload_max(const osp_client_t *c) {
	uint32_t block = c->gbt_block_size > 0 ? c->gbt_block_size : OSP_GBT_DEFAULT_BLOCK_SIZE;
	return block > OSP_GBT_HEADER_MAX ? block - OSP_GBT_HEADER_MAX : 1;
}

static osp_err_t client_send_apdu(osp_client_t *c, const uint8_t *data, uint32_t len) {
	const uint8_t *send_data = data;
	uint32_t send_len = len;
	uint8_t cipher_buf[OSP_CLIENT_MAX_PDU];

	if (client_apdu_use_ciphering(c, data, len)) {
		uint32_t cipher_len = 0;
		if (osp_glo_protect(&c->cipher_tx, osp_svc_cipher_tag_for_plain(&c->cipher_tx, data[0]), data, len, cipher_buf, &cipher_len) != 0) {
			return OSP_ERR_SECURITY;
		}
		/* IC overflow check — re-keying required */
		if (c->cipher_tx.invocation_counter == 0xFFFFFFFF) {
			return OSP_ERR_SECURITY;
		}
		c->cipher_tx.invocation_counter++;
		send_data = cipher_buf;
		send_len = cipher_len;
	}

	if (c->gbt_enabled && osp_gbt_applies_to_apdu(send_data, send_len) && send_len > client_gbt_payload_max(c)) {
		if (c->gbt_streaming) {
			return osp_gbt_transport_send_streaming(c->transport, c->framing, send_data, send_len, client_gbt_payload_max(c), c->gbt_window,
			                                        c->tx_buf, sizeof(c->tx_buf), c->rx_buf, sizeof(c->rx_buf), 5000);
		}
		return osp_gbt_transport_send(c->transport, c->framing, send_data, send_len, client_gbt_payload_max(c), c->gbt_window, c->tx_buf,
		                              sizeof(c->tx_buf), c->rx_buf, sizeof(c->rx_buf), 5000);
	}
	if (c->hdlc_active) {
		return osp_hdlc_session_send_apdu(&c->hdlc, send_data, send_len);
	}
	return osp_transport_send_apdu(c->transport, c->framing, send_data, send_len);
}

static osp_err_t client_recv_apdu(osp_client_t *c, uint8_t *apdu, uint32_t apdu_size, uint32_t *apdu_len, uint32_t timeout_ms) {
	uint32_t rx_len = 0;
	osp_err_t r;

	if (c->hdlc_active) {
		r = osp_hdlc_session_recv_apdu(&c->hdlc, c->rx_buf, sizeof(c->rx_buf), &rx_len, timeout_ms);
	} else {
		r = osp_transport_recv_apdu(c->transport, c->framing, c->rx_buf, sizeof(c->rx_buf), &rx_len, timeout_ms);
	}
	if (r != OSP_OK) {
		return r;
	}
	if (c->gbt_enabled && rx_len > 0 && c->rx_buf[0] == OSP_TAG_GENERAL_BLOCK_TRANSFER) {
		r = osp_gbt_transport_recv(c->transport, c->framing, c->rx_buf, sizeof(c->rx_buf), apdu, apdu_size, apdu_len, c->tx_buf,
		                           sizeof(c->tx_buf), timeout_ms, c->rx_buf, rx_len);
		if (r != OSP_OK) {
			return r;
		}
	} else {
		if (rx_len > apdu_size) {
			return OSP_ERR_NOMEM;
		}
		memcpy(apdu, c->rx_buf, rx_len);
		*apdu_len = rx_len;
	}

	if (c->ciphering_enabled && *apdu_len > 0 && osp_svc_is_ciphered_tag(apdu[0])) {
		uint8_t plain[OSP_CLIENT_REASSEMBLE_MAX];
		uint32_t plain_len = 0;
		if (osp_glo_unprotect(&c->cipher_rx, apdu, *apdu_len, plain, &plain_len) != 0) {
			return OSP_ERR_SECURITY;
		}
		if (plain_len > apdu_size) {
			return OSP_ERR_NOMEM;
		}
		memcpy(apdu, plain, plain_len);
		*apdu_len = plain_len;
	}
	return OSP_OK;
}

/* ── Internal: send APDU and receive response ────────────────────────────── */

static osp_err_t client_send_recv(osp_client_t *c, const uint8_t *tx, uint32_t tx_len, uint8_t *rx, uint32_t rx_size, uint32_t *rx_len) {
	osp_err_t r = client_send_apdu(c, tx, tx_len);
	if (r != OSP_OK) {
		return r;
	}
	return client_recv_apdu(c, rx, rx_size, rx_len, 5000);
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

	/* HDLC: SNRM/UA link setup before AARQ */
	if (c->framing == OSP_FRAMING_HDLC) {
		r = osp_hdlc_session_connect(&c->hdlc, timeout_ms);
		if (r != OSP_OK) {
			return r;
		}
		c->hdlc_active = true;
	}

	/* Build AARQ */
	osp_aarq_t aarq;
	memset(&aarq, 0, sizeof(aarq));
	aarq.application_context = c->ciphering_enabled ? OSP_CTX_LN_CIPHERING : OSP_CTX_LN;
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
	if (c->use_dedicated_key) {
		ireq.has_dedicated_key = true;
		ireq.dedicated_key_len = c->dedicated_key_len;
		memcpy(ireq.dedicated_key, c->dedicated_key, c->dedicated_key_len);
	}
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

	/* Store StoC challenge and peer system title */
	if (aare.responding_auth_value_len > 0) {
		memcpy(c->security.stoc, aare.responding_auth_value, aare.responding_auth_value_len);
		c->security.stoc_len = aare.responding_auth_value_len;
	}
	if (aare.responding_ap_title_len >= OSP_SEC_SYSTEM_TITLE_SIZE) {
		memcpy(c->security.peer_system_title, aare.responding_ap_title, OSP_SEC_SYSTEM_TITLE_SIZE);
	}

	/* HLS pass 3/4 */
	if (osp_hls_requires_handshake(c->security.mechanism)) {
		uint8_t f_stoc[OSP_SEC_HLS_AUTH_MAX];
		uint32_t f_len = 0;
		if (osp_hls_pass3_build(&c->security, f_stoc, sizeof(f_stoc), &f_len) != 0) {
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
		memcpy(act_req.as.normal.items[0].data.as.octetstring.data, f_stoc, f_len);
		act_req.as.normal.items[0].data.as.octetstring.len = f_len;

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

		if (act_resp.as.normal.items[0].return_data.tag == OSP_TAG_OCTETSTRING &&
		    act_resp.as.normal.items[0].return_data.as.octetstring.len > 0) {
			const uint8_t *f_ctos = act_resp.as.normal.items[0].return_data.as.octetstring.data;
			uint32_t f_ctos_len = act_resp.as.normal.items[0].return_data.as.octetstring.len;
			if (osp_hls_pass4_verify(&c->security, f_ctos, f_ctos_len) != 0) {
				return OSP_ERR_SECURITY;
			}
		} else {
			return OSP_ERR_INVALID;
		}
	}

	if (c->ciphering_enabled && ireq.has_dedicated_key) {
		osp_sec_cipher_session_use_dedicated(&c->cipher_tx, &c->cipher_rx, ireq.dedicated_key, ireq.dedicated_key_len);
	}

	c->associated = true;
	return OSP_OK;
}

/* ── GET ─────────────────────────────────────────────────────────────────── */

static osp_err_t client_recv_get_response(osp_client_t *c, osp_get_response_t *resp) {
	uint8_t apdu[OSP_CLIENT_REASSEMBLE_MAX];
	uint32_t apdu_len = 0;
	osp_err_t r = client_recv_apdu(c, apdu, sizeof(apdu), &apdu_len, 5000);
	if (r != OSP_OK) {
		return r;
	}
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, apdu, apdu_len);
	rbuf.wr = apdu_len;
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

	osp_err_t r = client_send_apdu(c, buf.buf, buf.wr);
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
	if (c->gbt_enabled) {
		return OSP_ERR_INVALID;
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
		r = client_send_apdu(c, buf.buf, buf.wr);
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

osp_err_t osp_client_get_with_selective_access(osp_client_t *c, uint16_t class_id, const osp_obis_t *obis, uint8_t attr_id,
                                                 const osp_selective_access_t *sa, osp_value_t *result) {
	if (!c || !c->associated || !obis || !sa || !result) {
		return OSP_ERR_INVALID;
	}

	osp_get_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = OSP_GET_NORMAL;
	req.invoke_id_priority = OSP_IIDP(++c->invoke_id, 0);
	req.as.normal.attr.class_id = class_id;
	req.as.normal.attr.instance_id = *obis;
	req.as.normal.attr.attribute_id = attr_id;
	req.as.normal.access_selection = *sa;

	osp_buf_t buf;
	osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
	if (osp_get_request_encode(&buf, &req) != 0) {
		return OSP_ERR_INVALID;
	}

	osp_err_t r = client_send_apdu(c, buf.buf, buf.wr);
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
	return OSP_ERR_INVALID;
}

osp_err_t osp_client_get_with_list(osp_client_t *c, const osp_client_attr_ref_t *attrs, uint8_t count, osp_get_result_item_t *results) {
	if (!c || !c->associated || !attrs || !results || count == 0 || count > OSP_XDLMS_MAX_LIST) {
		return OSP_ERR_INVALID;
	}

	osp_get_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = OSP_GET_WITH_LIST;
	req.invoke_id_priority = OSP_IIDP(++c->invoke_id, 0);
	req.as.with_list.count = count;
	for (uint8_t i = 0; i < count; i++) {
		req.as.with_list.items[i].attr.class_id = attrs[i].class_id;
		req.as.with_list.items[i].attr.instance_id = attrs[i].instance_id;
		req.as.with_list.items[i].attr.attribute_id = attrs[i].attribute_id;
	}

	osp_buf_t buf;
	osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
	if (osp_get_request_encode(&buf, &req) != 0) {
		return OSP_ERR_INVALID;
	}

	osp_err_t r = client_send_apdu(c, buf.buf, buf.wr);
	if (r != OSP_OK) {
		return r;
	}

	osp_get_response_t resp;
	r = client_recv_get_response(c, &resp);
	if (r != OSP_OK) {
		return r;
	}
	if (resp.type != OSP_GET_RESP_WITH_LIST || resp.with_list.count != count) {
		return OSP_ERR_INVALID;
	}

	for (uint8_t i = 0; i < count; i++) {
		results[i] = resp.with_list.items[i];
	}
	return OSP_OK;
}

/* ── SET ─────────────────────────────────────────────────────────────────── */

static osp_err_t client_recv_set_response(osp_client_t *c, osp_set_response_t *resp) {
	uint8_t apdu[OSP_CLIENT_REASSEMBLE_MAX];
	uint32_t apdu_len = 0;
	osp_err_t r = client_recv_apdu(c, apdu, sizeof(apdu), &apdu_len, 5000);
	if (r != OSP_OK) {
		return r;
	}
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, apdu, apdu_len);
	rbuf.wr = apdu_len;
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
		osp_err_t r = client_send_apdu(c, buf.buf, buf.wr);
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

	osp_err_t r = client_send_apdu(c, buf.buf, buf.wr);
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

osp_err_t osp_client_set_with_list(osp_client_t *c, const osp_client_attr_ref_t *attrs, const osp_value_t *values, uint8_t count,
                                   osp_dar_t *results) {
	if (!c || !c->associated || !attrs || !values || !results || count == 0 || count > OSP_XDLMS_MAX_LIST) {
		return OSP_ERR_INVALID;
	}

	osp_set_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = OSP_SET_WITH_LIST;
	req.invoke_id_priority = OSP_IIDP(++c->invoke_id, 0);
	req.as.with_list.count = count;
	for (uint8_t i = 0; i < count; i++) {
		req.as.with_list.items[i].attr.class_id = attrs[i].class_id;
		req.as.with_list.items[i].attr.instance_id = attrs[i].instance_id;
		req.as.with_list.items[i].attr.attribute_id = attrs[i].attribute_id;
		req.as.with_list.items[i].data = values[i];
	}

	osp_buf_t buf;
	osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
	if (osp_set_request_encode(&buf, &req) != 0) {
		return OSP_ERR_INVALID;
	}

	osp_err_t r = client_send_apdu(c, buf.buf, buf.wr);
	if (r != OSP_OK) {
		return r;
	}

	osp_set_response_t resp;
	r = client_recv_set_response(c, &resp);
	if (r != OSP_OK) {
		return r;
	}
	if (resp.type != OSP_SET_RESP_WITH_LIST || resp.as.with_list.count != count) {
		return OSP_ERR_INVALID;
	}

	for (uint8_t i = 0; i < count; i++) {
		results[i] = resp.as.with_list.results[i];
	}
	return OSP_OK;
}

/* ── ACTION ──────────────────────────────────────────────────────────────── */

static osp_err_t client_recv_action_response(osp_client_t *c, osp_action_response_t *resp) {
	uint8_t apdu[OSP_CLIENT_REASSEMBLE_MAX];
	uint32_t apdu_len = 0;
	osp_err_t r = client_recv_apdu(c, apdu, sizeof(apdu), &apdu_len, 5000);
	if (r != OSP_OK) {
		return r;
	}
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, apdu, apdu_len);
	rbuf.wr = apdu_len;
	if (osp_action_response_decode(&rbuf, resp) != 0) {
		return OSP_ERR_INVALID;
	}
	return OSP_OK;
}

static osp_err_t client_action_param_blocks(osp_client_t *c, uint8_t invoke_id_priority, uint16_t class_id, const osp_obis_t *obis,
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
			req.type = OSP_ACTION_WITH_FIRST_PARAM_BLOCK;
			req.as.first_param_block.method.class_id = class_id;
			req.as.first_param_block.method.instance_id = *obis;
			req.as.first_param_block.method.method_id = method_id;
			req.as.first_param_block.param_block.last_block = last;
			req.as.first_param_block.param_block.block_number = block_number;
			req.as.first_param_block.param_block.raw_data_len = chunk;
			memcpy(req.as.first_param_block.param_block.raw_data, &param_bytes[offset], chunk);
		} else {
			req.type = OSP_ACTION_WITH_PARAM_BLOCK;
			req.as.with_param_block.param_block.last_block = last;
			req.as.with_param_block.param_block.block_number = block_number;
			req.as.with_param_block.param_block.raw_data_len = chunk;
			memcpy(req.as.with_param_block.param_block.raw_data, &param_bytes[offset], chunk);
		}

		osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
		if (osp_action_request_encode(&buf, &req) != 0) {
			return OSP_ERR_INVALID;
		}
		osp_err_t r = client_send_apdu(c, buf.buf, buf.wr);
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
		if (out_resp->type != OSP_ACTION_RESP_NEXT_PARAM_BLOCK) {
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
	while (resp->type == OSP_ACTION_RESP_WITH_PARAM_BLOCK) {
		if (acc_len + resp->as.with_param_block.param_block.raw_data_len > sizeof(acc)) {
			return OSP_ERR_NOMEM;
		}
		memcpy(&acc[acc_len], resp->as.with_param_block.param_block.raw_data, resp->as.with_param_block.param_block.raw_data_len);
		acc_len += resp->as.with_param_block.param_block.raw_data_len;
		if (resp->as.with_param_block.param_block.last_block) {
			break;
		}

		osp_action_request_t next;
		memset(&next, 0, sizeof(next));
		next.type = OSP_ACTION_NEXT_PARAM_BLOCK;
		next.invoke_id_priority = invoke_id_priority;
		next.as.next_param_block.block_number = resp->as.with_param_block.param_block.block_number;

		osp_buf_t buf;
		osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
		if (osp_action_request_encode(&buf, &next) != 0) {
			return OSP_ERR_INVALID;
		}
		osp_err_t r = client_send_apdu(c, buf.buf, buf.wr);
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
		r = client_action_param_blocks(c, iidp, class_id, obis, method_id, param_bytes, param_len, &resp);
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

	r = client_send_apdu(c, buf.buf, buf.wr);
	if (r != OSP_OK) {
		return r;
	}

	r = client_recv_action_response(c, &resp);
	if (r != OSP_OK) {
		return r;
	}
	return client_action_finish_response(c, iidp, &resp, result);
}

osp_err_t osp_client_action_with_list(osp_client_t *c, const osp_client_method_ref_t *methods, const osp_value_t *params, uint8_t count,
                                      osp_action_response_item_t *results) {
	if (!c || !c->associated || !methods || !results || count == 0 || count > OSP_XDLMS_MAX_LIST) {
		return OSP_ERR_INVALID;
	}

	osp_action_request_t req;
	memset(&req, 0, sizeof(req));
	req.type = OSP_ACTION_WITH_LIST;
	req.invoke_id_priority = OSP_IIDP(++c->invoke_id, 0);
	req.as.with_list.count = count;
	for (uint8_t i = 0; i < count; i++) {
		req.as.with_list.items[i].method.class_id = methods[i].class_id;
		req.as.with_list.items[i].method.instance_id = methods[i].instance_id;
		req.as.with_list.items[i].method.method_id = methods[i].method_id;
		req.as.with_list.items[i].data = params ? params[i] : osp_val_null();
	}

	osp_buf_t buf;
	osp_buf_init(&buf, c->tx_buf, sizeof(c->tx_buf));
	if (osp_action_request_encode(&buf, &req) != 0) {
		return OSP_ERR_INVALID;
	}

	osp_err_t r = client_send_apdu(c, buf.buf, buf.wr);
	if (r != OSP_OK) {
		return r;
	}

	osp_action_response_t resp;
	r = client_recv_action_response(c, &resp);
	if (r != OSP_OK) {
		return r;
	}
	if (resp.type != OSP_ACTION_RESP_WITH_LIST || resp.as.with_list.count != count) {
		return OSP_ERR_INVALID;
	}

	for (uint8_t i = 0; i < count; i++) {
		results[i] = resp.as.with_list.items[i];
	}
	return OSP_OK;
}

osp_err_t osp_client_recv_data_notification(osp_client_t *c, osp_data_notification_t *dn, uint32_t timeout_ms) {
	if (!c || !c->transport || !dn) {
		return OSP_ERR_INVALID;
	}

	uint32_t rx_len = 0;
	osp_err_t r = osp_transport_recv_apdu(c->transport, c->framing, c->rx_buf, sizeof(c->rx_buf), &rx_len, timeout_ms);
	if (r != OSP_OK) {
		return r;
	}
	if (rx_len == 0 || c->rx_buf[0] != OSP_TAG_DATA_NOTIFICATION) {
		return OSP_ERR_INVALID;
	}

	osp_buf_t buf;
	osp_buf_init(&buf, c->rx_buf, rx_len);
	buf.wr = rx_len;
	if (osp_data_notification_decode(&buf, dn) != 0) {
		return OSP_ERR_INVALID;
	}
	return OSP_OK;
}

osp_err_t osp_client_recv_event_notification(osp_client_t *c, osp_event_notification_t *ev, uint32_t timeout_ms) {
	if (!c || !c->transport || !ev) {
		return OSP_ERR_INVALID;
	}

	uint8_t apdu[OSP_CLIENT_REASSEMBLE_MAX];
	uint32_t apdu_len = 0;
	osp_err_t r = client_recv_apdu(c, apdu, sizeof(apdu), &apdu_len, timeout_ms);
	if (r != OSP_OK) {
		return r;
	}
	if (apdu_len == 0 || apdu[0] != OSP_TAG_EVENT_NOTIFICATION_REQ) {
		return OSP_ERR_INVALID;
	}

	osp_buf_t buf;
	osp_buf_init(&buf, apdu, apdu_len);
	buf.wr = apdu_len;
	if (osp_event_notification_decode(&buf, ev) != 0) {
		return OSP_ERR_INVALID;
	}
	return OSP_OK;
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
	osp_err_t r = client_send_apdu(c, buf.buf, buf.wr);
	if (r != OSP_OK) {
		return r;
	}
	if (c->hdlc_active) {
		r = osp_hdlc_session_recv_apdu(&c->hdlc, c->rx_buf, sizeof(c->rx_buf), &rx_len, 2000);
	} else {
		r = osp_transport_recv_apdu(c->transport, c->framing, c->rx_buf, sizeof(c->rx_buf), &rx_len, 2000);
	}
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

	/* HDLC: DISC/UA link teardown */
	if (c->hdlc_active) {
		osp_hdlc_session_disconnect(&c->hdlc, 2000);
		c->hdlc_active = false;
	} else {
		c->transport->close(c->transport->ctx);
	}
	return OSP_OK;
}
