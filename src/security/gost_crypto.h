#ifndef OSP_GOST_CRYPTO_H
#define OSP_GOST_CRYPTO_H

#include "../openspodes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_GOST_K_EM_SIZE     64
#define OSP_GOST_CMAC_SIZE     16
#define OSP_GOST_SIG_SIZE      64
#define OSP_GOST_PUBKEY_SIZE   64
#define OSP_GOST_PRIVKEY_SIZE  32

/* Streebog-256 (GOST R 34.11-2012). */
int osp_gost_streebog256(const uint8_t *input, uint32_t len, uint8_t output[32]);

/* HMAC-Streebog256 (RFC 7836 / R 50.1.113-2016). */
int osp_gost_hmac_streebog256(const uint8_t *key, uint32_t key_len, const uint8_t *msg, uint32_t msg_len, uint8_t mac[32]);

/* KDF_TREE_GOSTR3411_2012_256 (one-octet counter, R = 1). */
int osp_gost_kdf_tree(const uint8_t *key, uint32_t key_len, const uint8_t *label, uint32_t label_len, const uint8_t *seed,
                      uint32_t seed_len, uint8_t *out, uint32_t out_len);

/* Kuznyechik-CMAC (GOST R 34.13), 128-bit tag truncated to 16 bytes. */
int osp_gost_kuznyechik_cmac(const uint8_t key[32], const uint8_t *data, uint32_t len, uint8_t mac[16]);

/* Kuznyechik-CTR (R 1323565.1 KUZN_CTR): CTR1 = IV(96) || counter(32), counter starts at 1. */
int osp_gost_kuznyechik_ctr(const uint8_t key[32], const uint8_t iv[12], const uint8_t *in, uint32_t in_len, uint8_t *out);

/* KUZN_CMAC with s=96 (12-byte auth tag for DLMS transport). */
int osp_gost_kuznyechik_cmac96(const uint8_t key[32], const uint8_t *data, uint32_t len, uint8_t tag[12]);

/* KUZN-CTR-CMAC transport AEAD (R 1323565.1 §7.1): MSB256(K_EM)=encrypt, LSB256=MAC. */
int osp_gost_kuzn_aead_encrypt(const uint8_t k_em[64], const uint8_t iv[12], uint8_t sc, const uint8_t *af, uint32_t af_len,
                               const uint8_t *plaintext, uint32_t plain_len, bool auth, bool encr, uint8_t *out,
                               uint32_t *out_len, uint8_t tag[12]);
int osp_gost_kuzn_aead_decrypt(const uint8_t k_em[64], const uint8_t iv[12], uint8_t sc, const uint8_t *af, uint32_t af_len,
                               const uint8_t *in, uint32_t in_len, bool auth, bool encr, uint8_t *plaintext, uint32_t *plain_len,
                               const uint8_t tag[12]);

/* HLS mechanism 8: KUZN_CMAC(LSB256(K_EM), IV || SC || chal_a || chal_b). */
int osp_hls_gost_cmac(const uint8_t k_em[64], const uint8_t iv[12], uint8_t sc, const uint8_t *challenge_a,
                      uint32_t challenge_a_len, const uint8_t *challenge_b, uint32_t challenge_b_len, uint8_t mac[16]);

/* GOST R 34.10-2018-256 (paramSetB, Streebog-256 digest). Keys/signatures: little-endian Vec256. */
int osp_gost3410_public_key(const uint8_t d[32], uint8_t pk[64]);
int osp_gost3410_sign_with_k(const uint8_t d[32], const uint8_t *msg, uint32_t msg_len, const uint8_t k[32], uint8_t sig[64]);
int osp_gost3410_sign(const uint8_t d[32], const uint8_t *msg, uint32_t msg_len, uint8_t sig[64]);
int osp_gost3410_verify(const uint8_t pk[64], const uint8_t *msg, uint32_t msg_len, const uint8_t sig[64]);

/* VKO_GOST3410_2012_256 key agreement (R 50.1.113-2016 / RFC 7836). */
int osp_gost3410_vko(const uint8_t d[32], const uint8_t q_pub[64], const uint8_t *ukm, uint32_t ukm_len, uint8_t kek[32]);

#ifdef __cplusplus
}
#endif

#endif /* OSP_GOST_CRYPTO_H */
