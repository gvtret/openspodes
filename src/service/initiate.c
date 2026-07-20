/**
 * initiate.c — xDLMS InitiateRequest / InitiateResponse (IEC 62056-5-3 §11.2)
 */

#include "initiate.h"
#include "../codec/codec.h"
#include <string.h>

static const uint8_t conformance_tag[2] = {0x5F, 0x1F};

static osp_err_t push_conformance(uint32_t conformance, osp_buf_t *buf) {
	osp_err_t r;

	for (uint8_t i = 0; i < 2; i++) {
		r = osp_axdr_write_u8(buf, conformance_tag[i]);
		if (r != OSP_OK) {
			return r;
		}
	}
	r = osp_axdr_write_u8(buf, 0x04);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_axdr_write_u8(buf, 0x00);
	if (r != OSP_OK) {
		return r;
	}
	uint8_t b[4];
	b[0] = (uint8_t)((conformance >> 16) & 0xFF);
	b[1] = (uint8_t)((conformance >> 8) & 0xFF);
	b[2] = (uint8_t)(conformance & 0xFF);
	for (uint8_t i = 0; i < 3; i++) {
		r = osp_axdr_write_u8(buf, b[i]);
		if (r != OSP_OK) {
			return r;
		}
	}
	return OSP_OK;
}

static osp_err_t take_conformance(osp_buf_t *buf, uint32_t *conformance) {
	if (!buf || !conformance) {
		return OSP_ERR_INVALID;
	}
	if (osp_buf_unread(buf) < 3) {
		return OSP_ERR_INVALID;
	}
	if (buf->buf[buf->rd] != conformance_tag[0] || buf->buf[buf->rd + 1] != conformance_tag[1]) {
		return OSP_ERR_INVALID;
	}
	buf->rd += 2;
	uint8_t len;
	osp_err_t r = osp_axdr_read_u8(buf, &len);
	if (r != OSP_OK) {
		return r;
	}
	if (len < 1 || osp_buf_unread(buf) < len) {
		return OSP_ERR_INVALID;
	}
	buf->rd++; /* skip unused-bits octet */
	uint32_t conf = 0;
	for (uint8_t i = 1; i < len; i++) {
		conf = (conf << 8) | buf->buf[buf->rd++];
	}
	*conformance = conf;
	return OSP_OK;
}

void osp_initiate_request_default(osp_initiate_request_t *req) {
	if (!req) {
		return;
	}
	memset(req, 0, sizeof(*req));
	req->response_allowed = true;
	req->proposed_dlms_version = 6;
	req->proposed_conformance = 0x007E1F;
	req->client_max_receive_pdu_size = 0x04B0;
}

void osp_initiate_response_default(osp_initiate_response_t *resp) {
	if (!resp) {
		return;
	}
	memset(resp, 0, sizeof(*resp));
	resp->negotiated_dlms_version = 6;
	resp->negotiated_conformance = 0x007E1F;
	resp->server_max_receive_pdu_size = 0x0800;
	resp->vaa_name = 0x0007;
}

osp_err_t osp_initiate_request_encode(const osp_initiate_request_t *req, osp_buf_t *buf) {
	if (!req || !buf) {
		return OSP_ERR_INVALID;
	}

	osp_err_t r = osp_axdr_write_u8(buf, OSP_INITIATE_REQUEST_TAG);
	if (r != OSP_OK) {
		return r;
	}

	if (!req->has_dedicated_key) {
		r = osp_axdr_write_u8(buf, 0x00);
	} else {
		r = osp_axdr_write_u8(buf, 0x01);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, req->dedicated_key_len);
		if (r != OSP_OK) {
			return r;
		}
		for (uint8_t i = 0; i < req->dedicated_key_len; i++) {
			r = osp_axdr_write_u8(buf, req->dedicated_key[i]);
			if (r != OSP_OK) {
				return r;
			}
		}
	}
	if (r != OSP_OK) {
		return r;
	}

	if (req->response_allowed) {
		r = osp_axdr_write_u8(buf, 0x00);
	} else {
		r = osp_axdr_write_u8(buf, 0x01);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, 0x00);
	}
	if (r != OSP_OK) {
		return r;
	}

	if (!req->has_qos) {
		r = osp_axdr_write_u8(buf, 0x00);
	} else {
		r = osp_axdr_write_u8(buf, 0x01);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, (uint8_t)req->proposed_quality_of_service);
	}
	if (r != OSP_OK) {
		return r;
	}

	r = osp_axdr_write_u8(buf, req->proposed_dlms_version);
	if (r != OSP_OK) {
		return r;
	}
	r = push_conformance(req->proposed_conformance, buf);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_axdr_write_u16(buf, req->client_max_receive_pdu_size);
	return r;
}

osp_err_t osp_initiate_request_decode(osp_buf_t *buf, osp_initiate_request_t *req) {
	if (!buf || !req) {
		return OSP_ERR_INVALID;
	}

	uint8_t tag;
	osp_err_t r = osp_axdr_read_u8(buf, &tag);
	if (r != OSP_OK) {
		return r;
	}
	if (tag != OSP_INITIATE_REQUEST_TAG) {
		return OSP_ERR_INVALID;
	}

	memset(req, 0, sizeof(*req));

	uint8_t flag;
	r = osp_axdr_read_u8(buf, &flag);
	if (r != OSP_OK) {
		return r;
	}
	if (flag == 0x00) {
		req->has_dedicated_key = false;
	} else if (flag == 0x01) {
		uint8_t len;
		r = osp_axdr_read_u8(buf, &len);
		if (r != OSP_OK) {
			return r;
		}
		if (len > OSP_INITIATE_DEDICATED_KEY_MAX || osp_buf_unread(buf) < len) {
			return OSP_ERR_INVALID;
		}
		memcpy(req->dedicated_key, &buf->buf[buf->rd], len);
		buf->rd += len;
		req->dedicated_key_len = len;
		req->has_dedicated_key = true;
	} else {
		return OSP_ERR_INVALID;
	}

	r = osp_axdr_read_u8(buf, &flag);
	if (r != OSP_OK) {
		return r;
	}
	if (flag == 0x00) {
		req->response_allowed = true;
	} else if (flag == 0x01) {
		uint8_t val;
		r = osp_axdr_read_u8(buf, &val);
		if (r != OSP_OK) {
			return r;
		}
		req->response_allowed = (val != 0);
	} else {
		return OSP_ERR_INVALID;
	}

	r = osp_axdr_read_u8(buf, &flag);
	if (r != OSP_OK) {
		return r;
	}
	if (flag == 0x00) {
		req->has_qos = false;
	} else if (flag == 0x01) {
		uint8_t q;
		r = osp_axdr_read_u8(buf, &q);
		if (r != OSP_OK) {
			return r;
		}
		req->proposed_quality_of_service = (int8_t)q;
		req->has_qos = true;
	} else {
		return OSP_ERR_INVALID;
	}

	r = osp_axdr_read_u8(buf, &req->proposed_dlms_version);
	if (r != OSP_OK) {
		return r;
	}
	r = take_conformance(buf, &req->proposed_conformance);
	if (r != OSP_OK) {
		return r;
	}
	return osp_axdr_read_u16(buf, &req->client_max_receive_pdu_size);
}

osp_err_t osp_initiate_response_encode(const osp_initiate_response_t *resp, osp_buf_t *buf) {
	if (!resp || !buf) {
		return OSP_ERR_INVALID;
	}

	osp_err_t r = osp_axdr_write_u8(buf, OSP_INITIATE_RESPONSE_TAG);
	if (r != OSP_OK) {
		return r;
	}

	if (!resp->has_qos) {
		r = osp_axdr_write_u8(buf, 0x00);
	} else {
		r = osp_axdr_write_u8(buf, 0x01);
		if (r != OSP_OK) {
			return r;
		}
		r = osp_axdr_write_u8(buf, (uint8_t)resp->negotiated_quality_of_service);
	}
	if (r != OSP_OK) {
		return r;
	}

	r = osp_axdr_write_u8(buf, resp->negotiated_dlms_version);
	if (r != OSP_OK) {
		return r;
	}
	r = push_conformance(resp->negotiated_conformance, buf);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_axdr_write_u16(buf, resp->server_max_receive_pdu_size);
	if (r != OSP_OK) {
		return r;
	}
	return osp_axdr_write_u16(buf, resp->vaa_name);
}

osp_err_t osp_initiate_response_decode(osp_buf_t *buf, osp_initiate_response_t *resp) {
	if (!buf || !resp) {
		return OSP_ERR_INVALID;
	}

	uint8_t tag;
	osp_err_t r = osp_axdr_read_u8(buf, &tag);
	if (r != OSP_OK) {
		return r;
	}
	if (tag != OSP_INITIATE_RESPONSE_TAG) {
		return OSP_ERR_INVALID;
	}

	memset(resp, 0, sizeof(*resp));

	uint8_t flag;
	r = osp_axdr_read_u8(buf, &flag);
	if (r != OSP_OK) {
		return r;
	}
	if (flag == 0x00) {
		resp->has_qos = false;
	} else if (flag == 0x01) {
		uint8_t q;
		r = osp_axdr_read_u8(buf, &q);
		if (r != OSP_OK) {
			return r;
		}
		resp->negotiated_quality_of_service = (int8_t)q;
		resp->has_qos = true;
	} else {
		return OSP_ERR_INVALID;
	}

	r = osp_axdr_read_u8(buf, &resp->negotiated_dlms_version);
	if (r != OSP_OK) {
		return r;
	}
	r = take_conformance(buf, &resp->negotiated_conformance);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_axdr_read_u16(buf, &resp->server_max_receive_pdu_size);
	if (r != OSP_OK) {
		return r;
	}
	return osp_axdr_read_u16(buf, &resp->vaa_name);
}
