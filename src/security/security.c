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
#include "gost_crypto.h"
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
void (*osp_hal_streebog256)(const uint8_t *input, uint32_t len, uint8_t output[32]) = NULL;
int (*osp_hal_ecdsa_sign)(osp_sec_suite_t suite, const uint8_t *sk, uint32_t sk_len, const uint8_t *msg, uint32_t msg_len, uint8_t *sig,
                          uint32_t *sig_len) = NULL;
int (*osp_hal_ecdsa_verify)(osp_sec_suite_t suite, const uint8_t *pk, uint32_t pk_len, const uint8_t *msg, uint32_t msg_len,
                            const uint8_t *sig, uint32_t sig_len) = NULL;
int (*osp_hal_random_fill)(uint8_t *buf, uint32_t len) = NULL;

static void hls_streebog256(const uint8_t *input, uint32_t len, uint8_t output[32]) {
	if (osp_hal_streebog256) {
		osp_hal_streebog256(input, len, output);
	} else {
		osp_gost_streebog256(input, len, output);
	}
}

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

void osp_sec_context_destroy(osp_sec_context_t *ctx) {
	if (!ctx)
		return;

	/* Zeroize all key material before releasing the context */
	memset(ctx->guek, 0, sizeof(ctx->guek));
	memset(ctx->gak, 0, sizeof(ctx->gak));
	memset(ctx->gbek, 0, sizeof(ctx->gbek));
	memset(ctx->k_em, 0, sizeof(ctx->k_em));
	memset(ctx->dedicated_key, 0, sizeof(ctx->dedicated_key));
	memset(ctx->signing_key, 0, sizeof(ctx->signing_key));
	memset(ctx->peer_public_key, 0, sizeof(ctx->peer_public_key));
	memset(ctx->ctos, 0, sizeof(ctx->ctos));
	memset(ctx->stoc, 0, sizeof(ctx->stoc));
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
	if (data && data_len > 0) {
		if (data_len > 256)
			data_len = 256;
		memcpy(&aad_buf[17], data, data_len);
	}

	/* Use HAL GCM: init(encrypt) → update(zero plaintext) → finish(tag)
	 * Per IEC 62056-5-3, GMAC uses GAK (Authentication Key), not GUEK. */
	if (osp_hal_gcm_init != NULL) {
		osp_hal_gcm_init(OSP_SEC_GAK, 16, iv, 12, aad_buf, 17 + data_len);
		osp_hal_gcm_update(NULL, 0, NULL); /* zero-length plaintext */
		osp_hal_gcm_finish(tag);
	} else {
		return -1;
	}

	return 0;
}

int osp_hls_md5(const uint8_t *input, uint32_t len, uint8_t output[16]) {
	if (!input || !output || !osp_hal_md5) {
		return -1;
	}
	osp_hal_md5(input, len, output);
	return 0;
}

int osp_hls_sha1(const uint8_t *input, uint32_t len, uint8_t output[20]) {
	if (!input || !output || !osp_hal_sha1) {
		return -1;
	}
	osp_hal_sha1(input, len, output);
	return 0;
}

int osp_hls_sha256(const uint8_t *input, uint32_t len, uint8_t output[32]) {
	if (!input || !output || !osp_hal_sha256) {
		return -1;
	}
	osp_hal_sha256(input, len, output);
	return 0;
}

static int hls_hash_legacy(const osp_sec_context_t *ctx, const uint8_t *challenge, uint32_t challenge_len, uint8_t *out,
                           uint32_t *out_len) {
	uint8_t buf[OSP_SEC_CHALLENGE_MAX + OSP_SEC_KEY_MAX];
	if (!ctx || !challenge || !out || !out_len || challenge_len + 16 > sizeof(buf)) {
		return -1;
	}
	memcpy(buf, challenge, challenge_len);
	memcpy(&buf[challenge_len], ctx->gak, 16);
	uint32_t total = challenge_len + 16;

	if (ctx->mechanism == OSP_MECH_HLS_MD5) {
		if (osp_hls_md5(buf, total, out) != 0) {
			return -1;
		}
		*out_len = 16;
		return 0;
	}
	if (ctx->mechanism == OSP_MECH_HLS_SHA1) {
		if (osp_hls_sha1(buf, total, out) != 0) {
			return -1;
		}
		*out_len = 20;
		return 0;
	}
	return -1;
}

static int hls_hash_with_titles(const osp_sec_context_t *ctx, const uint8_t *st_a, const uint8_t *st_b, const uint8_t *challenge_a,
                                uint32_t challenge_a_len, const uint8_t *challenge_b, uint32_t challenge_b_len, uint8_t *out,
                                uint32_t *out_len) {
	if (!ctx || !st_a || !st_b || !challenge_a || !challenge_b || !out || !out_len) {
		return -1;
	}
	if (ctx->mechanism != OSP_MECH_HLS_SHA256 && ctx->mechanism != OSP_MECH_HLS_GOST_STREEBOG) {
		return -1;
	}
	uint8_t buf[OSP_SEC_KEY_MAX + OSP_SEC_SYSTEM_TITLE_SIZE * 2 + OSP_SEC_CHALLENGE_MAX * 2];
	uint32_t pos = 0;
	memcpy(&buf[pos], ctx->gak, 16);
	pos += 16;
	memcpy(&buf[pos], st_a, OSP_SEC_SYSTEM_TITLE_SIZE);
	pos += OSP_SEC_SYSTEM_TITLE_SIZE;
	memcpy(&buf[pos], st_b, OSP_SEC_SYSTEM_TITLE_SIZE);
	pos += OSP_SEC_SYSTEM_TITLE_SIZE;
	memcpy(&buf[pos], challenge_a, challenge_a_len);
	pos += challenge_a_len;
	memcpy(&buf[pos], challenge_b, challenge_b_len);
	pos += challenge_b_len;
	if (ctx->mechanism == OSP_MECH_HLS_SHA256) {
		if (osp_hls_sha256(buf, pos, out) != 0) {
			return -1;
		}
	} else {
		hls_streebog256(buf, pos, out);
	}
	*out_len = 32;
	return 0;
}

static int hls_build_sig_message(const osp_sec_context_t *ctx, const uint8_t *st_a, const uint8_t *st_b, const uint8_t *challenge_a,
                                 uint32_t challenge_a_len, const uint8_t *challenge_b, uint32_t challenge_b_len, uint8_t *out,
                                 uint32_t *out_len) {
	if (!ctx || !st_a || !st_b || !challenge_a || !challenge_b || !out || !out_len) {
		return -1;
	}
	uint32_t need = OSP_SEC_SYSTEM_TITLE_SIZE * 2 + challenge_a_len + challenge_b_len;
	if (need > OSP_SEC_CHALLENGE_MAX * 2 + OSP_SEC_SYSTEM_TITLE_SIZE * 2) {
		return -1;
	}
	uint32_t pos = 0;
	memcpy(&out[pos], st_a, OSP_SEC_SYSTEM_TITLE_SIZE);
	pos += OSP_SEC_SYSTEM_TITLE_SIZE;
	memcpy(&out[pos], st_b, OSP_SEC_SYSTEM_TITLE_SIZE);
	pos += OSP_SEC_SYSTEM_TITLE_SIZE;
	memcpy(&out[pos], challenge_a, challenge_a_len);
	pos += challenge_a_len;
	memcpy(&out[pos], challenge_b, challenge_b_len);
	pos += challenge_b_len;
	*out_len = pos;
	return 0;
}

static int hls_pass3_gost_cmac_build(const osp_sec_context_t *ctx, uint8_t *out, uint32_t out_size, uint32_t *out_len) {
	if (!ctx || !out || out_size < 21 || !out_len) {
		return -1;
	}
	uint8_t sc = osp_sec_control_byte(OSP_POLICY_AUTH_ONLY, ctx->suite);
	uint32_t ic = ctx->invocation_counter;
	uint8_t iv[12];
	memcpy(iv, ctx->system_title, 8);
	iv[8] = (uint8_t)(ic >> 24);
	iv[9] = (uint8_t)(ic >> 16);
	iv[10] = (uint8_t)(ic >> 8);
	iv[11] = (uint8_t)(ic);
	uint8_t mac[OSP_SEC_GOST_CMAC_SIZE];
	if (osp_hls_gost_cmac(ctx->k_em, iv, sc, ctx->stoc, ctx->stoc_len, ctx->ctos, ctx->ctos_len, mac) != 0) {
		return -1;
	}
	out[0] = sc;
	out[1] = (uint8_t)(ic >> 24);
	out[2] = (uint8_t)(ic >> 16);
	out[3] = (uint8_t)(ic >> 8);
	out[4] = (uint8_t)(ic);
	memcpy(&out[5], mac, OSP_SEC_GOST_CMAC_SIZE);
	*out_len = 21;
	return 0;
}

static int hls_pass4_gost_cmac_build(osp_sec_context_t *ctx, uint8_t *out, uint32_t out_size, uint32_t *out_len) {
	if (!ctx || !out || out_size < 21 || !out_len) {
		return -1;
	}
	uint8_t sc = osp_sec_control_byte(OSP_POLICY_AUTH_ONLY, ctx->suite);
	/* Check for IC overflow — per IEC 62056-5-3, counter must not wrap to 0 */
	if (ctx->invocation_counter == 0xFFFFFFFF) {
		return -1; /* IC overflow — re-keying required */
	}
	uint32_t ic = ctx->invocation_counter++;
	uint8_t iv[12];
	memcpy(iv, ctx->system_title, 8);
	iv[8] = (uint8_t)(ic >> 24);
	iv[9] = (uint8_t)(ic >> 16);
	iv[10] = (uint8_t)(ic >> 8);
	iv[11] = (uint8_t)(ic);
	uint8_t mac[OSP_SEC_GOST_CMAC_SIZE];
	if (osp_hls_gost_cmac(ctx->k_em, iv, sc, ctx->ctos, ctx->ctos_len, ctx->stoc, ctx->stoc_len, mac) != 0) {
		return -1;
	}
	out[0] = sc;
	out[1] = (uint8_t)(ic >> 24);
	out[2] = (uint8_t)(ic >> 16);
	out[3] = (uint8_t)(ic >> 8);
	out[4] = (uint8_t)(ic);
	memcpy(&out[5], mac, OSP_SEC_GOST_CMAC_SIZE);
	*out_len = 21;
	return 0;
}

static int hls_pass3_sig_build(const osp_sec_context_t *ctx, uint8_t *out, uint32_t out_size, uint32_t *out_len) {
	uint8_t msg[OSP_SEC_SYSTEM_TITLE_SIZE * 2 + OSP_SEC_CHALLENGE_MAX * 2];
	uint32_t msg_len = 0;
	if (hls_build_sig_message(ctx, ctx->system_title, ctx->peer_system_title, ctx->stoc, ctx->stoc_len, ctx->ctos, ctx->ctos_len, msg,
	                          &msg_len) != 0) {
		return -1;
	}
	if (ctx->mechanism == OSP_MECH_HLS_ECDSA) {
		if (!osp_hal_ecdsa_sign || ctx->suite == OSP_SUITE_0 || ctx->signing_key_len == 0) {
			return -1;
		}
		return osp_hal_ecdsa_sign(ctx->suite, ctx->signing_key, ctx->signing_key_len, msg, msg_len, out, out_len);
	}
	if (ctx->mechanism == OSP_MECH_HLS_GOST_SIG) {
		if (out_size < OSP_GOST_SIG_SIZE || ctx->signing_key_len != OSP_GOST_PRIVKEY_SIZE) {
			return -1;
		}
		if (osp_gost3410_sign(ctx->signing_key, msg, msg_len, out) != 0) {
			return -1;
		}
		*out_len = OSP_GOST_SIG_SIZE;
		return 0;
	}
	return -1;
}

static int hls_pass4_sig_build(osp_sec_context_t *ctx, uint8_t *out, uint32_t out_size, uint32_t *out_len) {
	uint8_t msg[OSP_SEC_SYSTEM_TITLE_SIZE * 2 + OSP_SEC_CHALLENGE_MAX * 2];
	uint32_t msg_len = 0;
	if (hls_build_sig_message(ctx, ctx->system_title, ctx->peer_system_title, ctx->ctos, ctx->ctos_len, ctx->stoc, ctx->stoc_len, msg,
	                          &msg_len) != 0) {
		return -1;
	}
	if (ctx->mechanism == OSP_MECH_HLS_ECDSA) {
		if (!osp_hal_ecdsa_sign || ctx->suite == OSP_SUITE_0 || ctx->signing_key_len == 0) {
			return -1;
		}
		return osp_hal_ecdsa_sign(ctx->suite, ctx->signing_key, ctx->signing_key_len, msg, msg_len, out, out_len);
	}
	if (ctx->mechanism == OSP_MECH_HLS_GOST_SIG) {
		if (out_size < OSP_GOST_SIG_SIZE || ctx->signing_key_len != OSP_GOST_PRIVKEY_SIZE) {
			return -1;
		}
		if (osp_gost3410_sign(ctx->signing_key, msg, msg_len, out) != 0) {
			return -1;
		}
		*out_len = OSP_GOST_SIG_SIZE;
		return 0;
	}
	return -1;
}

static int hls_pass3_sig_verify(osp_sec_context_t *ctx, const uint8_t *f_stoc, uint32_t len) {
	uint8_t msg[OSP_SEC_SYSTEM_TITLE_SIZE * 2 + OSP_SEC_CHALLENGE_MAX * 2];
	uint32_t msg_len = 0;
	if (hls_build_sig_message(ctx, ctx->peer_system_title, ctx->system_title, ctx->stoc, ctx->stoc_len, ctx->ctos, ctx->ctos_len, msg,
	                          &msg_len) != 0) {
		return -1;
	}
	if (ctx->mechanism == OSP_MECH_HLS_ECDSA) {
		if (!osp_hal_ecdsa_verify || ctx->peer_public_key_len == 0) {
			return -1;
		}
		return osp_hal_ecdsa_verify(ctx->suite, ctx->peer_public_key, ctx->peer_public_key_len, msg, msg_len, f_stoc, len);
	}
	if (ctx->mechanism == OSP_MECH_HLS_GOST_SIG) {
		if (len != OSP_GOST_SIG_SIZE || ctx->peer_public_key_len != OSP_GOST_PUBKEY_SIZE) {
			return -1;
		}
		return osp_gost3410_verify(ctx->peer_public_key, msg, msg_len, f_stoc);
	}
	return -1;
}

static int hls_pass4_sig_verify(osp_sec_context_t *ctx, const uint8_t *f_ctos, uint32_t len) {
	uint8_t msg[OSP_SEC_SYSTEM_TITLE_SIZE * 2 + OSP_SEC_CHALLENGE_MAX * 2];
	uint32_t msg_len = 0;
	if (hls_build_sig_message(ctx, ctx->peer_system_title, ctx->system_title, ctx->ctos, ctx->ctos_len, ctx->stoc, ctx->stoc_len, msg,
	                          &msg_len) != 0) {
		return -1;
	}
	if (ctx->mechanism == OSP_MECH_HLS_ECDSA) {
		if (!osp_hal_ecdsa_verify || ctx->peer_public_key_len == 0) {
			return -1;
		}
		return osp_hal_ecdsa_verify(ctx->suite, ctx->peer_public_key, ctx->peer_public_key_len, msg, msg_len, f_ctos, len);
	}
	if (ctx->mechanism == OSP_MECH_HLS_GOST_SIG) {
		if (len != OSP_GOST_SIG_SIZE || ctx->peer_public_key_len != OSP_GOST_PUBKEY_SIZE) {
			return -1;
		}
		return osp_gost3410_verify(ctx->peer_public_key, msg, msg_len, f_ctos);
	}
	return -1;
}

static int hls_pass3_gmac_build(const osp_sec_context_t *ctx, uint8_t *out, uint32_t out_size, uint32_t *out_len) {
	if (!ctx || !out || out_size < 17 || !out_len) {
		return -1;
	}
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
	*out_len = 17;
	return 0;
}

static int hls_pass4_gmac_build(osp_sec_context_t *ctx, uint8_t *out, uint32_t out_size, uint32_t *out_len) {
	if (!ctx || !out || out_size < 17 || !out_len) {
		return -1;
	}
	/* Check for IC overflow */
	if (ctx->invocation_counter == 0xFFFFFFFF) {
		return -1;
	}
	uint8_t sc = osp_sec_control_byte(OSP_POLICY_AUTH_ONLY, ctx->suite);
	uint32_t ic = ctx->invocation_counter++;
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
	*out_len = 17;
	return 0;
}

int osp_hls_pass3_build(const osp_sec_context_t *ctx, uint8_t *out, uint32_t out_size, uint32_t *out_len) {
	if (!ctx || !out || !out_len) {
		return -1;
	}
	if (ctx->mechanism == OSP_MECH_HLS_GMAC) {
		return hls_pass3_gmac_build(ctx, out, out_size, out_len);
	}
	if (ctx->mechanism == OSP_MECH_HLS_GOST_CMAC) {
		return hls_pass3_gost_cmac_build(ctx, out, out_size, out_len);
	}
	if (osp_hls_uses_signature(ctx->mechanism)) {
		return hls_pass3_sig_build(ctx, out, out_size, out_len);
	}
	if (ctx->mechanism == OSP_MECH_HLS_MD5 || ctx->mechanism == OSP_MECH_HLS_SHA1) {
		return hls_hash_legacy(ctx, ctx->stoc, ctx->stoc_len, out, out_len);
	}
	if (ctx->mechanism == OSP_MECH_HLS_SHA256 || ctx->mechanism == OSP_MECH_HLS_GOST_STREEBOG) {
		return hls_hash_with_titles(ctx, ctx->system_title, ctx->peer_system_title, ctx->stoc, ctx->stoc_len, ctx->ctos, ctx->ctos_len,
		                            out, out_len);
	}
	return -1;
}

int osp_hls_pass3_verify(osp_sec_context_t *ctx, const uint8_t *f_stoc, uint32_t len) {
	if (!ctx || !f_stoc || len == 0) {
		return -1;
	}

	if (ctx->mechanism == OSP_MECH_HLS_GMAC) {
		if (len < 17) {
			return -1;
		}
		if (ctx->hls_failures >= 5) {
			return -2;
		}
		uint32_t ic = ((uint32_t)f_stoc[1] << 24) | ((uint32_t)f_stoc[2] << 16) | ((uint32_t)f_stoc[3] << 8) | (uint32_t)f_stoc[4];
		if (ctx->ic_valid && ic <= ctx->last_peer_ic) {
			ctx->hls_failures++;
			return -3;
		}
		uint8_t expected_tag[OSP_SEC_TAG_SIZE];
		if (osp_hls_gmac(ctx, ctx->system_title, ic, ctx->stoc, ctx->stoc_len, expected_tag) != 0) {
			ctx->hls_failures++;
			return -4;
		}
		uint8_t diff = 0;
		for (uint32_t i = 0; i < OSP_SEC_TAG_SIZE; i++) {
			diff |= f_stoc[5 + i] ^ expected_tag[i];
		}
		if (diff != 0) {
			ctx->hls_failures++;
			return -5;
		}
		ctx->last_peer_ic = ic;
		ctx->ic_valid = true;
		ctx->hls_failures = 0;
		return 0;
	}

	uint8_t expected[32];
	uint32_t expected_len = 0;
	if (ctx->mechanism == OSP_MECH_HLS_GOST_CMAC) {
		if (len < 21) {
			return -1;
		}
		if (ctx->hls_failures >= 5) {
			return -2;
		}
		uint32_t ic = ((uint32_t)f_stoc[1] << 24) | ((uint32_t)f_stoc[2] << 16) | ((uint32_t)f_stoc[3] << 8) | (uint32_t)f_stoc[4];
		if (ctx->ic_valid && ic <= ctx->last_peer_ic) {
			ctx->hls_failures++;
			return -3;
		}
		uint8_t iv[12];
		memcpy(iv, ctx->peer_system_title, 8);
		iv[8] = (uint8_t)(ic >> 24);
		iv[9] = (uint8_t)(ic >> 16);
		iv[10] = (uint8_t)(ic >> 8);
		iv[11] = (uint8_t)(ic);
		uint8_t mac[OSP_SEC_GOST_CMAC_SIZE];
		if (osp_hls_gost_cmac(ctx->k_em, iv, f_stoc[0], ctx->stoc, ctx->stoc_len, ctx->ctos, ctx->ctos_len, mac) != 0) {
			ctx->hls_failures++;
			return -4;
		}
		uint8_t diff = 0;
		for (uint32_t i = 0; i < OSP_SEC_GOST_CMAC_SIZE; i++) {
			diff |= f_stoc[5 + i] ^ mac[i];
		}
		if (diff != 0) {
			ctx->hls_failures++;
			return -5;
		}
		ctx->last_peer_ic = ic;
		ctx->ic_valid = true;
		ctx->hls_failures = 0;
		return 0;
	}
	if (osp_hls_uses_signature(ctx->mechanism)) {
		return hls_pass3_sig_verify(ctx, f_stoc, len);
	}
	if (ctx->mechanism == OSP_MECH_HLS_SHA256 || ctx->mechanism == OSP_MECH_HLS_GOST_STREEBOG) {
		if (hls_hash_with_titles(ctx, ctx->peer_system_title, ctx->system_title, ctx->stoc, ctx->stoc_len, ctx->ctos, ctx->ctos_len,
		                         expected, &expected_len) != 0) {
			return -1;
		}
	} else if (ctx->mechanism == OSP_MECH_HLS_MD5 || ctx->mechanism == OSP_MECH_HLS_SHA1) {
		if (hls_hash_legacy(ctx, ctx->stoc, ctx->stoc_len, expected, &expected_len) != 0) {
			return -1;
		}
	} else {
		return -1;
	}
	if (len != expected_len) {
		return -5;
	}
	uint8_t diff = 0;
	for (uint32_t i = 0; i < expected_len; i++) {
		diff |= f_stoc[i] ^ expected[i];
	}
	return (diff == 0) ? 0 : -5;
}

int osp_hls_pass4_build(osp_sec_context_t *ctx, uint8_t *out, uint32_t out_size, uint32_t *out_len) {
	if (!ctx || !out || !out_len) {
		return -1;
	}
	if (ctx->mechanism == OSP_MECH_HLS_GMAC) {
		return hls_pass4_gmac_build(ctx, out, out_size, out_len);
	}
	if (ctx->mechanism == OSP_MECH_HLS_GOST_CMAC) {
		return hls_pass4_gost_cmac_build(ctx, out, out_size, out_len);
	}
	if (osp_hls_uses_signature(ctx->mechanism)) {
		return hls_pass4_sig_build(ctx, out, out_size, out_len);
	}
	if (ctx->mechanism == OSP_MECH_HLS_MD5 || ctx->mechanism == OSP_MECH_HLS_SHA1) {
		return hls_hash_legacy(ctx, ctx->ctos, ctx->ctos_len, out, out_len);
	}
	if (ctx->mechanism == OSP_MECH_HLS_SHA256 || ctx->mechanism == OSP_MECH_HLS_GOST_STREEBOG) {
		return hls_hash_with_titles(ctx, ctx->system_title, ctx->peer_system_title, ctx->ctos, ctx->ctos_len, ctx->stoc, ctx->stoc_len,
		                            out, out_len);
	}
	return -1;
}

int osp_hls_pass4_verify(osp_sec_context_t *ctx, const uint8_t *f_ctos, uint32_t len) {
	if (!ctx || !f_ctos || len == 0) {
		return -1;
	}

	if (ctx->mechanism == OSP_MECH_HLS_GMAC) {
		if (len < 17) {
			return -1;
		}
		uint32_t ic = ((uint32_t)f_ctos[1] << 24) | ((uint32_t)f_ctos[2] << 16) | ((uint32_t)f_ctos[3] << 8) | (uint32_t)f_ctos[4];
		uint8_t expected_tag[OSP_SEC_TAG_SIZE];
		if (osp_hls_gmac(ctx, ctx->system_title, ic, ctx->ctos, ctx->ctos_len, expected_tag) != 0) {
			return -1;
		}
		uint8_t diff = 0;
		for (uint32_t i = 0; i < OSP_SEC_TAG_SIZE; i++) {
			diff |= f_ctos[5 + i] ^ expected_tag[i];
		}
		return (diff == 0) ? 0 : -1;
	}

	uint8_t expected[32];
	uint32_t expected_len = 0;
	if (ctx->mechanism == OSP_MECH_HLS_GOST_CMAC) {
		if (len < 21) {
			return -1;
		}
		uint32_t ic = ((uint32_t)f_ctos[1] << 24) | ((uint32_t)f_ctos[2] << 16) | ((uint32_t)f_ctos[3] << 8) | (uint32_t)f_ctos[4];
		uint8_t iv[12];
		memcpy(iv, ctx->peer_system_title, 8);
		iv[8] = (uint8_t)(ic >> 24);
		iv[9] = (uint8_t)(ic >> 16);
		iv[10] = (uint8_t)(ic >> 8);
		iv[11] = (uint8_t)(ic);
		uint8_t mac[OSP_SEC_GOST_CMAC_SIZE];
		if (osp_hls_gost_cmac(ctx->k_em, iv, f_ctos[0], ctx->ctos, ctx->ctos_len, ctx->stoc, ctx->stoc_len, mac) != 0) {
			return -1;
		}
		uint8_t diff = 0;
		for (uint32_t i = 0; i < OSP_SEC_GOST_CMAC_SIZE; i++) {
			diff |= f_ctos[5 + i] ^ mac[i];
		}
		return (diff == 0) ? 0 : -1;
	}
	if (osp_hls_uses_signature(ctx->mechanism)) {
		return hls_pass4_sig_verify(ctx, f_ctos, len);
	}
	if (ctx->mechanism == OSP_MECH_HLS_SHA256 || ctx->mechanism == OSP_MECH_HLS_GOST_STREEBOG) {
		if (hls_hash_with_titles(ctx, ctx->peer_system_title, ctx->system_title, ctx->ctos, ctx->ctos_len, ctx->stoc, ctx->stoc_len,
		                         expected, &expected_len) != 0) {
			return -1;
		}
	} else if (hls_hash_legacy(ctx, ctx->ctos, ctx->ctos_len, expected, &expected_len) != 0) {
		return -1;
	}
	if (len != expected_len) {
		return -1;
	}
	uint8_t diff = 0;
	for (uint32_t i = 0; i < expected_len; i++) {
		diff |= f_ctos[i] ^ expected[i];
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

bool osp_ded_is_ciphered_tag(uint8_t tag) {
	switch (tag) {
		case OSP_DED_GET_REQUEST:
		case OSP_DED_SET_REQUEST:
		case OSP_DED_ACTION_REQUEST:
		case OSP_DED_GET_RESPONSE:
		case OSP_DED_SET_RESPONSE:
		case OSP_DED_ACTION_RESPONSE:
			return true;
		default:
			return false;
	}
}

uint8_t osp_ded_tag_for_plain(uint8_t plain_tag) {
	if (plain_tag >= 0xC0 && plain_tag <= 0xC7) {
		return (uint8_t)(plain_tag + 0x10);
	}
	return plain_tag;
}

uint8_t osp_ded_plain_tag_for_ciphered(uint8_t ciphered_tag) {
	if (ciphered_tag >= 0xD0 && ciphered_tag <= 0xD7) {
		return (uint8_t)(ciphered_tag - 0x10);
	}
	return ciphered_tag;
}

bool osp_svc_is_ciphered_tag(uint8_t tag) {
	return osp_glo_is_ciphered_tag(tag) || osp_ded_is_ciphered_tag(tag);
}

uint8_t osp_svc_cipher_tag_for_plain(const osp_sec_context_t *ctx, uint8_t plain_tag) {
	if (ctx && ctx->use_dedicated_key) {
		return osp_ded_tag_for_plain(plain_tag);
	}
	return osp_glo_tag_for_plain(plain_tag);
}

int osp_sec_cipher_apply_dedicated_key(osp_sec_context_t *ctx, const uint8_t *key, uint8_t key_len) {
	if (!ctx || !key || key_len == 0 || key_len > OSP_SEC_KEY_MAX) {
		return -1;
	}
	memcpy(ctx->dedicated_key, key, key_len);
	ctx->dedicated_key_len = key_len;
	ctx->use_dedicated_key = true;
	return 0;
}

void osp_sec_cipher_session_use_dedicated(osp_sec_context_t *tx, osp_sec_context_t *rx, const uint8_t *key, uint8_t key_len) {
	if (tx) {
		osp_sec_cipher_apply_dedicated_key(tx, key, key_len);
	}
	if (rx) {
		osp_sec_cipher_apply_dedicated_key(rx, key, key_len);
	}
}

static const uint8_t *sec_aes_encryption_key(const osp_sec_context_t *ctx, uint32_t *key_len_out) {
	if (ctx->use_dedicated_key) {
		*key_len_out = ctx->dedicated_key_len;
		return ctx->dedicated_key;
	}
	*key_len_out = osp_sec_suite_aes_key_len(ctx->suite);
	return ctx->guek;
}

static void sec_gost_k_em_for_cipher(const osp_sec_context_t *ctx, uint8_t k_em[OSP_SEC_K_EM_SIZE]) {
	memcpy(k_em, ctx->k_em, OSP_SEC_K_EM_SIZE);
	if (!ctx->use_dedicated_key) {
		return;
	}
	uint32_t copy = ctx->dedicated_key_len > 32 ? 32 : ctx->dedicated_key_len;
	memcpy(k_em, ctx->dedicated_key, copy);
	if (copy < 32) {
		memset(k_em + copy, 0, 32 - copy);
	}
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
	if (!ctx || !plaintext || !out || !out_len || plain_len > OSP_GLO_MAX_PLAIN) {
		return -1;
	}
	if (!osp_sec_uses_gost_kem(ctx->suite) && !osp_hal_gcm_crypt) {
		return -1;
	}

	uint8_t sc = osp_sec_control_byte(ctx->policy, ctx->suite);
	uint32_t ic = ctx->invocation_counter;
	bool auth = (sc & 0x10u) != 0;
	bool encr = (sc & 0x20u) != 0;

	uint8_t iv[12];
	glo_build_iv(ctx, ic, iv);

	uint32_t aes_key_len;
	const uint8_t *aes_key = sec_aes_encryption_key(ctx, &aes_key_len);
	uint8_t gost_k_em[OSP_SEC_K_EM_SIZE];

	uint8_t protected_part[OSP_GLO_MAX_PLAIN + OSP_SEC_TAG_SIZE];
	uint32_t protected_len = 0;
	uint8_t tag[OSP_SEC_TAG_SIZE];

	if (osp_sec_uses_gost_kem(ctx->suite)) {
		sec_gost_k_em_for_cipher(ctx, gost_k_em);
		uint32_t body_len = 0;
		if (osp_gost_kuzn_aead_encrypt(gost_k_em, iv, sc, NULL, 0, plaintext, plain_len, auth, encr, protected_part, &body_len,
		                               auth ? tag : NULL) != 0) {
			return -1;
		}
		if (auth) {
			memcpy(&protected_part[body_len], tag, OSP_SEC_TAG_SIZE);
			protected_len = body_len + OSP_SEC_TAG_SIZE;
		} else {
			protected_len = body_len;
		}
	} else if (auth && encr) {
		uint8_t aad[17];
		aad[0] = sc;
		memcpy(&aad[1], ctx->gak, 16);
		if (osp_hal_gcm_crypt(OSP_GCM_ENCRYPT, aes_key, aes_key_len, iv, aad, 17, plaintext, plain_len, protected_part, NULL, tag) != 0) {
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
		if (osp_hal_gcm_crypt(OSP_GCM_ENCRYPT, aes_key, aes_key_len, iv, aad, 17 + plain_len, NULL, 0, NULL, NULL, tag) != 0) {
			return -1;
		}
		memcpy(protected_part, plaintext, plain_len);
		memcpy(&protected_part[plain_len], tag, OSP_SEC_TAG_SIZE);
		protected_len = plain_len + OSP_SEC_TAG_SIZE;
	} else if (!auth && encr) {
		if (osp_hal_gcm_crypt(OSP_GCM_ENCRYPT, aes_key, aes_key_len, iv, NULL, 0, plaintext, plain_len, protected_part, NULL, NULL) != 0) {
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
	if (!ctx || !ciphered || ciphered_len < 7 || !plaintext || !plain_len) {
		return -1;
	}
	if (!osp_sec_uses_gost_kem(ctx->suite) && !osp_hal_gcm_crypt) {
		return -1;
	}
	if (!osp_svc_is_ciphered_tag(ciphered[0])) {
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
	uint8_t iv[12];
	glo_build_iv(ctx, ic, iv);

	uint32_t aes_key_len;
	const uint8_t *aes_key = sec_aes_encryption_key(ctx, &aes_key_len);
	uint8_t gost_k_em[OSP_SEC_K_EM_SIZE];

	if (osp_sec_uses_gost_kem(ctx->suite)) {
		sec_gost_k_em_for_cipher(ctx, gost_k_em);
		if (auth && encr) {
			if (protected_len < OSP_SEC_TAG_SIZE) {
				return -1;
			}
			uint32_t ct_len = protected_len - OSP_SEC_TAG_SIZE;
			if (ct_len > OSP_GLO_MAX_PLAIN) {
				return -1;
			}
			int rc = osp_gost_kuzn_aead_decrypt(gost_k_em, iv, sc, NULL, 0, protected_part, ct_len, true, true, plaintext,
			                                      plain_len, &protected_part[ct_len]);
			if (rc == -2) {
				return -3;
			}
			if (rc != 0) {
				return -1;
			}
		} else if (auth && !encr) {
			if (protected_len < OSP_SEC_TAG_SIZE) {
				return -1;
			}
			uint32_t plen = protected_len - OSP_SEC_TAG_SIZE;
			if (plen > OSP_GLO_MAX_PLAIN) {
				return -1;
			}
			int rc = osp_gost_kuzn_aead_decrypt(gost_k_em, iv, sc, NULL, 0, protected_part, plen, true, false, plaintext,
			                                      plain_len, &protected_part[plen]);
			if (rc == -2) {
				return -3;
			}
			if (rc != 0) {
				return -1;
			}
		} else if (!auth && encr) {
			if (protected_len > OSP_GLO_MAX_PLAIN) {
				return -1;
			}
			if (osp_gost_kuzn_aead_decrypt(gost_k_em, iv, sc, NULL, 0, protected_part, protected_len, false, true, plaintext,
			                               plain_len, NULL) != 0) {
				return -3;
			}
		} else {
			if (protected_len > OSP_GLO_MAX_PLAIN) {
				return -1;
			}
			memcpy(plaintext, protected_part, protected_len);
			*plain_len = protected_len;
		}
	} else if (auth && encr) {
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
		if (osp_hal_gcm_crypt(OSP_GCM_DECRYPT, aes_key, aes_key_len, iv, aad, 17, protected_part, ct_len, plaintext, &protected_part[ct_len], NULL) != 0) {
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
		if (osp_hal_gcm_crypt(OSP_GCM_DECRYPT, aes_key, aes_key_len, iv, aad, 17 + plen, NULL, 0, NULL, &protected_part[plen], NULL) != 0) {
			return -3;
		}
		memcpy(plaintext, protected_part, plen);
		*plain_len = plen;
	} else if (!auth && encr) {
		if (protected_len > OSP_GLO_MAX_PLAIN) {
			return -1;
		}
		if (osp_hal_gcm_crypt(OSP_GCM_DECRYPT, aes_key, aes_key_len, iv, NULL, 0, protected_part, protected_len, plaintext, NULL, NULL) != 0) {
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
