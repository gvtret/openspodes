#ifndef OSP_SECURITY_H
#define OSP_SECURITY_H

#include "../openspodes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_SEC_KEY_MAX           32
#define OSP_SEC_SYSTEM_TITLE_SIZE 8
#define OSP_SEC_CHALLENGE_MAX     64
#define OSP_SEC_TAG_SIZE          12

typedef enum {
	OSP_SEC_GUEK = 0,
	OSP_SEC_GAK = 1,
	OSP_SEC_GBEK = 2,
	OSP_SEC_KEK = 3,
} osp_sec_key_id;

extern void (*osp_hal_gcm_init)(osp_sec_key_id key_id, uint32_t key_len, const uint8_t *iv, uint32_t iv_len, const uint8_t *aad, uint32_t aad_len);
extern void (*osp_hal_gcm_update)(const uint8_t *in, uint32_t len, uint8_t *out);
extern void (*osp_hal_gcm_finish)(uint8_t *tag);
extern void (*osp_hal_md5)(const uint8_t *input, uint32_t len, uint8_t *output);
extern void (*osp_hal_sha1)(const uint8_t *input, uint32_t len, uint8_t *output);
extern void (*osp_hal_sha256)(const uint8_t *input, uint32_t len, uint8_t *output);

typedef enum {
	OSP_SUITE_0 = 0, /* AES-GCM-128 */
	OSP_SUITE_1 = 1, /* ECDH-ECDSA-AES-GCM-128-SHA-256 */
	OSP_SUITE_2 = 2, /* ECDH-ECDSA-AES-GCM-256-SHA-384 */
	OSP_SUITE_8 = 8, /* Kuznyechik (GOST) */
	OSP_SUITE_9 = 9, /* Kuznyechik + VKO + Streebog (GOST) */
} osp_sec_suite_t;

static inline uint8_t osp_sec_suite_id(osp_sec_suite_t s) {
	return (uint8_t)s;
}

static inline osp_sec_suite_t osp_sec_suite_from_id(uint8_t id) {
	return (osp_sec_suite_t)id;
}

static inline uint8_t osp_sec_suite_aes_key_len(osp_sec_suite_t s) {
	return (s == OSP_SUITE_2) ? 32 : 16;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Security Policy (protection level per APDU)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
	OSP_POLICY_NONE = 0,
	OSP_POLICY_AUTH_ONLY = 1,
	OSP_POLICY_ENCR_ONLY = 2,
	OSP_POLICY_ENCR_AUTH = 3,
} osp_sec_policy_t;

/* Security control byte layout:
 *   bits 0-3: suite id
 *   bit 4:    A (authentication)
 *   bit 5:    E (encryption)
 *   bit 6:    key set (0=unicast, 1=broadcast)
 *   bit 7:    compression */

typedef union {
	uint8_t raw;

	struct {
		uint8_t suite_id:4;
		uint8_t auth    :1;
		uint8_t encr    :1;
		uint8_t key_set :1;
		uint8_t compress:1;
	} bits;
} osp_sec_control_t;

static inline uint8_t osp_sec_control_byte(osp_sec_policy_t policy, osp_sec_suite_t suite) {
	uint8_t sc = osp_sec_suite_id(suite);
	switch (policy) {
		case OSP_POLICY_AUTH_ONLY:
			sc |= 0x10;
			break;
		case OSP_POLICY_ENCR_ONLY:
			sc |= 0x20;
			break;
		case OSP_POLICY_ENCR_AUTH:
			sc |= 0x30;
			break;
		default:
			break;
	}
	return sc;
}

static inline osp_sec_policy_t osp_sec_control_to_policy(uint8_t sc) {
	if (sc & 0x10) {
		return (sc & 0x20) ? OSP_POLICY_ENCR_AUTH : OSP_POLICY_AUTH_ONLY;
	}
	return (sc & 0x20) ? OSP_POLICY_ENCR_ONLY : OSP_POLICY_NONE;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Authentication Mechanism
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
	OSP_MECH_LOWEST = 0,
	OSP_MECH_LLS = 1,
	OSP_MECH_HLS = 2,
	OSP_MECH_HLS_MD5 = 3,
	OSP_MECH_HLS_SHA1 = 4,
	OSP_MECH_HLS_GMAC = 5,
} osp_auth_mechanism_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Security Context (per-association)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define OSP_SEC_KEY_MAX       32
#define OSP_SEC_CHALLENGE_MAX 64
#define OSP_SEC_TAG_SIZE      12

typedef struct {
	osp_sec_suite_t suite;
	osp_sec_policy_t policy;
	osp_auth_mechanism_t mechanism;
	uint8_t system_title[OSP_SEC_SYSTEM_TITLE_SIZE];
	uint32_t invocation_counter; /* IC, monotonically increasing */
	uint32_t last_peer_ic;       /* last received IC from peer */
	bool ic_valid;               /* true after first successful auth */

	/* Keys (per-association, from HAL) */
	uint8_t guek[OSP_SEC_KEY_MAX]; /* Global Unicast Encryption Key */
	uint8_t gak[OSP_SEC_KEY_MAX];  /* Global Authentication Key */
	uint8_t gbek[OSP_SEC_KEY_MAX]; /* Global Broadcast Encryption Key */

	/* Challenge values */
	uint8_t ctos[OSP_SEC_CHALLENGE_MAX]; /* Client-to-Server */
	uint8_t ctos_len;
	uint8_t stoc[OSP_SEC_CHALLENGE_MAX]; /* Server-to-Client */
	uint8_t stoc_len;

	/* Rate limiting */
	uint8_t hls_failures;
} osp_sec_context_t;

void osp_sec_context_init(osp_sec_context_t *ctx, osp_sec_suite_t suite, osp_auth_mechanism_t mech, const uint8_t *system_title);

/* ═══════════════════════════════════════════════════════════════════════════
 *  HLS 4-pass handshake
 * ═══════════════════════════════════════════════════════════════════════════ */

/* HLS hash functions (Green Book §9.2.7.4) */
int osp_hls_md5(const uint8_t *input, uint32_t len, uint8_t output[16]);
int osp_hls_sha1(const uint8_t *input, uint32_t len, uint8_t output[20]);
int osp_hls_sha256(const uint8_t *input, uint32_t len, uint8_t output[32]);

/* GMAC computation using HAL crypto */
int osp_hls_gmac(const osp_sec_context_t *ctx, const uint8_t *system_title, uint32_t ic, const uint8_t *data, uint32_t data_len, uint8_t tag[OSP_SEC_TAG_SIZE]);

/* Pass 3: build f(StoC) = SC || IC || GMAC(SC || AK || StoC) */
int osp_hls_pass3_build(const osp_sec_context_t *ctx, uint8_t *out, uint32_t out_size);

/* Pass 3: verify f(StoC) received from client */
int osp_hls_pass3_verify(osp_sec_context_t *ctx, const uint8_t *f_stoc, uint32_t len);

/* Pass 4: build f(CtoS) = SC || IC || GMAC(SC || AK || CtoS) */
int osp_hls_pass4_build(osp_sec_context_t *ctx, uint8_t *out, uint32_t out_size);

/* Pass 4: verify f(CtoS) received from server */
int osp_hls_pass4_verify(osp_sec_context_t *ctx, const uint8_t *f_ctos, uint32_t len);

/* ═══════════════════════════════════════════════════════════════════════════
 *  Glo-ciphering (global ciphered APDUs)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Protect a plaintext APDU with glo-ciphering (auth + optional encrypt) */
int osp_glo_protect(const osp_sec_context_t *ctx, uint8_t ciphered_tag, const uint8_t *plaintext, uint32_t plain_len, uint8_t *out, uint32_t *out_len);

/* Unprotect a glo-ciphered APDU */
int osp_glo_unprotect(osp_sec_context_t *ctx, const uint8_t *ciphered, uint32_t ciphered_len, uint8_t *plaintext, uint32_t *plain_len);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SECURITY_H */
