#include "gost_crypto.h"
#include "streebog/gost3411-2012-core.h"
#include <string.h>

#define STREEBOG_BLOCK 64

int osp_gost_streebog256(const uint8_t *input, uint32_t len, uint8_t output[32]) {
	if (!input || !output) {
		return -1;
	}
	GOST34112012Context ctx;
	memset(&ctx, 0, sizeof(ctx));
	GOST34112012Init(&ctx, 256);
	GOST34112012Update(&ctx, input, len);
	GOST34112012Final(&ctx, output);
	GOST34112012Cleanup(&ctx);
	return 0;
}

int osp_gost_hmac_streebog256(const uint8_t *key, uint32_t key_len, const uint8_t *msg, uint32_t msg_len, uint8_t mac[32]) {
	if (!key || !msg || !mac) {
		return -1;
	}
	uint8_t k_block[STREEBOG_BLOCK];
	if (key_len > STREEBOG_BLOCK) {
		if (osp_gost_streebog256(key, key_len, k_block) != 0) {
			return -1;
		}
		memset(k_block + 32, 0, STREEBOG_BLOCK - 32);
	} else {
		memset(k_block, 0, STREEBOG_BLOCK);
		if (key_len > 0) {
			memcpy(k_block, key, key_len);
		}
	}

	uint8_t ipad[STREEBOG_BLOCK];
	uint8_t opad[STREEBOG_BLOCK];
	for (uint32_t i = 0; i < STREEBOG_BLOCK; i++) {
		ipad[i] = (uint8_t)(k_block[i] ^ 0x36);
		opad[i] = (uint8_t)(k_block[i] ^ 0x5C);
	}

	uint8_t inner[STREEBOG_BLOCK + 65536];
	if (msg_len > sizeof(inner) - STREEBOG_BLOCK) {
		return -1;
	}
	memcpy(inner, ipad, STREEBOG_BLOCK);
	memcpy(inner + STREEBOG_BLOCK, msg, msg_len);
	uint8_t inner_hash[32];
	if (osp_gost_streebog256(inner, STREEBOG_BLOCK + msg_len, inner_hash) != 0) {
		return -1;
	}

	uint8_t outer[STREEBOG_BLOCK + 32];
	memcpy(outer, opad, STREEBOG_BLOCK);
	memcpy(outer + STREEBOG_BLOCK, inner_hash, 32);
	return osp_gost_streebog256(outer, STREEBOG_BLOCK + 32, mac);
}

int osp_gost_kdf_tree(const uint8_t *key, uint32_t key_len, const uint8_t *label, uint32_t label_len, const uint8_t *seed,
                      uint32_t seed_len, uint8_t *out, uint32_t out_len) {
	if (!key || !label || !seed || !out || out_len == 0) {
		return -1;
	}
	uint32_t blocks = (out_len + 31u) / 32u;
	if (blocks == 0 || blocks > 255) {
		return -1;
	}
	uint16_t l_bits = (uint16_t)(out_len * 8u);
	uint8_t l_be[2] = {(uint8_t)(l_bits >> 8), (uint8_t)(l_bits)};

	uint8_t block[32];
	uint32_t pos = 0;
	for (uint8_t i = 1; i <= (uint8_t)blocks; i++) {
		uint8_t buf[512];
		uint32_t blen = 0;
		buf[blen++] = i;
		if (label_len > sizeof(buf) - blen - 3 - seed_len) {
			return -1;
		}
		memcpy(buf + blen, label, label_len);
		blen += label_len;
		buf[blen++] = 0x00;
		memcpy(buf + blen, seed, seed_len);
		blen += seed_len;
		memcpy(buf + blen, l_be, 2);
		blen += 2;
		if (osp_gost_hmac_streebog256(key, key_len, buf, blen, block) != 0) {
			return -1;
		}
		uint32_t copy = (pos + 32 <= out_len) ? 32 : (out_len - pos);
		memcpy(out + pos, block, copy);
		pos += copy;
	}
	return 0;
}
