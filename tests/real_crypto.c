#include "mock_crypto.h"
#include <openssl/evp.h>
#include <string.h>

static int real_gcm_crypt(osp_gcm_dir_t dir, const uint8_t *key, uint32_t key_len, const uint8_t iv[12], const uint8_t *aad, uint32_t aad_len,
                          const uint8_t *in, uint32_t in_len, uint8_t *out, const uint8_t tag_in[OSP_SEC_TAG_SIZE], uint8_t tag_out[OSP_SEC_TAG_SIZE]) {
	const EVP_CIPHER *cipher = (key_len == 32) ? EVP_aes_256_gcm() : EVP_aes_128_gcm();
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (!ctx) {
		return -1;
	}

	int ok = 0;
	int enc = (dir == OSP_GCM_ENCRYPT) ? 1 : 0;
	int out_len = 0;
	int final_len = 0;

	if (EVP_CipherInit_ex(ctx, cipher, NULL, NULL, NULL, enc) != 1) {
		goto done;
	}
	if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, NULL) != 1) {
		goto done;
	}
	if (EVP_CipherInit_ex(ctx, cipher, NULL, key, iv, enc) != 1) {
		goto done;
	}
	if (aad && aad_len > 0) {
		if (EVP_CipherUpdate(ctx, NULL, &out_len, aad, (int)aad_len) != 1) {
			goto done;
		}
	}
	if (enc) {
		if (in && in_len > 0) {
			if (EVP_CipherUpdate(ctx, out, &out_len, in, (int)in_len) != 1) {
				goto done;
			}
		}
		if (EVP_CipherFinal_ex(ctx, out + out_len, &final_len) != 1) {
			goto done;
		}
		if (!tag_out || EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, OSP_SEC_TAG_SIZE, tag_out) != 1) {
			goto done;
		}
	} else {
		if (tag_in && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, OSP_SEC_TAG_SIZE, (void *)tag_in) != 1) {
			goto done;
		}
		if (in && in_len > 0) {
			if (EVP_CipherUpdate(ctx, out, &out_len, in, (int)in_len) != 1) {
				goto done;
			}
		}
		if (EVP_CipherFinal_ex(ctx, out + out_len, &final_len) != 1) {
			goto done;
		}
	}
	ok = 1;

done:
	EVP_CIPHER_CTX_free(ctx);
	return ok ? 0 : -1;
}

void mock_crypto_init_real_gcm(void) {
	osp_hal_gcm_crypt = real_gcm_crypt;
}

static void real_md5(const uint8_t *input, uint32_t len, uint8_t output[16]) {
	unsigned int out_len = 16;
	EVP_Digest(input, len, output, &out_len, EVP_md5(), NULL);
}

static void real_sha1(const uint8_t *input, uint32_t len, uint8_t output[20]) {
	unsigned int out_len = 20;
	EVP_Digest(input, len, output, &out_len, EVP_sha1(), NULL);
}

static void real_sha256(const uint8_t *input, uint32_t len, uint8_t output[32]) {
	unsigned int out_len = 32;
	EVP_Digest(input, len, output, &out_len, EVP_sha256(), NULL);
}

void mock_crypto_init_real_hashes(void) {
	osp_hal_md5 = real_md5;
	osp_hal_sha1 = real_sha1;
	osp_hal_sha256 = real_sha256;
}
