#include "../src/security/gost_crypto.h"
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

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_gost_cmac_golden),
	    cmocka_unit_test(test_streebog_hash_with_titles),
#if 1
	    cmocka_unit_test(test_gost3410_public_key_vko_vector),
	    cmocka_unit_test(test_gost3410_control_example),
	    cmocka_unit_test(test_gost3410_sign_verify_roundtrip),
	    cmocka_unit_test(test_gost3410_vko_r1323565),
	    cmocka_unit_test(test_gost_kdf_tree_r1323565),
	    cmocka_unit_test(test_gost3410_vko_roundtrip),
#endif
	    cmocka_unit_test(test_hls_gost_streebog_handshake),
	    cmocka_unit_test(test_hls_gost_cmac_handshake),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
