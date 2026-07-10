#include "general_ciphering.h"
#include "../codec/codec.h"
#include "gost_crypto.h"
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

static int sec_sign_content(const osp_sec_context_t *ctx, const uint8_t *content, uint32_t content_len, uint8_t *sig,
                            uint32_t *sig_len) {
	if (!ctx || !content || !sig || !sig_len) {
		return -1;
	}
	if (ctx->mechanism == OSP_MECH_HLS_GOST_SIG) {
		if (ctx->signing_key_len != OSP_GOST_PRIVKEY_SIZE) {
			return -1;
		}
		if (osp_gost3410_sign(ctx->signing_key, content, content_len, sig) != 0) {
			return -1;
		}
		*sig_len = OSP_GOST_SIG_SIZE;
		return 0;
	}
	if (ctx->mechanism == OSP_MECH_HLS_ECDSA) {
		if (!osp_hal_ecdsa_sign || ctx->suite == OSP_SUITE_0 || ctx->signing_key_len == 0) {
			return -1;
		}
		return osp_hal_ecdsa_sign(ctx->suite, ctx->signing_key, ctx->signing_key_len, content, content_len, sig, sig_len);
	}
	return -1;
}

static int sec_verify_content(osp_sec_context_t *ctx, const uint8_t *content, uint32_t content_len, const uint8_t *sig,
                              uint32_t sig_len) {
	if (!ctx || !content || !sig) {
		return -1;
	}
	if (ctx->mechanism == OSP_MECH_HLS_GOST_SIG) {
		if (sig_len != OSP_GOST_SIG_SIZE || ctx->peer_public_key_len != OSP_GOST_PUBKEY_SIZE) {
			return -1;
		}
		return osp_gost3410_verify(ctx->peer_public_key, content, content_len, sig);
	}
	if (ctx->mechanism == OSP_MECH_HLS_ECDSA) {
		if (!osp_hal_ecdsa_verify || ctx->peer_public_key_len == 0) {
			return -1;
		}
		return osp_hal_ecdsa_verify(ctx->suite, ctx->peer_public_key, ctx->peer_public_key_len, content, content_len, sig,
		                            sig_len);
	}
	return -1;
}

int osp_gen_signing_encode(const osp_gen_signing_t *apdu, uint8_t *out, uint32_t *out_len) {
	if (!apdu || !out || !out_len) {
		return -1;
	}
	osp_buf_t buf;
	osp_buf_init(&buf, out, OSP_GLO_MAX_CIPHERED + 128);
	if (osp_axdr_write_u8(&buf, OSP_GEN_SIGNING) != OSP_OK) {
		return -1;
	}
	if (write_octet_string(&buf, apdu->transaction_id, apdu->transaction_id_len) != 0) {
		return -1;
	}
	if (write_octet_string(&buf, apdu->originator_st, OSP_SEC_SYSTEM_TITLE_SIZE) != 0) {
		return -1;
	}
	if (write_octet_string(&buf, apdu->recipient_st, apdu->recipient_st_len) != 0) {
		return -1;
	}
	if (write_octet_string(&buf, apdu->date_time, apdu->date_time_len) != 0) {
		return -1;
	}
	if (write_octet_string(&buf, apdu->other_information, apdu->other_information_len) != 0) {
		return -1;
	}
	if (write_octet_string(&buf, apdu->content, apdu->content_len) != 0) {
		return -1;
	}
	if (write_octet_string(&buf, apdu->signature, apdu->signature_len) != 0) {
		return -1;
	}
	*out_len = buf.wr;
	return 0;
}

int osp_gen_signing_decode(const uint8_t *apdu, uint32_t apdu_len, osp_gen_signing_t *decoded) {
	if (!apdu || apdu_len < 2 || !decoded) {
		return -1;
	}
	memset(decoded, 0, sizeof(*decoded));
	osp_buf_t buf;
	osp_buf_init(&buf, (uint8_t *)apdu, apdu_len);
	buf.wr = apdu_len;
	uint8_t tag;
	if (osp_axdr_read_u8(&buf, &tag) != OSP_OK || tag != OSP_GEN_SIGNING) {
		return -1;
	}
	const uint8_t *tmp;
	uint32_t tmp_len;
	if (read_octet_string(&buf, &tmp, &tmp_len) != 0 || tmp_len > sizeof(decoded->transaction_id)) {
		return -1;
	}
	memcpy(decoded->transaction_id, tmp, tmp_len);
	decoded->transaction_id_len = tmp_len;
	if (read_octet_string(&buf, &tmp, &tmp_len) != 0 || tmp_len != OSP_SEC_SYSTEM_TITLE_SIZE) {
		return -1;
	}
	memcpy(decoded->originator_st, tmp, tmp_len);
	if (read_octet_string(&buf, &tmp, &tmp_len) != 0 || tmp_len > sizeof(decoded->recipient_st)) {
		return -1;
	}
	memcpy(decoded->recipient_st, tmp, tmp_len);
	decoded->recipient_st_len = tmp_len;
	if (read_octet_string(&buf, &tmp, &tmp_len) != 0 || tmp_len > sizeof(decoded->date_time)) {
		return -1;
	}
	memcpy(decoded->date_time, tmp, tmp_len);
	decoded->date_time_len = tmp_len;
	if (read_octet_string(&buf, &tmp, &tmp_len) != 0 || tmp_len > sizeof(decoded->other_information)) {
		return -1;
	}
	memcpy(decoded->other_information, tmp, tmp_len);
	decoded->other_information_len = tmp_len;
	if (read_octet_string(&buf, &tmp, &tmp_len) != 0 || tmp_len > sizeof(decoded->content)) {
		return -1;
	}
	memcpy(decoded->content, tmp, tmp_len);
	decoded->content_len = tmp_len;
	if (read_octet_string(&buf, &tmp, &tmp_len) != 0 || tmp_len > sizeof(decoded->signature)) {
		return -1;
	}
	memcpy(decoded->signature, tmp, tmp_len);
	decoded->signature_len = tmp_len;
	return 0;
}

int osp_gen_signing_protect(const osp_sec_context_t *ctx, const uint8_t *transaction_id, uint32_t tx_id_len,
                            const uint8_t *recipient_st, uint32_t recipient_len, const uint8_t *content, uint32_t content_len,
                            uint8_t *out, uint32_t *out_len) {
	if (!ctx || !transaction_id || !content || !out || !out_len || content_len > OSP_GLO_MAX_PLAIN) {
		return -1;
	}
	osp_gen_signing_t apdu;
	memset(&apdu, 0, sizeof(apdu));
	if (tx_id_len > sizeof(apdu.transaction_id)) {
		return -1;
	}
	memcpy(apdu.transaction_id, transaction_id, tx_id_len);
	apdu.transaction_id_len = tx_id_len;
	memcpy(apdu.originator_st, ctx->system_title, OSP_SEC_SYSTEM_TITLE_SIZE);
	if (recipient_st && recipient_len > 0) {
		if (recipient_len > sizeof(apdu.recipient_st)) {
			return -1;
		}
		memcpy(apdu.recipient_st, recipient_st, recipient_len);
		apdu.recipient_st_len = recipient_len;
	}
	memcpy(apdu.content, content, content_len);
	apdu.content_len = content_len;
	if (sec_sign_content(ctx, content, content_len, apdu.signature, &apdu.signature_len) != 0) {
		return -1;
	}
	return osp_gen_signing_encode(&apdu, out, out_len);
}

int osp_gen_signing_unprotect(osp_sec_context_t *ctx, const uint8_t *apdu, uint32_t apdu_len, uint8_t *content,
                              uint32_t *content_len) {
	if (!ctx || !apdu || !content || !content_len) {
		return -1;
	}
	osp_gen_signing_t decoded;
	if (osp_gen_signing_decode(apdu, apdu_len, &decoded) != 0) {
		return -1;
	}
	memcpy(ctx->peer_system_title, decoded.originator_st, OSP_SEC_SYSTEM_TITLE_SIZE);
	if (sec_verify_content(ctx, decoded.content, decoded.content_len, decoded.signature, decoded.signature_len) != 0) {
		return -1;
	}
	if (decoded.content_len > *content_len) {
		return -1;
	}
	memcpy(content, decoded.content, decoded.content_len);
	*content_len = decoded.content_len;
	return 0;
}
