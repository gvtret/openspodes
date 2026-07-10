/**
 * security.c — HLS GMAC handshake + glo-ciphering implementation
 *
 * Implements HLS mechanism 5 (GMAC) per Green Book §9.2.7.4:
 *   Pass 1: Client sends CtoS in AARQ (handled by ACSE)
 *   Pass 2: Server sends StoC in AARE (handled by ACSE)
 *   Pass 3: Client builds f(StoC) = SC || IC || GMAC(SC || AK || StoC)
 *            and sends via ACTION reply_to_HLS_authentication
 *   Pass 4: Server builds f(CtoS) = SC || IC || GMAC(SC || AK || CtoS)
 *            and returns in ACTION response
 *
 * GMAC = AES-GCM with zero-length plaintext (authentication only).
 * IV = system_title(8) || IC(4) (12 bytes).
 * AAD = SC(1) || AK(16) || data.
 */

#include "security.h"
#include "../codec/codec.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  HAL crypto function pointers (set by application at startup)
 * ═══════════════════════════════════════════════════════════════════════════ */

void (*osp_hal_gcm_init)(osp_sec_key_id key_id, uint32_t key_len, const uint8_t *iv, uint32_t iv_len, const uint8_t *aad, uint32_t aad_len) = NULL;
void (*osp_hal_gcm_update)(const uint8_t *in, uint32_t len, uint8_t *out) = NULL;
void (*osp_hal_gcm_finish)(uint8_t tag[OSP_SEC_TAG_SIZE]) = NULL;
int (*osp_hal_gcm_crypt)(osp_gcm_dir_t dir, const uint8_t *key, uint32_t key_len, const uint8_t iv[12], const uint8_t *aad, uint32_t aad_len,
                         const uint8_t *in, uint32_t in_len, uint8_t *out, const uint8_t tag_in[OSP_SEC_TAG_SIZE], uint8_t tag_out[OSP_SEC_TAG_SIZE]) = NULL;
void (*osp_hal_md5)(const uint8_t *input, uint32_t len, uint8_t output[16]) = NULL;
void (*osp_hal_sha1)(const uint8_t *input, uint32_t len, uint8_t output[20]) = NULL;
void (*osp_hal_sha256)(const uint8_t *input, uint32_t len, uint8_t output[32]) = NULL;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Security context
 * ═══════════════════════════════════════════════════════════════════════════ */

void osp_sec_context_init(osp_sec_context_t *ctx, osp_sec_suite_t suite, osp_auth_mechanism_t mech, const uint8_t *system_title) {
	if (!ctx)
		return;

	memset(ctx, 0, sizeof(*ctx));
	ctx->suite = suite;
	ctx->mechanism = mech;
	ctx->policy = OSP_POLICY_NONE;
	ctx->invocation_counter = 0;
	ctx->last_peer_ic = 0;
	ctx->ic_valid = false;
	ctx->hls_failures = 0;

	if (system_title) {
		memcpy(ctx->system_title, system_title, OSP_SEC_SYSTEM_TITLE_SIZE);
	}
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  GMAC computation using HAL crypto
 * ═══════════════════════════════════════════════════════════════════════════ */

int osp_hls_gmac(
    const osp_sec_context_t *ctx, const uint8_t *system_title, uint32_t ic, const uint8_t *data, uint32_t data_len, uint8_t tag[OSP_SEC_TAG_SIZE]
) {
	if (!ctx || !system_title || !tag)
		return -1;

	/* IV = system_title(8) || IC(4) (big-endian) */
	uint8_t iv[12];
	memcpy(iv, system_title, 8);
	iv[8] = (uint8_t)(ic >> 24);
	iv[9] = (uint8_t)(ic >> 16);
	iv[10] = (uint8_t)(ic >> 8);
	iv[11] = (uint8_t)(ic);

	/* Build AAD = SC(1) || AK(16) || data */
	uint8_t aad_buf[17 + 256];
	uint8_t sc = osp_sec_control_byte(OSP_POLICY_AUTH_ONLY, ctx->suite);
	aad_buf[0] = sc;
	memcpy(&aad_buf[1], ctx->gak, 16);
	if (data && data_len > 0 && data_len <= 256) {
		memcpy(&aad_buf[17], data, data_len);
	}

	/* Use HAL GCM: init(encrypt) → update(zero plaintext) → finish(tag) */
	if (osp_hal_gcm_init != NULL) {
		osp_hal_gcm_init(OSP_SEC_GUEK, 16, iv, 12, aad_buf, 17 + data_len);
		osp_hal_gcm_update(NULL, 0, NULL); /* zero-length plaintext */
		osp_hal_gcm_finish(tag);
	} else {
		return -1;
	}

	return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  HLS pass 3/4
 * ═══════════════════════════════════════════════════════════════════════════ */

int osp_hls_pass3_build(const osp_sec_context_t *ctx, uint8_t *out, uint32_t out_size) {
	if (!ctx || !out || out_size < 17)
		return -1;

	/* f(StoC) = SC || IC || GMAC(SC || AK || StoC) */
	uint8_t sc = osp_sec_control_byte(OSP_POLICY_AUTH_ONLY, ctx->suite);
	uint32_t ic = ctx->invocation_counter;

	uint8_t tag[OSP_SEC_TAG_SIZE];
	if (osp_hls_gmac(ctx, ctx->system_title, ic, ctx->stoc, ctx->stoc_len, tag) != 0) {
		return -1;
	}

	out[0] = sc;
	out[1] = (uint8_t)(ic >> 24);
	out[2] = (uint8_t)(ic >> 16);
	out[3] = (uint8_t)(ic >> 8);
	out[4] = (uint8_t)(ic);
	memcpy(&out[5], tag, OSP_SEC_TAG_SIZE);

	return 17;
}

int osp_hls_pass3_verify(osp_sec_context_t *ctx, const uint8_t *f_stoc, uint32_t len) {
	if (!ctx || !f_stoc || len < 17)
		return -1;

	/* Rate limiting */
	if (ctx->hls_failures >= 5)
		return -2;

	/* Replay protection: IC must be strictly increasing */
	uint32_t ic = ((uint32_t)f_stoc[1] << 24) | ((uint32_t)f_stoc[2] << 16) | ((uint32_t)f_stoc[3] << 8) | (uint32_t)f_stoc[4];
	if (ctx->ic_valid && ic <= ctx->last_peer_ic) {
		ctx->hls_failures++;
		return -3;
	}

	/* Rebuild expected tag: GMAC(SC || AK || StoC) using peer's IC */
	uint8_t expected_tag[OSP_SEC_TAG_SIZE];
	if (osp_hls_gmac(ctx, ctx->system_title, ic, ctx->stoc, ctx->stoc_len, expected_tag) != 0) {
		ctx->hls_failures++;
		return -4;
	}

	/* Constant-time comparison */
	uint8_t diff = 0;
	for (uint32_t i = 0; i < OSP_SEC_TAG_SIZE; i++) {
		diff |= f_stoc[5 + i] ^ expected_tag[i];
	}

	if (diff != 0) {
		ctx->hls_failures++;
		return -5;
	}

	/* Success: update IC tracking */
	ctx->last_peer_ic = ic;
	ctx->ic_valid = true;
	ctx->hls_failures = 0;
	return 0;
}

int osp_hls_pass4_build(osp_sec_context_t *ctx, uint8_t *out, uint32_t out_size) {
	if (!ctx || !out || out_size < 17)
		return -1;

	/* f(CtoS) = SC || IC || GMAC(SC || AK || CtoS) */
	uint8_t sc = osp_sec_control_byte(OSP_POLICY_AUTH_ONLY, ctx->suite);
	uint32_t ic = ctx->invocation_counter++;
	ctx->invocation_counter++;

	uint8_t tag[OSP_SEC_TAG_SIZE];
	if (osp_hls_gmac(ctx, ctx->system_title, ic, ctx->ctos, ctx->ctos_len, tag) != 0) {
		return -1;
	}

	out[0] = sc;
	out[1] = (uint8_t)(ic >> 24);
	out[2] = (uint8_t)(ic >> 16);
	out[3] = (uint8_t)(ic >> 8);
	out[4] = (uint8_t)(ic);
	memcpy(&out[5], tag, OSP_SEC_TAG_SIZE);

	return 17;
}

int osp_hls_pass4_verify(osp_sec_context_t *ctx, const uint8_t *f_ctos, uint32_t len) {
	if (!ctx || !f_ctos || len < 17)
		return -1;

	/* Rebuild expected tag: GMAC(SC || AK || CtoS) using peer's IC */
	uint32_t ic = ((uint32_t)f_ctos[1] << 24) | ((uint32_t)f_ctos[2] << 16) | ((uint32_t)f_ctos[3] << 8) | (uint32_t)f_ctos[4];

	uint8_t expected_tag[OSP_SEC_TAG_SIZE];
	if (osp_hls_gmac(ctx, ctx->system_title, ic, ctx->ctos, ctx->ctos_len, expected_tag) != 0) {
		return -1;
	}

	/* Constant-time comparison */
	uint8_t diff = 0;
	for (uint32_t i = 0; i < OSP_SEC_TAG_SIZE; i++) {
		diff |= f_ctos[5 + i] ^ expected_tag[i];
	}

	return (diff == 0) ? 0 : -1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Glo-ciphering (IEC 62056-5-3 §5.7)
 * ═══════════════════════════════════════════════════════════════════════════ */

bool osp_glo_is_ciphered_tag(uint8_t tag) {
	switch (tag) {
		case OSP_GLO_INITIATE_REQUEST:
		case OSP_GLO_INITIATE_RESPONSE:
		case OSP_GLO_GET_REQUEST:
		case OSP_GLO_SET_REQUEST:
		case OSP_GLO_ACTION_REQUEST:
		case OSP_GLO_GET_RESPONSE:
		case OSP_GLO_SET_RESPONSE:
		case OSP_GLO_ACTION_RESPONSE:
			return true;
		default:
			return false;
	}
}

uint8_t osp_glo_tag_for_plain(uint8_t plain_tag) {
	if (plain_tag >= 0xC0 && plain_tag <= 0xC7) {
		return (uint8_t)(plain_tag + 0x08);
	}
	return plain_tag;
}

uint8_t osp_glo_plain_tag_for_ciphered(uint8_t ciphered_tag) {
	if (ciphered_tag >= 0xC8 && ciphered_tag <= 0xCF) {
		return (uint8_t)(ciphered_tag - 0x08);
	}
	return ciphered_tag;
}

static void glo_build_iv(const osp_sec_context_t *ctx, uint32_t ic, uint8_t iv[12]) {
	memcpy(iv, ctx->system_title, 8);
	iv[8] = (uint8_t)(ic >> 24);
	iv[9] = (uint8_t)(ic >> 16);
	iv[10] = (uint8_t)(ic >> 8);
	iv[11] = (uint8_t)(ic);
}

static int glo_write_ciphered_apdu(uint8_t ciphered_tag, const uint8_t *body, uint32_t body_len, uint8_t *out, uint32_t *out_len) {
	if (!out || !out_len || body_len + 8 > OSP_GLO_MAX_CIPHERED) {
		return -1;
	}
	osp_buf_t buf;
	osp_buf_init(&buf, out, OSP_GLO_MAX_CIPHERED);
	osp_axdr_write_u8(&buf, ciphered_tag);
	if (osp_dlms_write_len(&buf, body_len) != OSP_OK) {
		return -1;
	}
	if (osp_buf_free(&buf) < body_len) {
		return -1;
	}
	memcpy(&buf.buf[buf.wr], body, body_len);
	buf.wr += body_len;
	*out_len = buf.wr;
	return 0;
}

int osp_glo_protect(const osp_sec_context_t *ctx, uint8_t ciphered_tag, const uint8_t *plaintext, uint32_t plain_len, uint8_t *out, uint32_t *out_len) {
	if (!ctx || !plaintext || !out || !out_len || plain_len > OSP_GLO_MAX_PLAIN || !osp_hal_gcm_crypt) {
		return -1;
	}

	uint8_t sc = osp_sec_control_byte(ctx->policy, ctx->suite);
	uint32_t ic = ctx->invocation_counter;
	uint32_t key_len = osp_sec_suite_aes_key_len(ctx->suite);
	bool auth = (sc & 0x10u) != 0;
	bool encr = (sc & 0x20u) != 0;

	uint8_t iv[12];
	glo_build_iv(ctx, ic, iv);

	uint8_t protected_part[OSP_GLO_MAX_PLAIN + OSP_SEC_TAG_SIZE];
	uint32_t protected_len = 0;
	uint8_t tag[OSP_SEC_TAG_SIZE];

	if (auth && encr) {
		uint8_t aad[17];
		aad[0] = sc;
		memcpy(&aad[1], ctx->gak, 16);
		if (osp_hal_gcm_crypt(OSP_GCM_ENCRYPT, ctx->guek, key_len, iv, aad, 17, plaintext, plain_len, protected_part, NULL, tag) != 0) {
			return -1;
		}
		memcpy(&protected_part[plain_len], tag, OSP_SEC_TAG_SIZE);
		protected_len = plain_len + OSP_SEC_TAG_SIZE;
	} else if (auth && !encr) {
		uint8_t aad[17 + OSP_GLO_MAX_PLAIN];
		if (17 + plain_len > sizeof(aad)) {
			return -1;
		}
		aad[0] = sc;
		memcpy(&aad[1], ctx->gak, 16);
		memcpy(&aad[17], plaintext, plain_len);
		if (osp_hal_gcm_crypt(OSP_GCM_ENCRYPT, ctx->guek, key_len, iv, aad, 17 + plain_len, NULL, 0, NULL, NULL, tag) != 0) {
			return -1;
		}
		memcpy(protected_part, plaintext, plain_len);
		memcpy(&protected_part[plain_len], tag, OSP_SEC_TAG_SIZE);
		protected_len = plain_len + OSP_SEC_TAG_SIZE;
	} else if (!auth && encr) {
		if (osp_hal_gcm_crypt(OSP_GCM_ENCRYPT, ctx->guek, key_len, iv, NULL, 0, plaintext, plain_len, protected_part, NULL, NULL) != 0) {
			return -1;
		}
		protected_len = plain_len;
	} else {
		memcpy(protected_part, plaintext, plain_len);
		protected_len = plain_len;
	}

	uint8_t body[5 + OSP_GLO_MAX_PLAIN + OSP_SEC_TAG_SIZE];
	body[0] = sc;
	body[1] = (uint8_t)(ic >> 24);
	body[2] = (uint8_t)(ic >> 16);
	body[3] = (uint8_t)(ic >> 8);
	body[4] = (uint8_t)(ic);
	memcpy(&body[5], protected_part, protected_len);
	return glo_write_ciphered_apdu(ciphered_tag, body, 5 + protected_len, out, out_len);
}

int osp_glo_unprotect(osp_sec_context_t *ctx, const uint8_t *ciphered, uint32_t ciphered_len, uint8_t *plaintext, uint32_t *plain_len) {
	if (!ctx || !ciphered || ciphered_len < 7 || !plaintext || !plain_len || !osp_hal_gcm_crypt) {
		return -1;
	}
	if (!osp_glo_is_ciphered_tag(ciphered[0])) {
		return -1;
	}

	osp_buf_t buf;
	osp_buf_init(&buf, (uint8_t *)ciphered, ciphered_len);
	buf.wr = ciphered_len;
	uint8_t tag;
	if (osp_axdr_read_u8(&buf, &tag) != OSP_OK) {
		return -1;
	}
	uint32_t body_len;
	if (osp_dlms_read_len(&buf, &body_len) != OSP_OK || body_len < 5 || osp_buf_unread(&buf) < body_len) {
		return -1;
	}

	const uint8_t *body = &buf.buf[buf.rd];
	uint8_t sc = body[0];
	uint32_t ic = ((uint32_t)body[1] << 24) | ((uint32_t)body[2] << 16) | ((uint32_t)body[3] << 8) | (uint32_t)body[4];
	if (ctx->ic_valid && ic <= ctx->last_peer_ic) {
		return -2;
	}

	const uint8_t *protected_part = &body[5];
	uint32_t protected_len = body_len - 5;
	bool auth = (sc & 0x10u) != 0;
	bool encr = (sc & 0x20u) != 0;
	uint32_t key_len = osp_sec_suite_aes_key_len(ctx->suite);
	uint8_t iv[12];
	glo_build_iv(ctx, ic, iv);

	if (auth && encr) {
		if (protected_len < OSP_SEC_TAG_SIZE) {
			return -1;
		}
		uint32_t ct_len = protected_len - OSP_SEC_TAG_SIZE;
		if (ct_len > OSP_GLO_MAX_PLAIN) {
			return -1;
		}
		uint8_t aad[17];
		aad[0] = sc;
		memcpy(&aad[1], ctx->gak, 16);
		if (osp_hal_gcm_crypt(OSP_GCM_DECRYPT, ctx->guek, key_len, iv, aad, 17, protected_part, ct_len, plaintext, &protected_part[ct_len], NULL) != 0) {
			return -3;
		}
		*plain_len = ct_len;
	} else if (auth && !encr) {
		if (protected_len < OSP_SEC_TAG_SIZE) {
			return -1;
		}
		uint32_t plen = protected_len - OSP_SEC_TAG_SIZE;
		if (plen > OSP_GLO_MAX_PLAIN) {
			return -1;
		}
		uint8_t aad[17 + OSP_GLO_MAX_PLAIN];
		aad[0] = sc;
		memcpy(&aad[1], ctx->gak, 16);
		memcpy(&aad[17], protected_part, plen);
		if (osp_hal_gcm_crypt(OSP_GCM_DECRYPT, ctx->guek, key_len, iv, aad, 17 + plen, NULL, 0, NULL, &protected_part[plen], NULL) != 0) {
			return -3;
		}
		memcpy(plaintext, protected_part, plen);
		*plain_len = plen;
	} else if (!auth && encr) {
		if (protected_len > OSP_GLO_MAX_PLAIN) {
			return -1;
		}
		if (osp_hal_gcm_crypt(OSP_GCM_DECRYPT, ctx->guek, key_len, iv, NULL, 0, protected_part, protected_len, plaintext, NULL, NULL) != 0) {
			return -3;
		}
		*plain_len = protected_len;
	} else {
		if (protected_len > OSP_GLO_MAX_PLAIN) {
			return -1;
		}
		memcpy(plaintext, protected_part, protected_len);
		*plain_len = protected_len;
	}

	ctx->last_peer_ic = ic;
	ctx->ic_valid = true;
	ctx->policy = osp_sec_control_to_policy(sc);
	return 0;
}
