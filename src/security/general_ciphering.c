#include "general_ciphering.h"
#include "../codec/codec.h"
#include <string.h>

bool osp_gen_is_ciphered_tag(uint8_t tag) {
	return tag == OSP_GEN_GLO_CIPHERING || tag == OSP_GEN_DED_CIPHERING || tag == OSP_GEN_CIPHERING || tag == OSP_GEN_SIGNING;
}

static int write_octet_string(osp_buf_t *buf, const uint8_t *data, uint32_t len) {
	if (osp_dlms_write_len(buf, len) != OSP_OK) {
		return -1;
	}
	if (len > 0 && osp_buf_free(buf) < len) {
		return -1;
	}
	if (len > 0) {
		memcpy(&buf->buf[buf->wr], data, len);
		buf->wr += len;
	}
	return 0;
}

static int read_octet_string(osp_buf_t *buf, const uint8_t **data, uint32_t *len) {
	if (osp_dlms_read_len(buf, len) != OSP_OK) {
		return -1;
	}
	if (*len > 0 && osp_buf_unread(buf) < *len) {
		return -1;
	}
	*data = (*len > 0) ? &buf->buf[buf->rd] : NULL;
	buf->rd += *len;
	return 0;
}

static int gen_glo_ded_encode(uint8_t tag, const uint8_t *system_title, uint32_t st_len, const uint8_t *ciphered_service,
                              uint32_t cs_len, uint8_t *out, uint32_t *out_len) {
	if (!out || !out_len) {
		return -1;
	}
	osp_buf_t buf;
	osp_buf_init(&buf, out, OSP_GLO_MAX_CIPHERED + 32);
	if (osp_axdr_write_u8(&buf, tag) != OSP_OK) {
		return -1;
	}
	if (write_octet_string(&buf, system_title, st_len) != 0) {
		return -1;
	}
	if (write_octet_string(&buf, ciphered_service, cs_len) != 0) {
		return -1;
	}
	*out_len = buf.wr;
	return 0;
}

int osp_gen_glo_ded_protect(const osp_sec_context_t *ctx, bool dedicated, uint8_t plain_tag, const uint8_t *plaintext, uint32_t plain_len,
                            uint8_t *out, uint32_t *out_len) {
	if (!ctx || !plaintext || !out || !out_len) {
		return -1;
	}
	uint8_t ciphered_tag = osp_glo_tag_for_plain(plain_tag);
	uint8_t body[OSP_GLO_MAX_CIPHERED];
	uint32_t body_len = 0;
	if (osp_glo_protect(ctx, ciphered_tag, plaintext, plain_len, body, &body_len) != 0) {
		return -1;
	}
	uint8_t tag = dedicated ? OSP_GEN_DED_CIPHERING : OSP_GEN_GLO_CIPHERING;
	return gen_glo_ded_encode(tag, ctx->system_title, OSP_SEC_SYSTEM_TITLE_SIZE, body, body_len, out, out_len);
}

int osp_gen_glo_ded_unprotect(osp_sec_context_t *ctx, const uint8_t *apdu, uint32_t apdu_len, uint8_t *plaintext, uint32_t *plain_len,
                              uint8_t *plain_tag) {
	if (!ctx || !apdu || apdu_len < 3 || !plaintext || !plain_len || !plain_tag) {
		return -1;
	}
	osp_buf_t buf;
	osp_buf_init(&buf, (uint8_t *)apdu, apdu_len);
	buf.wr = apdu_len;
	uint8_t tag;
	if (osp_axdr_read_u8(&buf, &tag) != OSP_OK || (tag != OSP_GEN_GLO_CIPHERING && tag != OSP_GEN_DED_CIPHERING)) {
		return -1;
	}
	const uint8_t *st;
	uint32_t st_len;
	if (read_octet_string(&buf, &st, &st_len) != 0) {
		return -1;
	}
	if (st_len == OSP_SEC_SYSTEM_TITLE_SIZE) {
		memcpy(ctx->peer_system_title, st, st_len);
	}
	const uint8_t *ciphered;
	uint32_t cs_len;
	if (read_octet_string(&buf, &ciphered, &cs_len) != 0 || cs_len < 1) {
		return -1;
	}
	if (osp_glo_unprotect(ctx, ciphered, cs_len, plaintext, plain_len) != 0) {
		return -1;
	}
	*plain_tag = osp_glo_plain_tag_for_ciphered(plaintext[0]);
	return 0;
}

int osp_gen_ciphering_protect(const osp_sec_context_t *ctx, const uint8_t *transaction_id, uint32_t tx_id_len, const uint8_t *recipient_st,
                              uint32_t recipient_len, uint8_t plain_tag, const uint8_t *plaintext, uint32_t plain_len, uint8_t *out,
                              uint32_t *out_len) {
	if (!ctx || !plaintext || !out || !out_len) {
		return -1;
	}
	uint8_t ciphered_tag = osp_glo_tag_for_plain(plain_tag);
	uint8_t body[OSP_GLO_MAX_CIPHERED];
	uint32_t body_len = 0;
	if (osp_glo_protect(ctx, ciphered_tag, plaintext, plain_len, body, &body_len) != 0) {
		return -1;
	}

	osp_buf_t buf;
	osp_buf_init(&buf, out, OSP_GLO_MAX_CIPHERED + 64);
	if (osp_axdr_write_u8(&buf, OSP_GEN_CIPHERING) != OSP_OK) {
		return -1;
	}
	if (write_octet_string(&buf, transaction_id, tx_id_len) != 0) {
		return -1;
	}
	if (write_octet_string(&buf, ctx->system_title, OSP_SEC_SYSTEM_TITLE_SIZE) != 0) {
		return -1;
	}
	if (write_octet_string(&buf, recipient_st, recipient_len) != 0) {
		return -1;
	}
	if (write_octet_string(&buf, NULL, 0) != 0 || write_octet_string(&buf, NULL, 0) != 0) {
		return -1;
	}
	if (osp_axdr_write_u8(&buf, 0x00) != OSP_OK) { /* key-info absent */
		return -1;
	}
	if (write_octet_string(&buf, body, body_len) != 0) {
		return -1;
	}
	*out_len = buf.wr;
	return 0;
}

int osp_gen_ciphering_unprotect(osp_sec_context_t *ctx, const uint8_t *apdu, uint32_t apdu_len, uint8_t *plaintext, uint32_t *plain_len,
                                uint8_t *plain_tag) {
	if (!ctx || !apdu || apdu_len < 5 || !plaintext || !plain_len || !plain_tag) {
		return -1;
	}
	osp_buf_t buf;
	osp_buf_init(&buf, (uint8_t *)apdu, apdu_len);
	buf.wr = apdu_len;
	uint8_t tag;
	if (osp_axdr_read_u8(&buf, &tag) != OSP_OK || tag != OSP_GEN_CIPHERING) {
		return -1;
	}
	const uint8_t *tmp;
	uint32_t tmp_len;
	if (read_octet_string(&buf, &tmp, &tmp_len) != 0) {
		return -1;
	}
	if (read_octet_string(&buf, &tmp, &tmp_len) != 0 || tmp_len != OSP_SEC_SYSTEM_TITLE_SIZE) {
		return -1;
	}
	memcpy(ctx->peer_system_title, tmp, tmp_len);
	if (read_octet_string(&buf, &tmp, &tmp_len) != 0) {
		return -1;
	}
	if (read_octet_string(&buf, &tmp, &tmp_len) != 0 || read_octet_string(&buf, &tmp, &tmp_len) != 0) {
		return -1;
	}
	uint8_t key_info;
	if (osp_axdr_read_u8(&buf, &key_info) != OSP_OK || key_info != 0x00) {
		return -1;
	}
	const uint8_t *ciphered;
	uint32_t cs_len;
	if (read_octet_string(&buf, &ciphered, &cs_len) != 0) {
		return -1;
	}
	if (osp_glo_unprotect(ctx, ciphered, cs_len, plaintext, plain_len) != 0) {
		return -1;
	}
	*plain_tag = osp_glo_plain_tag_for_ciphered(plaintext[0]);
	return 0;
}
