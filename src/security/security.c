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
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  HAL crypto function pointers (set by application at startup)
 * ═══════════════════════════════════════════════════════════════════════════ */

void (*osp_hal_gcm_init)(osp_sec_key_id key_id, uint32_t key_len, const uint8_t *iv, uint32_t iv_len, const uint8_t *aad, uint32_t aad_len) = NULL;
void (*osp_hal_gcm_update)(const uint8_t *in, uint32_t len, uint8_t *out) = NULL;
void (*osp_hal_gcm_finish)(uint8_t tag[OSP_SEC_TAG_SIZE]) = NULL;
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
 *  Glo-ciphering (stub — full impl requires HAL GCM)
 * ═══════════════════════════════════════════════════════════════════════════ */

int osp_glo_protect(const osp_sec_context_t *ctx, uint8_t ciphered_tag, const uint8_t *plaintext, uint32_t plain_len, uint8_t *out, uint32_t *out_len) {
	(void)ctx;
	(void)ciphered_tag;
	(void)plaintext;
	(void)plain_len;
	(void)out;
	(void)out_len;
	/* TODO: implement glo-ciphering (encrypt + auth) */
	return -1;
}

int osp_glo_unprotect(osp_sec_context_t *ctx, const uint8_t *ciphered, uint32_t ciphered_len, uint8_t *plaintext, uint32_t *plain_len) {
	(void)ctx;
	(void)ciphered;
	(void)ciphered_len;
	(void)plaintext;
	(void)plain_len;
	/* TODO: implement glo-deciphering (decrypt + auth verify) */
	return -1;
}
