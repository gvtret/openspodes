#include "gost_crypto.h"
#include "security.h"
#include "kuznyechik_tables.h"
#include <stdbool.h>
#include <string.h>

typedef uint8_t kuz_block[16];
typedef kuz_block kuz_round_keys[10];

static inline uint8_t kuz_get_idx(uint8_t b, uint8_t i) {
	return (uint8_t)((b - i) & 0x0F);
}

static inline uint8_t kuz_get_m(const kuz_block msg, uint8_t b, uint8_t i) {
	return msg[kuz_get_idx(b, i)];
}

static void kuz_l_step(kuz_block block, uint8_t i) {
	uint8_t x = block[kuz_get_idx(15, i)];
	x ^= OSP_KUZN_GFT_148[kuz_get_m(block, 14, i)];
	x ^= OSP_KUZN_GFT_32[kuz_get_m(block, 13, i)];
	x ^= OSP_KUZN_GFT_133[kuz_get_m(block, 12, i)];
	x ^= OSP_KUZN_GFT_16[kuz_get_m(block, 11, i)];
	x ^= OSP_KUZN_GFT_194[kuz_get_m(block, 10, i)];
	x ^= OSP_KUZN_GFT_192[kuz_get_m(block, 9, i)];
	x ^= block[kuz_get_idx(8, i)];
	x ^= OSP_KUZN_GFT_251[kuz_get_m(block, 7, i)];
	x ^= block[kuz_get_idx(6, i)];
	x ^= OSP_KUZN_GFT_192[kuz_get_m(block, 5, i)];
	x ^= OSP_KUZN_GFT_194[kuz_get_m(block, 4, i)];
	x ^= OSP_KUZN_GFT_16[kuz_get_m(block, 3, i)];
	x ^= OSP_KUZN_GFT_133[kuz_get_m(block, 2, i)];
	x ^= OSP_KUZN_GFT_32[kuz_get_m(block, 1, i)];
	x ^= OSP_KUZN_GFT_148[kuz_get_m(block, 0, i)];
	block[kuz_get_idx(15, i)] = x;
}

static void kuz_xor(kuz_block a, const kuz_block b) {
	for (uint32_t i = 0; i < 16; i++) {
		a[i] ^= b[i];
	}
}

static void kuz_lsx(kuz_block block, const kuz_block key) {
	kuz_xor(block, key);
	for (uint32_t i = 0; i < 16; i++) {
		block[i] = OSP_KUZN_P[block[i]];
	}
	for (uint32_t i = 0; i < 16; i++) {
		kuz_l_step(block, (uint8_t)i);
	}
}

static void kuz_get_c(kuz_block out, uint32_t n) {
	memcpy(out, OSP_KUZN_KEYGEN[n], 16);
}

static void kuz_f(kuz_block k1, kuz_block k2, uint32_t n) {
	for (uint32_t i = 0; i < 4; i++) {
		kuz_block k1_cpy, k2_cpy, c;
		memcpy(k1_cpy, k1, 16);
		kuz_get_c(c, 8 * n + 2 * i);
		kuz_lsx(k1_cpy, c);
		kuz_xor(k2, k1_cpy);

		memcpy(k2_cpy, k2, 16);
		kuz_get_c(c, 8 * n + 2 * i + 1);
		kuz_lsx(k2_cpy, c);
		kuz_xor(k1, k2_cpy);
	}
}

static void kuz_expand(const uint8_t key[32], kuz_round_keys keys) {
	kuz_block k1, k2;
	memcpy(k1, key, 16);
	memcpy(k2, key + 16, 16);
	memcpy(keys[0], k1, 16);
	memcpy(keys[1], k2, 16);
	for (uint32_t i = 1; i < 5; i++) {
		kuz_f(k1, k2, i - 1);
		memcpy(keys[2 * i], k1, 16);
		memcpy(keys[2 * i + 1], k2, 16);
	}
}

static void kuz_encrypt_block(const kuz_round_keys keys, kuz_block block) {
	for (uint32_t i = 0; i < 9; i++) {
		kuz_lsx(block, keys[i]);
	}
	kuz_xor(block, keys[9]);
}

static void kuz_dbl(uint8_t block[16]) {
	uint8_t carry = (uint8_t)(block[0] >> 7);
	for (uint32_t i = 0; i < 15; i++) {
		block[i] = (uint8_t)((block[i] << 1) | (block[i + 1] >> 7));
	}
	block[15] = (uint8_t)((block[15] << 1) ^ (carry ? 0x87 : 0));
}

int osp_gost_kuznyechik_cmac(const uint8_t key[32], const uint8_t *data, uint32_t len, uint8_t mac[16]) {
	if (!key || !mac) {
		return -1;
	}
	if (!data && len > 0) {
		return -1;
	}

	kuz_round_keys keys;
	kuz_expand(key, keys);

	kuz_block l = {0};
	kuz_encrypt_block(keys, l);

	kuz_block k1, k2;
	memcpy(k1, l, 16);
	kuz_dbl(k1);
	memcpy(k2, k1, 16);
	kuz_dbl(k2);

	const uint32_t nblocks = (len == 0) ? 1 : ((len + 15) / 16);
	const bool complete_last = (len > 0) && ((len % 16) == 0);

	kuz_block state = {0};
	for (uint32_t b = 0; b < nblocks - 1; b++) {
		for (uint32_t i = 0; i < 16; i++) {
			state[i] ^= data[b * 16 + i];
		}
		kuz_encrypt_block(keys, state);
	}

	kuz_block last = {0};
	if (len == 0) {
		last[0] = 0x80;
		kuz_xor(last, k2);
	} else if (complete_last) {
		memcpy(last, data + (nblocks - 1) * 16, 16);
		kuz_xor(last, k1);
	} else {
		uint32_t rem = len - (nblocks - 1) * 16;
		memcpy(last, data + (nblocks - 1) * 16, rem);
		last[rem] = 0x80;
		kuz_xor(last, k2);
	}

	kuz_xor(state, last);
	kuz_encrypt_block(keys, state);
	memcpy(mac, state, 16);
	osp_memzero(keys, sizeof(keys));
	return 0;
}

int osp_gost_kuznyechik_ctr(const uint8_t key[32], const uint8_t iv[12], const uint8_t *in, uint32_t in_len, uint8_t *out) {
	if (!key || !iv || !out || (!in && in_len > 0)) {
		return -1;
	}

	kuz_round_keys keys;
	kuz_expand(key, keys);

	uint32_t offset = 0;
	uint32_t counter = 1;
	while (offset < in_len) {
		kuz_block ctr = {0};
		memcpy(ctr, iv, 12);
		ctr[12] = (uint8_t)(counter >> 24);
		ctr[13] = (uint8_t)(counter >> 16);
		ctr[14] = (uint8_t)(counter >> 8);
		ctr[15] = (uint8_t)(counter);

		kuz_encrypt_block(keys, ctr);

		uint32_t chunk = in_len - offset;
		if (chunk > 16) {
			chunk = 16;
		}
		for (uint32_t i = 0; i < chunk; i++) {
			out[offset + i] = in[offset + i] ^ ctr[i];
		}
		offset += chunk;
		counter++;
	}
	osp_memzero(keys, sizeof(keys));
	return 0;
}

int osp_gost_kuznyechik_cmac96(const uint8_t key[32], const uint8_t *data, uint32_t len, uint8_t tag[12]) {
	uint8_t mac[16];
	if (osp_gost_kuznyechik_cmac(key, data, len, mac) != 0) {
		return -1;
	}
	memcpy(tag, mac, 12);
	return 0;
}

static int kuzn_build_tag(const uint8_t k_m[32], const uint8_t iv[12], uint8_t sc, const uint8_t *af, uint32_t af_len,
                          const uint8_t *data, uint32_t data_len, uint8_t tag[12]) {
	uint8_t buf[12 + 1 + 512 + OSP_GLO_MAX_PLAIN];
	uint32_t pos = 0;
	if (12 + 1 + af_len + data_len > sizeof(buf)) {
		return -1;
	}
	memcpy(&buf[pos], iv, 12);
	pos += 12;
	buf[pos++] = sc;
	if (af && af_len > 0) {
		memcpy(&buf[pos], af, af_len);
		pos += af_len;
	}
	if (data && data_len > 0) {
		memcpy(&buf[pos], data, data_len);
		pos += data_len;
	}
	return osp_gost_kuznyechik_cmac96(k_m, buf, pos, tag);
}

int osp_gost_kuzn_aead_encrypt(const uint8_t k_em[64], const uint8_t iv[12], uint8_t sc, const uint8_t *af, uint32_t af_len,
                               const uint8_t *plaintext, uint32_t plain_len, bool auth, bool encr, uint8_t *out,
                               uint32_t *out_len, uint8_t tag[12]) {
	if (!k_em || !iv || !out || !out_len || (auth && !tag)) {
		return -1;
	}
	if (encr && !plaintext && plain_len > 0) {
		return -1;
	}
	if (!encr && auth && !plaintext && plain_len > 0) {
		return -1;
	}

	const uint8_t *k_e = k_em;
	const uint8_t *k_m = k_em + 32;

	if (auth && encr) {
		if (osp_gost_kuznyechik_ctr(k_e, iv, plaintext, plain_len, out) != 0) {
			return -1;
		}
		*out_len = plain_len;
		return kuzn_build_tag(k_m, iv, sc, af, af_len, out, plain_len, tag);
	}
	if (auth && !encr) {
		memcpy(out, plaintext, plain_len);
		*out_len = plain_len;
		return kuzn_build_tag(k_m, iv, sc, af, af_len, plaintext, plain_len, tag);
	}
	if (!auth && encr) {
		if (osp_gost_kuznyechik_ctr(k_e, iv, plaintext, plain_len, out) != 0) {
			return -1;
		}
		*out_len = plain_len;
		return 0;
	}
	if (!plaintext || plain_len == 0) {
		return -1;
	}
	memcpy(out, plaintext, plain_len);
	*out_len = plain_len;
	return 0;
}

int osp_gost_kuzn_aead_decrypt(const uint8_t k_em[64], const uint8_t iv[12], uint8_t sc, const uint8_t *af, uint32_t af_len,
                               const uint8_t *in, uint32_t in_len, bool auth, bool encr, uint8_t *plaintext, uint32_t *plain_len,
                               const uint8_t tag[12]) {
	if (!k_em || !iv || !plaintext || !plain_len) {
		return -1;
	}
	if (auth && !tag) {
		return -1;
	}

	const uint8_t *k_e = k_em;
	const uint8_t *k_m = k_em + 32;

	if (auth && encr) {
		uint8_t expected[12];
		if (kuzn_build_tag(k_m, iv, sc, af, af_len, in, in_len, expected) != 0) {
			return -1;
		}
		if (memcmp(expected, tag, 12) != 0) {
			return -2;
		}
		if (osp_gost_kuznyechik_ctr(k_e, iv, in, in_len, plaintext) != 0) {
			return -1;
		}
		*plain_len = in_len;
		return 0;
	}
	if (auth && !encr) {
		uint8_t expected[12];
		if (kuzn_build_tag(k_m, iv, sc, af, af_len, in, in_len, expected) != 0) {
			return -1;
		}
		if (memcmp(expected, tag, 12) != 0) {
			return -2;
		}
		memcpy(plaintext, in, in_len);
		*plain_len = in_len;
		return 0;
	}
	if (!auth && encr) {
		if (osp_gost_kuznyechik_ctr(k_e, iv, in, in_len, plaintext) != 0) {
			return -1;
		}
		*plain_len = in_len;
		return 0;
	}
	if (!auth && !encr) {
		if (!in || in_len == 0) {
			return -1;
		}
		memcpy(plaintext, in, in_len);
		*plain_len = in_len;
		return 0;
	}
	return -1;
}

int osp_hls_gost_cmac(const uint8_t k_em[64], const uint8_t iv[12], uint8_t sc, const uint8_t *challenge_a,
                      uint32_t challenge_a_len, const uint8_t *challenge_b, uint32_t challenge_b_len, uint8_t mac[16]) {
	if (!k_em || !iv || !challenge_a || !challenge_b || !mac) {
		return -1;
	}
	uint8_t buf[12 + 1 + OSP_SEC_CHALLENGE_MAX * 2];
	if (12 + 1 + challenge_a_len + challenge_b_len > sizeof(buf)) {
		return -1;
	}
	uint32_t pos = 0;
	memcpy(&buf[pos], iv, 12);
	pos += 12;
	buf[pos++] = sc;
	memcpy(&buf[pos], challenge_a, challenge_a_len);
	pos += challenge_a_len;
	memcpy(&buf[pos], challenge_b, challenge_b_len);
	pos += challenge_b_len;
	return osp_gost_kuznyechik_cmac(&k_em[32], buf, pos, mac);
}
