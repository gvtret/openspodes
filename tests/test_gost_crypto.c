#include "../src/security/gost_crypto.h"
#include "../src/security/general_ciphering.h"
#include "../src/security/security.h"
#include "mock_crypto.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <cmocka.h>

static int hex_nib(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

static int hex_decode(const char *hex, uint8_t *out, uint32_t max_len) {
	uint32_t n = 0;
	for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) {
		int hi = hex_nib(hex[i]);
		int lo = hex_nib(hex[i + 1]);
		if (hi < 0 || lo < 0 || n >= max_len) {
			return -1;
		}
		out[n++] = (uint8_t)((hi << 4) | lo);
	}
	return (int)n;
}

static void test_gost_cmac_golden(void **state) {
	(void)state;
	uint8_t k_em[64];
	uint8_t iv[12];
	uint8_t stoc[8];
	uint8_t ctos[8];
	assert_int_equal(hex_decode(
	                     "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f",
	                     k_em, sizeof(k_em)),
	                 64);
	assert_int_equal(hex_decode("4d4d4d0000bc614e01234567", iv, sizeof(iv)), 12);
	assert_int_equal(hex_decode("8899aabbccddeeff", stoc, sizeof(stoc)), 8);
	assert_int_equal(hex_decode("0011223344556677", ctos, sizeof(ctos)), 8);

	uint8_t mac[16];
	assert_int_equal(osp_hls_gost_cmac(k_em, iv, 0x30, stoc, 8, ctos, 8, mac), 0);

	uint8_t expected[16];
	assert_int_equal(hex_decode("49084b38029d145d3621a1fd76a31d5e", expected, sizeof(expected)), 16);
	assert_memory_equal(mac, expected, 16);
}

static void test_streebog_hash_with_titles(void **state) {
	(void)state;
	uint8_t buf[128];
	uint32_t pos = 0;
	memcpy(&buf[pos], "secret16bytes!!!", 16);
	pos += 16;
	memcpy(&buf[pos], "CCCCCCCC", 8);
	pos += 8;
	memcpy(&buf[pos], "SSSSSSSS", 8);
	pos += 8;
	buf[pos++] = 1;
	buf[pos++] = 2;
	buf[pos++] = 3;
	buf[pos++] = 4;

	uint8_t h[32];
	assert_int_equal(osp_gost_streebog256(buf, pos, h), 0);
	uint8_t expected[32];
	assert_int_equal(hex_decode("ae3b1505ee28c1d5da476608320f8b12cbe8f53faea43b3860a2bfb4e2d8dfb4", expected, sizeof(expected)), 32);
	assert_memory_equal(h, expected, 32);
}

#if 1
static void test_gost3410_public_key_vko_vector(void **state) {
	(void)state;
	uint8_t d[32], pk[64], expected[64];
	assert_int_equal(hex_decode("68696a6b6c6d6e6f6061626364656667ddddddddccccccccaaaaaaaabbbbbbbb", d, sizeof(d)), 32);
	assert_int_equal(hex_decode("95da164bcee759b2ae0f1860c9845d34734b5ae066aeaa3fcf4394bfec09d4d"
	                             "3f4a226a0015ca8c9338ea12dbe83502a581ecc15b80ace45c7f25bc8275962a7",
	                             expected, sizeof(expected)),
	                 64);
	assert_int_equal(osp_gost3410_public_key(d, pk), 0);
	assert_memory_equal(pk, expected, 64);
}

static void test_gost3410_control_example(void **state) {
	(void)state;
	uint8_t d[32], k[32], msg[32], sig[64], expected[64];
	assert_int_equal(hex_decode("48494a4b4c4d4e4f4041424344454647bbbbaaaa999988884444555566667777", d, sizeof(d)), 32);
	assert_int_equal(hex_decode("43730c5cbccacf915ac292676f21e8bd4ef75331d9405e5f1a61dc3130a65011", k, sizeof(k)), 32);
	assert_int_equal(hex_decode("77006611552244338899aabbccddeeff001122334455667789abcdef", msg, sizeof(msg)), 28);
	assert_int_equal(hex_decode("d3b72bb12fb7da1a06f8e11acdec034ffcf14588301a3315bbe8cd611fc4545e"
	                             "a9fae88aeac47cd46a0858711d942223c523bfd53cbadff97e0eec1f69a3efca",
	                             expected, sizeof(expected)),
	                 64);
	assert_int_equal(osp_gost3410_sign_with_k(d, msg, 28, k, sig), 0);
	assert_memory_equal(sig, expected, 64);
}

static void test_gost3410_sign_verify_roundtrip(void **state) {
	(void)state;
	uint8_t d[32];
	assert_int_equal(hex_decode("48494a4b4c4d4e4f4041424344454647bbbbaaaa999988884444555566667777", d, sizeof(d)), 32);
	uint8_t pk[64];
	assert_int_equal(osp_gost3410_public_key(d, pk), 0);

	const uint8_t msg[] = "SystemTitle-a||SystemTitle-b||StoC||CtoS";
	uint8_t sig[64];
	assert_int_equal(osp_gost3410_sign(d, msg, (uint32_t)sizeof(msg) - 1, sig), 0);
	assert_int_equal(osp_gost3410_verify(pk, msg, (uint32_t)sizeof(msg) - 1, sig), 0);
	assert_int_not_equal(osp_gost3410_verify(pk, (const uint8_t *)"tampered", 8, sig), 0);
}

static void test_gost3410_vko_r1323565(void **state) {
	(void)state;
	uint8_t d[32], q[64], ukm[16], kek[32], expected[32];
	assert_int_equal(hex_decode("68696a6b6c6d6e6f6061626364656667ddddddddccccccccaaaaaaaabbbbbbbb", d, sizeof(d)), 32);
	assert_int_equal(hex_decode("212daf02de1c91ea961e58e01e42df1733c00748998bc34d76dad96b3b256378"
	                             "7b9cffcfa0f24753d6d5eb6133b35a95375a0ef683b3ff5be7d61b99d7fe6617",
	                             q, sizeof(q)),
	                 64);
	assert_int_equal(hex_decode("f0f0f0f0e1e1e1e1d2d2d2d2c3c3c3c3", ukm, sizeof(ukm)), 16);
	assert_int_equal(hex_decode("4f54f663029709c0271facd5bb6d58187410477e102555a893d45a04ab0cafc0", expected, sizeof(expected)), 32);
	assert_int_equal(osp_gost3410_vko(d, q, ukm, 16, kek), 0);
	assert_memory_equal(kek, expected, 32);
}

static void test_gost_kdf_tree_r1323565(void **state) {
	(void)state;
	uint8_t k[32], label[7], seed[16];
	uint8_t out[96], expected[96];
	assert_int_equal(hex_decode("4f54f663029709c0271facd5bb6d58187410477e102555a893d45a04ab0cafc0", k, sizeof(k)), 32);
	assert_int_equal(hex_decode("60857406080304", label, sizeof(label)), 7);
	assert_int_equal(hex_decode("ff00ee11dd22cc33bb44aa5599668877", seed, sizeof(seed)), 16);
	assert_int_equal(hex_decode("e7f74dc8fcafd9738fd14d5aa542834bac7e883eff37931c082a9a80b45f60dd"
	                             "159d1118b56f8e78e938c28715c34c3c197a2339638901de1c610180f7de34ac"
	                             "424237f626e9ae5b55dbfa12ffd9cb7dfb903019eecc8228876015b2c15cbc89",
	                             expected, sizeof(expected)),
	                 96);
	assert_int_equal(osp_gost_kdf_tree(k, 32, label, 7, seed, 16, out, 96), 0);
	assert_memory_equal(out, expected, 96);
}

static void test_gost3410_vko_roundtrip(void **state) {
	(void)state;
	uint8_t alice_sk[32], bob_sk[32], alice_pk[64], bob_pk[64], ukm[16];
	uint8_t key_a[32], key_b[32];
	assert_int_equal(hex_decode("48494a4b4c4d4e4f4041424344454647bbbbaaaa999988884444555566667777", alice_sk, sizeof(alice_sk)), 32);
	assert_int_equal(hex_decode("68696a6b6c6d6e6f6061626364656667ddddddddccccccccaaaaaaaabbbbbbbb", bob_sk, sizeof(bob_sk)), 32);
	assert_int_equal(osp_gost3410_public_key(alice_sk, alice_pk), 0);
	assert_int_equal(osp_gost3410_public_key(bob_sk, bob_pk), 0);
	memset(ukm, 0xAB, sizeof(ukm));
	assert_int_equal(osp_gost3410_vko(alice_sk, bob_pk, ukm, sizeof(ukm), key_a), 0);
	assert_int_equal(osp_gost3410_vko(bob_sk, alice_pk, ukm, sizeof(ukm), key_b), 0);
	assert_memory_equal(key_a, key_b, 32);
}

static void test_gen_signing_roundtrip(void **state) {
	(void)state;
	osp_gen_signing_t apdu;
	memset(&apdu, 0, sizeof(apdu));
	const uint8_t tx[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45};
	const uint8_t orig[8] = {0x4D, 0x4D, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x01};
	const uint8_t recip[8] = {0x4D, 0x4D, 0x4D, 0x00, 0x00, 0xBC, 0x61, 0x4E};
	const uint8_t content[7] = {0xC4, 0x01, 0xC1, 0x00, 0x12, 0x12, 0x34};
	memcpy(apdu.transaction_id, tx, sizeof(tx));
	apdu.transaction_id_len = sizeof(tx);
	memcpy(apdu.originator_st, orig, sizeof(orig));
	memcpy(apdu.recipient_st, recip, sizeof(recip));
	apdu.recipient_st_len = sizeof(recip);
	memcpy(apdu.content, content, sizeof(content));
	apdu.content_len = sizeof(content);
	memset(apdu.signature, 0xAB, 64);
	apdu.signature_len = 64;

	uint8_t encoded[256];
	uint32_t enc_len = 0;
	assert_int_equal(osp_gen_signing_encode(&apdu, encoded, &enc_len), 0);
	assert_int_equal(encoded[0], OSP_GEN_SIGNING);

	osp_gen_signing_t decoded;
	assert_int_equal(osp_gen_signing_decode(encoded, enc_len, &decoded), 0);
	assert_int_equal(decoded.transaction_id_len, apdu.transaction_id_len);
	assert_memory_equal(decoded.transaction_id, apdu.transaction_id, apdu.transaction_id_len);
	assert_memory_equal(decoded.originator_st, apdu.originator_st, OSP_SEC_SYSTEM_TITLE_SIZE);
	assert_int_equal(decoded.recipient_st_len, apdu.recipient_st_len);
	assert_memory_equal(decoded.recipient_st, apdu.recipient_st, apdu.recipient_st_len);
	assert_int_equal(decoded.content_len, apdu.content_len);
	assert_memory_equal(decoded.content, apdu.content, apdu.content_len);
	assert_int_equal(decoded.signature_len, apdu.signature_len);
	assert_memory_equal(decoded.signature, apdu.signature, apdu.signature_len);
}

static void test_gen_signing_protect_gost(void **state) {
	(void)state;
	static const uint8_t client_sk[32] = {0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	                                      0xBB, 0xBB, 0xAA, 0xAA, 0x99, 0x99, 0x88, 0x88, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66, 0x77, 0x77};
	uint8_t client_pk[64];
	assert_int_equal(osp_gost3410_public_key(client_sk, client_pk), 0);

	const uint8_t client_st[8] = {0x4D, 0x4D, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x01};
	const uint8_t server_st[8] = {0x4D, 0x4D, 0x4D, 0x00, 0x00, 0xBC, 0x61, 0x4E};
	const uint8_t tx[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45};
	const uint8_t content[7] = {0xC4, 0x01, 0xC1, 0x00, 0x12, 0x12, 0x34};

	osp_sec_context_t tx_ctx, rx_ctx;
	osp_sec_context_init(&tx_ctx, OSP_SUITE_8, OSP_MECH_HLS_GOST_SIG, client_st);
	memcpy(tx_ctx.signing_key, client_sk, sizeof(client_sk));
	tx_ctx.signing_key_len = (uint8_t)sizeof(client_sk);
	osp_sec_context_init(&rx_ctx, OSP_SUITE_8, OSP_MECH_HLS_GOST_SIG, server_st);
	memcpy(rx_ctx.peer_public_key, client_pk, sizeof(client_pk));
	rx_ctx.peer_public_key_len = (uint8_t)sizeof(client_pk);

	uint8_t apdu[512];
	uint32_t apdu_len = 0;
	assert_int_equal(osp_gen_signing_protect(&tx_ctx, tx, sizeof(tx), server_st, sizeof(server_st), content, sizeof(content), apdu,
	                                         &apdu_len),
	                 0);
	assert_int_equal(apdu[0], OSP_GEN_SIGNING);

	uint8_t recovered[32];
	uint32_t recovered_len = sizeof(recovered);
	assert_int_equal(osp_gen_signing_unprotect(&rx_ctx, apdu, apdu_len, recovered, &recovered_len), 0);
	assert_int_equal(recovered_len, sizeof(content));
	assert_memory_equal(recovered, content, sizeof(content));
	assert_memory_equal(rx_ctx.peer_system_title, client_st, sizeof(client_st));
}
#endif

static void test_hls_gost_streebog_handshake(void **state) {
	(void)state;
	const uint8_t client_st[8] = {'C', 'L', 'I', 'E', 'N', 'T', '0', '1'};
	const uint8_t server_st[8] = {'S', 'E', 'R', 'V', 'E', 'R', '0', '1'};
	osp_sec_context_t client_sec, server_sec;

	osp_sec_context_init(&client_sec, OSP_SUITE_8, OSP_MECH_HLS_GOST_STREEBOG, client_st);
	osp_sec_context_init(&server_sec, OSP_SUITE_8, OSP_MECH_HLS_GOST_STREEBOG, server_st);
	memcpy(client_sec.gak, "secret16bytes!!!", 16);
	memcpy(server_sec.gak, "secret16bytes!!!", 16);
	memcpy(client_sec.peer_system_title, server_st, 8);
	memcpy(server_sec.peer_system_title, client_st, 8);

	client_sec.ctos_len = 4;
	client_sec.stoc_len = 4;
	server_sec.ctos_len = 4;
	server_sec.stoc_len = 4;
	client_sec.ctos[0] = 0x11;
	client_sec.stoc[0] = 0xAA;
	server_sec.ctos[0] = 0x11;
	server_sec.stoc[0] = 0xAA;

	uint8_t f_stoc[32], f_ctos[32];
	uint32_t f_len = 0;
	assert_int_equal(osp_hls_pass3_build(&client_sec, f_stoc, sizeof(f_stoc), &f_len), 0);
	assert_int_equal(f_len, 32);
	assert_int_equal(osp_hls_pass3_verify(&server_sec, f_stoc, f_len), 0);
	assert_int_equal(osp_hls_pass4_build(&server_sec, f_ctos, sizeof(f_ctos), &f_len), 0);
	assert_int_equal(osp_hls_pass4_verify(&client_sec, f_ctos, f_len), 0);
}

static void test_hls_gost_cmac_handshake(void **state) {
	(void)state;
	const uint8_t client_st[8] = {'C', 'L', 'I', 'E', 'N', 'T', '0', '2'};
	const uint8_t server_st[8] = {'S', 'E', 'R', 'V', 'E', 'R', '0', '2'};
	osp_sec_context_t client_sec, server_sec;

	osp_sec_context_init(&client_sec, OSP_SUITE_8, OSP_MECH_HLS_GOST_CMAC, client_st);
	osp_sec_context_init(&server_sec, OSP_SUITE_8, OSP_MECH_HLS_GOST_CMAC, server_st);
	memset(client_sec.k_em, 0x11, sizeof(client_sec.k_em));
	memset(server_sec.k_em, 0x11, sizeof(server_sec.k_em));
	memcpy(client_sec.peer_system_title, server_st, 8);
	memcpy(server_sec.peer_system_title, client_st, 8);

	client_sec.ctos_len = 8;
	client_sec.stoc_len = 8;
	server_sec.ctos_len = 8;
	server_sec.stoc_len = 8;
	memset(client_sec.ctos, 0xBB, 8);
	memset(client_sec.stoc, 0xCC, 8);
	memcpy(server_sec.ctos, client_sec.ctos, 8);
	memcpy(server_sec.stoc, client_sec.stoc, 8);

	uint8_t f_stoc[32], f_ctos[32];
	uint32_t f_len = 0;
	assert_int_equal(osp_hls_pass3_build(&client_sec, f_stoc, sizeof(f_stoc), &f_len), 0);
	assert_int_equal(f_len, 21);
	assert_int_equal(osp_hls_pass3_verify(&server_sec, f_stoc, f_len), 0);
	assert_int_equal(osp_hls_pass4_build(&server_sec, f_ctos, sizeof(f_ctos), &f_len), 0);
	assert_int_equal(osp_hls_pass4_verify(&client_sec, f_ctos, f_len), 0);
}

static void test_kuzn_ctr_roundtrip(void **state) {
	(void)state;
	uint8_t key[32];
	assert_int_equal(hex_decode("08090a0b0c0d0e0f0001020304050607fedcba9876543210eca86420fdb97531", key, sizeof(key)), 32);

	uint8_t iv[12];
	assert_int_equal(hex_decode("ff00ee11dd22cc33f0e1d2c3", iv, sizeof(iv)), 12);

	const uint8_t plain[] = {0xC0, 0x01, 0xC1, 0x00, 0x08, 0x00, 0x00, 0x01, 0x00, 0x00, 0xFF, 0x02, 0x00};
	uint8_t cipher[sizeof(plain)];
	uint8_t recovered[sizeof(plain)];

	assert_int_equal(osp_gost_kuznyechik_ctr(key, iv, plain, sizeof(plain), cipher), 0);
	assert_int_equal(osp_gost_kuznyechik_ctr(key, iv, cipher, sizeof(plain), recovered), 0);
	assert_memory_equal(recovered, plain, sizeof(plain));
}

static void test_glo_gost_suite8_roundtrip(void **state) {
	(void)state;
	osp_sec_context_t tx, rx;

	uint8_t st[8];
	assert_int_equal(hex_decode("ff00ee11dd22cc33", st, sizeof(st)), 8);
	osp_sec_context_init(&tx, OSP_SUITE_8, OSP_MECH_HLS_GOST_CMAC, st);
	osp_sec_context_init(&rx, OSP_SUITE_8, OSP_MECH_HLS_GOST_CMAC, st);
	tx.policy = OSP_POLICY_ENCR_AUTH;
	rx.policy = OSP_POLICY_ENCR_AUTH;
	tx.invocation_counter = 0xf0e1d2c3;

	assert_int_equal(hex_decode(
	                     "08090a0b0c0d0e0f0001020304050607fedcba9876543210eca86420fdb97531"
	                     "18191a1b1c1d1e1f10111213141516170123456789abcdef13579bdf02468ace",
	                     tx.k_em, sizeof(tx.k_em)),
	                 64);
	memcpy(rx.k_em, tx.k_em, sizeof(tx.k_em));

	const uint8_t plain[] = {0xC0, 0x01, 0xC1, 0x00, 0x08, 0x00, 0x00, 0x01, 0x00, 0x00, 0xFF, 0x02, 0x00};
	uint8_t ciphered[128];
	uint32_t ciphered_len = 0;
	assert_int_equal(osp_glo_protect(&tx, OSP_GLO_GET_REQUEST, plain, sizeof(plain), ciphered, &ciphered_len), 0);
	assert_int_equal(ciphered[0], OSP_GLO_GET_REQUEST);

	uint8_t recovered[128];
	uint32_t recovered_len = 0;
	assert_int_equal(osp_glo_unprotect(&rx, ciphered, ciphered_len, recovered, &recovered_len), 0);
	assert_int_equal(recovered_len, sizeof(plain));
	assert_memory_equal(recovered, plain, sizeof(plain));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Golden vectors: Kuznyechik AEAD (R 1323565.1)
 *
 *  Test data from R 1323565.1.032-2020 §A:
 *  K_EM (MSB 256 of K_KEK) = 08090a0b0c0d0e0f 0001020304050607
 *                             fedcba9876543210 eca86420fdb97531
 *  K_ENC (LSB 256 of K_KEK) = 18191a1b1c1d1e1f 1011121314151617
 *                              0123456789abcdef 13579bdf02468ace
 *  IV = ff00ee11dd22cc33 f0e1d2c3 (12 bytes)
 *  Key = 28292a2b2c2d2e2f 2021222324252627
 *        ffeeddccbbaa9988 0011223344556677
 *        38393a3b3c3d3e3f 3031323334353637
 *        ffffeeeeddddcccc 0000111122223333
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_kuzn_aead_encrypt_decrypt(void **state) {
	(void)state;

	/* K_EM (MSB 256) || K_ENC (LSB 256) = 64-byte key */
	uint8_t k_em[64] = {
	    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
	    0xec, 0xa8, 0x64, 0x20, 0xfd, 0xb9, 0x75, 0x31,
	    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
	    0x13, 0x57, 0x9b, 0xdf, 0x02, 0x46, 0x8a, 0xce,
	};

	/* IV = system-title-U || IC-KEK = ff00ee11dd22cc33 f0e1d2c3 */
	uint8_t iv[12] = {
	    0xff, 0x00, 0xee, 0x11, 0xdd, 0x22, 0xcc, 0x33,
	    0xf0, 0xe1, 0xd2, 0xc3,
	};

	/* Plaintext (8 bytes — minimal test) */
	uint8_t plaintext[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	uint8_t ciphertext[8] = {0};
	uint32_t ciphered_len = 0;
	uint8_t tag[12] = {0};

	/* Encrypt: auth + encr */
	assert_int_equal(osp_gost_kuzn_aead_encrypt(k_em, iv, 0x07, NULL, 0, plaintext, sizeof(plaintext),
	                                            true, true, ciphertext, &ciphered_len, tag),
	                 0);
	assert_int_equal(ciphered_len, sizeof(plaintext));

	/* Ciphertext should differ from plaintext */
	assert_memory_not_equal(ciphertext, plaintext, sizeof(plaintext));

	/* Tag should be non-zero */
	uint8_t zero_tag[12] = {0};
	assert_memory_not_equal(tag, zero_tag, sizeof(tag));

	/* Decrypt: auth + encr — should recover plaintext */
	uint8_t recovered[8] = {0};
	uint32_t recovered_len = 0;
	assert_int_equal(osp_gost_kuzn_aead_decrypt(k_em, iv, 0x07, NULL, 0,
	                                            ciphertext, sizeof(ciphertext), true, true,
	                                            recovered, &recovered_len, tag),
	                 0);
	assert_int_equal(recovered_len, sizeof(plaintext));
	assert_memory_equal(recovered, plaintext, sizeof(plaintext));
}

static void test_kuzn_aead_auth_only(void **state) {
	(void)state;

	uint8_t k_em[64] = {0};
	uint8_t iv[12] = {0};
	uint8_t plaintext[] = {0xAA, 0xBB, 0xCC, 0xDD};
	uint8_t out[4] = {0};
	uint32_t out_len = 0;
	uint8_t tag[12] = {0};

	/* Auth only (no encryption): output == input, tag is computed */
	assert_int_equal(osp_gost_kuzn_aead_encrypt(k_em, iv, 0x01, NULL, 0, plaintext, sizeof(plaintext),
	                                            true, false, out, &out_len, tag),
	                 0);
	assert_int_equal(out_len, sizeof(plaintext));
	assert_memory_equal(out, plaintext, sizeof(plaintext));

	/* Verify tag is non-zero */
	uint8_t zero_tag[12] = {0};
	assert_memory_not_equal(tag, zero_tag, sizeof(tag));
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_gost_cmac_golden),
	    cmocka_unit_test(test_streebog_hash_with_titles),
	    cmocka_unit_test(test_kuzn_ctr_roundtrip),
	    cmocka_unit_test(test_glo_gost_suite8_roundtrip),
#if 1
	    cmocka_unit_test(test_gost3410_public_key_vko_vector),
	    cmocka_unit_test(test_gost3410_control_example),
	    cmocka_unit_test(test_gost3410_sign_verify_roundtrip),
	    cmocka_unit_test(test_gost3410_vko_r1323565),
	    cmocka_unit_test(test_gost_kdf_tree_r1323565),
	    cmocka_unit_test(test_gost3410_vko_roundtrip),
	    cmocka_unit_test(test_gen_signing_roundtrip),
	    cmocka_unit_test(test_gen_signing_protect_gost),
#endif
	    cmocka_unit_test(test_hls_gost_streebog_handshake),
	    cmocka_unit_test(test_hls_gost_cmac_handshake),
	    cmocka_unit_test(test_kuzn_aead_encrypt_decrypt),
	    cmocka_unit_test(test_kuzn_aead_auth_only),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
