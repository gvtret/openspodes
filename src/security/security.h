/**
 * @file security.h
 * @brief DLMS/COSEM security layer (IEC 62056-5-3 Green Book).
 *
 * Provides:
 * - Security context management (key storage, suite configuration)
 * - HLS 4-pass authentication (mechanisms 0-10)
 * - Glo/ded ciphering (APDU-level encryption + authentication)
 * - General ciphering APDUs (0xDB/0xDC/0xDD tags)
 *
 * HAL crypto functions must be set before use:
 *   osp_hal_gcm_init, osp_hal_gcm_update, osp_hal_gcm_finish
 *   osp_hal_md5, osp_hal_sha1, osp_hal_sha256
 *   osp_hal_random_fill (for random challenge generation)
 */

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

typedef enum {
	OSP_GCM_ENCRYPT = 0,
	OSP_GCM_DECRYPT = 1,
} osp_gcm_dir_t;

/* One-shot AES-GCM: encrypt writes tag_out; decrypt verifies tag_in. */
extern int (*osp_hal_gcm_crypt)(osp_gcm_dir_t dir, const uint8_t *key, uint32_t key_len, const uint8_t iv[12], const uint8_t *aad,
                                uint32_t aad_len, const uint8_t *in, uint32_t in_len, uint8_t *out, const uint8_t tag_in[OSP_SEC_TAG_SIZE],
                                uint8_t tag_out[OSP_SEC_TAG_SIZE]);
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

extern void (*osp_hal_streebog256)(const uint8_t *input, uint32_t len, uint8_t output[32]);
extern int (*osp_hal_ecdsa_sign)(osp_sec_suite_t suite, const uint8_t *sk, uint32_t sk_len, const uint8_t *msg, uint32_t msg_len,
                                 uint8_t *sig, uint32_t *sig_len);
extern int (*osp_hal_ecdsa_verify)(osp_sec_suite_t suite, const uint8_t *pk, uint32_t pk_len, const uint8_t *msg, uint32_t msg_len,
                                   const uint8_t *sig, uint32_t sig_len);

/* Random number generator: fills buf with len cryptographically random bytes.
 * Returns 0 on success, negative on failure. May be NULL if not provided by HAL. */
extern int (*osp_hal_random_fill)(uint8_t *buf, uint32_t len);

static inline uint8_t osp_sec_suite_id(osp_sec_suite_t s) {
	return (uint8_t)s;
}

static inline osp_sec_suite_t osp_sec_suite_from_id(uint8_t id) {
	return (osp_sec_suite_t)id;
}

static inline uint8_t osp_sec_suite_aes_key_len(osp_sec_suite_t s) {
	return (s == OSP_SUITE_2) ? 32 : 16;
}

static inline bool osp_sec_uses_gost_kem(osp_sec_suite_t s) {
	return s == OSP_SUITE_8 || s == OSP_SUITE_9;
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
	OSP_MECH_HLS_SHA256 = 6,
	OSP_MECH_HLS_ECDSA = 7,
	OSP_MECH_HLS_GOST_CMAC = 8,
	OSP_MECH_HLS_GOST_STREEBOG = 9,
	OSP_MECH_HLS_GOST_SIG = 10,
} osp_auth_mechanism_t;

static inline bool osp_hls_requires_handshake(osp_auth_mechanism_t mech) {
	return mech >= OSP_MECH_HLS && mech <= OSP_MECH_HLS_GOST_SIG;
}

static inline bool osp_hls_uses_signature(osp_auth_mechanism_t mech) {
	return mech == OSP_MECH_HLS_ECDSA || mech == OSP_MECH_HLS_GOST_SIG;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Security Context (per-association)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define OSP_SEC_K_EM_SIZE         64
#define OSP_SEC_SIGN_KEY_MAX      48
#define OSP_SEC_PUBKEY_MAX        96
#define OSP_SEC_GOST_CMAC_SIZE    16
#define OSP_SEC_ECDSA_SIG_MAX     96
#define OSP_SEC_HLS_AUTH_MAX      OSP_SEC_ECDSA_SIG_MAX

typedef struct {
	osp_sec_suite_t suite;
	osp_sec_policy_t policy;
	osp_auth_mechanism_t mechanism;
	uint8_t system_title[OSP_SEC_SYSTEM_TITLE_SIZE];
	uint8_t peer_system_title[OSP_SEC_SYSTEM_TITLE_SIZE];
	uint32_t invocation_counter; /* IC, monotonically increasing */
	uint32_t last_peer_ic;       /* last received IC from peer */
	bool ic_valid;               /* true after first successful auth */

	/* Keys (per-association, from HAL) */
	uint8_t guek[OSP_SEC_KEY_MAX]; /* Global Unicast Encryption Key */
	uint8_t gak[OSP_SEC_KEY_MAX];  /* Global Authentication Key */
	uint8_t gbek[OSP_SEC_KEY_MAX]; /* Global Broadcast Encryption Key */
	uint8_t k_em[OSP_SEC_K_EM_SIZE]; /* GOST K_EM (512 bits) for mech 8 */

	/* Dedicated encryption key (DEK) from InitiateRequest — replaces GUEK / MSB256(K_EM) */
	bool use_dedicated_key;
	uint8_t dedicated_key[OSP_SEC_KEY_MAX];
	uint8_t dedicated_key_len;

	uint8_t signing_key[OSP_SEC_SIGN_KEY_MAX];
	uint8_t peer_public_key[OSP_SEC_PUBKEY_MAX];
	uint8_t signing_key_len;
	uint8_t peer_public_key_len;

	/* Challenge values */
	uint8_t ctos[OSP_SEC_CHALLENGE_MAX]; /* Client-to-Server */
	uint8_t ctos_len;
	uint8_t stoc[OSP_SEC_CHALLENGE_MAX]; /* Server-to-Client */
	uint8_t stoc_len;

	/* Rate limiting */
	uint8_t hls_failures;
} osp_sec_context_t;
/**
 * @brief Initialize a security context with the given suite and mechanism.
 * @param ctx          Security context to initialize (caller-owned).
 * @param suite        Security suite (OSP_SUITE_0 for AES-GCM, OSP_SUITE_8/9 for GOST).
 * @param mech         Authentication mechanism (OSP_MECH_HLS_GMAC, etc.).
 * @param system_title 8-byte system title (may be NULL, zeroed if so).
 */
void osp_sec_context_init(osp_sec_context_t *ctx, osp_sec_suite_t suite, osp_auth_mechanism_t mech, const uint8_t *system_title);

/**
 * @brief Destroy security context, zeroizing all key material.
 * @param ctx Security context to destroy. Safe to call on NULL.
 */
void osp_sec_context_destroy(osp_sec_context_t *ctx);

/* ═══════════════════════════════════════════════════════════════════════════
 *  HLS 4-pass handshake
 * ═══════════════════════════════════════════════════════════════════════════ */

/* HLS hash functions (Green Book §9.2.7.4) */
/** @brief Compute MD5 hash of input data. */
int osp_hls_md5(const uint8_t *input, uint32_t len, uint8_t output[16]);
/** @brief Compute SHA-1 hash of input data. */
int osp_hls_sha1(const uint8_t *input, uint32_t len, uint8_t output[20]);
/** @brief Compute SHA-256 hash of input data. */
int osp_hls_sha256(const uint8_t *input, uint32_t len, uint8_t output[32]);

/** @brief Compute AES-GMAC authentication tag over data. */
int osp_hls_gmac(const osp_sec_context_t *ctx, const uint8_t *system_title, uint32_t ic, const uint8_t *data, uint32_t data_len, uint8_t tag[OSP_SEC_TAG_SIZE]);

/** @brief Build HLS pass-3 client response f(StoC). */
int osp_hls_pass3_build(const osp_sec_context_t *ctx, uint8_t *out, uint32_t out_size, uint32_t *out_len);

/** @brief Verify HLS pass-3 client response f(StoC). */
int osp_hls_pass3_verify(osp_sec_context_t *ctx, const uint8_t *f_stoc, uint32_t len);

/** @brief Build HLS pass-4 server response f(CtoS). */
int osp_hls_pass4_build(osp_sec_context_t *ctx, uint8_t *out, uint32_t out_size, uint32_t *out_len);

/** @brief Verify HLS pass-4 server response f(CtoS). */
int osp_hls_pass4_verify(osp_sec_context_t *ctx, const uint8_t *f_ctos, uint32_t len);

/* ═══════════════════════════════════════════════════════════════════════════
 *  Glo-ciphering (global ciphered APDUs)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define OSP_GLO_GET_REQUEST      0xC8
#define OSP_GLO_SET_REQUEST      0xC9
#define OSP_GLO_ACTION_REQUEST   0xCB
#define OSP_GLO_GET_RESPONSE     0xCC
#define OSP_GLO_SET_RESPONSE     0xCD
#define OSP_GLO_ACTION_RESPONSE  0xCF
#define OSP_GLO_INITIATE_REQUEST  0x21
#define OSP_GLO_INITIATE_RESPONSE 0x28

#define OSP_DED_GET_REQUEST      0xD0
#define OSP_DED_SET_REQUEST      0xD1
#define OSP_DED_ACTION_REQUEST   0xD3
#define OSP_DED_GET_RESPONSE     0xD4
#define OSP_DED_SET_RESPONSE     0xD5
#define OSP_DED_ACTION_RESPONSE  0xD7

#define OSP_GLO_MAX_PLAIN    1024
#define OSP_GLO_MAX_CIPHERED (OSP_GLO_MAX_PLAIN + 32)

/** @brief Check whether a tag represents a glo-ciphered APDU. */
bool osp_glo_is_ciphered_tag(uint8_t tag);
/** @brief Map a plain APDU tag to its glo-ciphered counterpart. */
uint8_t osp_glo_tag_for_plain(uint8_t plain_tag);
/** @brief Map a glo-ciphered tag back to the original plain tag. */
uint8_t osp_glo_plain_tag_for_ciphered(uint8_t ciphered_tag);

/** @brief Check whether a tag represents a dedicated-ciphered APDU. */
bool osp_ded_is_ciphered_tag(uint8_t tag);
/** @brief Map a plain APDU tag to its dedicated-ciphered counterpart. */
uint8_t osp_ded_tag_for_plain(uint8_t plain_tag);
/** @brief Map a dedicated-ciphered tag back to the original plain tag. */
uint8_t osp_ded_plain_tag_for_ciphered(uint8_t ciphered_tag);

/** @brief Check whether a tag represents a service-ciphered APDU. */
bool osp_svc_is_ciphered_tag(uint8_t tag);
/** @brief Map a plain tag to its service-ciphered counterpart for the given suite. */
uint8_t osp_svc_cipher_tag_for_plain(const osp_sec_context_t *ctx, uint8_t plain_tag);

/** @brief Apply a dedicated encryption key (DEK) to the security context. */
int osp_sec_cipher_apply_dedicated_key(osp_sec_context_t *ctx, const uint8_t *key, uint8_t key_len);
/** @brief Enable dedicated key mode on both tx and rx cipher contexts. */
void osp_sec_cipher_session_use_dedicated(osp_sec_context_t *tx, osp_sec_context_t *rx, const uint8_t *key, uint8_t key_len);

/**
 * @brief Protect a plaintext APDU with glo-ciphering (auth + optional encrypt).
 * @param ctx         Security context with keys and invocation counter.
 * @param ciphered_tag Expected glo tag (e.g., OSP_GLO_GET_REQUEST).
 * @param plaintext   Input APDU to protect.
 * @param plain_len   Length of plaintext.
 * @param out         Output buffer for ciphered APDU.
 * @param out_len     In/Out: on input, size of out buffer; on output, actual written length.
 * @return 0 on success, negative on failure.
 */
int osp_glo_protect(const osp_sec_context_t *ctx, uint8_t ciphered_tag, const uint8_t *plaintext, uint32_t plain_len, uint8_t *out, uint32_t *out_len);

/**
 * @brief Unprotect a glo-ciphered APDU (verify auth tag, decrypt if needed).
 * @param ctx         Security context with keys and invocation counter.
 * @param ciphered    Input ciphered APDU.
 * @param ciphered_len Length of ciphered APDU.
 * @param plaintext   Output buffer for decrypted APDU.
 * @param plain_len   In/Out: on input, size of plaintext buffer; on output, actual written length.
 * @return 0 on success, negative on auth failure or invalid input.
 */
int osp_glo_unprotect(osp_sec_context_t *ctx, const uint8_t *ciphered, uint32_t ciphered_len, uint8_t *plaintext, uint32_t *plain_len);

/* General ciphering APDUs (IEC 62056-5-3 §5.7.2) */
#define OSP_GEN_GLO_CIPHERING 0xDB
#define OSP_GEN_DED_CIPHERING 0xDC
#define OSP_GEN_CIPHERING     0xDD
#define OSP_GEN_SIGNING       0xDF

/** @brief Check whether a tag represents a general-ciphered APDU. */
bool osp_gen_is_ciphered_tag(uint8_t tag);

/** @brief Protect a plaintext APDU with general glo/ded ciphering. */
int osp_gen_glo_ded_protect(const osp_sec_context_t *ctx, bool dedicated, uint8_t plain_tag, const uint8_t *plaintext,
                            uint32_t plain_len, uint8_t *out, uint32_t *out_len);
/** @brief Unprotect a general glo/ded-ciphered APDU. */
int osp_gen_glo_ded_unprotect(osp_sec_context_t *ctx, const uint8_t *apdu, uint32_t apdu_len, uint8_t *plaintext,
                              uint32_t *plain_len, uint8_t *plain_tag);

/** @brief Protect a plaintext APDU with general ciphering (tag 0xDD). */
int osp_gen_ciphering_protect(const osp_sec_context_t *ctx, const uint8_t *transaction_id, uint32_t tx_id_len,
                              const uint8_t *recipient_st, uint32_t recipient_len, uint8_t plain_tag, const uint8_t *plaintext,
                              uint32_t plain_len, uint8_t *out, uint32_t *out_len);
/** @brief Unprotect a general-ciphered APDU (tag 0xDD). */
int osp_gen_ciphering_unprotect(osp_sec_context_t *ctx, const uint8_t *apdu, uint32_t apdu_len, uint8_t *plaintext,
                                uint32_t *plain_len, uint8_t *plain_tag);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SECURITY_H */
