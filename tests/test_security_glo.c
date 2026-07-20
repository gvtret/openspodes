/**
 * test_security_glo.c — glo-ciphering unit tests (requires OpenSSL HAL)
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <cmocka.h>

#include "security/security.h"
#include "mock_crypto.h"

static void hex_to_bytes(const char *hex, uint8_t *out, uint32_t *out_len) {
	uint32_t n = 0;
	while (hex[0] && hex[1]) {
		char pair[3] = {hex[0], hex[1], 0};
		out[n++] = (uint8_t)strtoul(pair, NULL, 16);
		hex += 2;
	}
	*out_len = n;
}

static void fill_e5_ctx(osp_sec_context_t *ctx) {
	uint8_t ek[16], ak[16], st[8];
	uint32_t len;
	hex_to_bytes("000102030405060708090A0B0C0D0E0F", ek, &len);
	hex_to_bytes("D0D1D2D3D4D5D6D7D8D9DADBDCDDDEDF", ak, &len);
	hex_to_bytes("4D4D4D0000BC614E", st, &len);
	osp_sec_context_init(ctx, OSP_SUITE_0, OSP_MECH_LOWEST, st);
	ctx->policy = OSP_POLICY_ENCR_AUTH;
	memcpy(ctx->guek, ek, 16);
	memcpy(ctx->gak, ak, 16);
	ctx->invocation_counter = 0x01234567;
}

static void test_glo_e5_vector(void **state) {
	(void)state;
	mock_crypto_init();
	mock_crypto_init_real_gcm();
	if (!osp_hal_gcm_crypt) {
		skip();
	}

	osp_sec_context_t ctx;
	fill_e5_ctx(&ctx);

	uint8_t plain[32];
	uint32_t plain_len;
	hex_to_bytes("0800065F1F0400007C1F04000007", plain, &plain_len);

	uint8_t out[64];
	uint32_t out_len = 0;
	assert_int_equal(osp_glo_protect(&ctx, OSP_GLO_INITIATE_RESPONSE, plain, plain_len, out, &out_len), 0);

	const uint8_t expected[] = {0x28, 0x1F, 0x30, 0x01, 0x23, 0x45, 0x67, 0x89, 0x12, 0x14, 0xA0, 0x84, 0x5E, 0x47, 0x57,
	                            0x14, 0x38, 0x3F, 0x65, 0xBC, 0x19, 0x74, 0x5C, 0xA2, 0x35, 0x90, 0x65, 0x25, 0xE4, 0xF3, 0xE1, 0xC8, 0x93};
	assert_int_equal(out_len, sizeof(expected));
	assert_memory_equal(out, expected, sizeof(expected));

	uint8_t recovered[32];
	uint32_t recovered_len = 0;
	osp_sec_context_t rx;
	fill_e5_ctx(&rx);
	assert_int_equal(osp_glo_unprotect(&rx, out, out_len, recovered, &recovered_len), 0);
	assert_int_equal(recovered_len, plain_len);
	assert_memory_equal(recovered, plain, plain_len);
}

static void test_glo_peer_iv_decrypt(void **state) {
	(void)state;
	mock_crypto_init();
	mock_crypto_init_real_gcm();
	if (!osp_hal_gcm_crypt) {
		skip();
	}

	osp_sec_context_t tx, rx;
	fill_e5_ctx(&tx);
	fill_e5_ctx(&rx);
	tx.invocation_counter = 1;
	const uint8_t client_st[8] = {'T', 'S', 'T', '1', '2', '3', '4', '5'};
	memcpy(tx.system_title, client_st, 8);
	memcpy(rx.peer_system_title, client_st, 8);
	tx.policy = OSP_POLICY_ENCR_AUTH;

	const uint8_t plain[] = {0xC3, 0x01, 0xC1, 0x00, 0x0F, 0x00, 0x00, 0x28, 0x00, 0x00, 0xFF, 0x01, 0x01, 0x09, 0x11, 0x10};
	uint8_t ciphered[128];
	uint32_t ciphered_len = 0;
	assert_int_equal(osp_glo_protect(&tx, OSP_GLO_ACTION_REQUEST, plain, sizeof(plain), ciphered, &ciphered_len), 0);

	uint8_t recovered[128];
	uint32_t recovered_len = 0;
	assert_int_equal(osp_glo_unprotect(&rx, ciphered, ciphered_len, recovered, &recovered_len), 0);
	assert_int_equal(recovered_len, sizeof(plain));
	assert_memory_equal(recovered, plain, sizeof(plain));
}

static void test_glo_get_roundtrip(void **state) {
	(void)state;
	mock_crypto_init();
	mock_crypto_init_real_gcm();
	if (!osp_hal_gcm_crypt) {
		skip();
	}

	osp_sec_context_t tx, rx;
	fill_e5_ctx(&tx);
	fill_e5_ctx(&rx);
	tx.invocation_counter = 1;

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

static void test_ded_get_roundtrip(void **state) {
	(void)state;
	mock_crypto_init();
	mock_crypto_init_real_gcm();
	if (!osp_hal_gcm_crypt) {
		skip();
	}

	static const uint8_t dek[16] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF};

	osp_sec_context_t tx, rx;
	fill_e5_ctx(&tx);
	fill_e5_ctx(&rx);
	tx.invocation_counter = 1;
	assert_int_equal(osp_sec_cipher_apply_dedicated_key(&tx, dek, 16), 0);
	assert_int_equal(osp_sec_cipher_apply_dedicated_key(&rx, dek, 16), 0);

	const uint8_t plain[] = {0xC0, 0x01, 0xC1, 0x00, 0x08, 0x00, 0x00, 0x01, 0x00, 0x00, 0xFF, 0x02, 0x00};
	uint8_t ciphered[128];
	uint32_t ciphered_len = 0;
	assert_int_equal(osp_glo_protect(&tx, osp_svc_cipher_tag_for_plain(&tx, plain[0]), plain, sizeof(plain), ciphered, &ciphered_len), 0);
	assert_int_equal(ciphered[0], OSP_DED_GET_REQUEST);

	uint8_t recovered[128];
	uint32_t recovered_len = 0;
	assert_int_equal(osp_glo_unprotect(&rx, ciphered, ciphered_len, recovered, &recovered_len), 0);
	assert_int_equal(recovered_len, sizeof(plain));
	assert_memory_equal(recovered, plain, sizeof(plain));
}

static void test_gen_ciphering_roundtrip(void **state) {
	(void)state;
	mock_crypto_init();
	mock_crypto_init_real_gcm();
	if (!osp_hal_gcm_crypt) {
		skip();
	}

	osp_sec_context_t ctx;
	fill_e5_ctx(&ctx);

	/* Transaction ID and recipient system title */
	uint8_t tx_id[] = {0xDE, 0xAD, 0xBE, 0xEF};
	uint8_t recipient_st[] = {0x4D, 0x4D, 0x4D, 0x00, 0x00, 0xBC, 0x61, 0x4E};

	const uint8_t plain[] = {0xC0, 0x01, 0xC1, 0x00, 0x08, 0x00, 0x00, 0x01, 0x00, 0x00, 0xFF, 0x02, 0x00};
	uint8_t ciphered[256];
	uint32_t ciphered_len = 0;

	/* Protect */
	assert_int_equal(osp_gen_ciphering_protect(&ctx, tx_id, sizeof(tx_id), recipient_st, sizeof(recipient_st),
	                                            plain[0], plain, sizeof(plain), ciphered, &ciphered_len),
	                 0);
	assert_true(ciphered_len > sizeof(plain)); /* must be larger due to envelope */

	/* Unprotect */
	uint8_t recovered[256];
	uint32_t recovered_len = 0;
	uint8_t recovered_tag = 0;
	assert_int_equal(osp_gen_ciphering_unprotect(&ctx, ciphered, ciphered_len, recovered, &recovered_len, &recovered_tag), 0);
	assert_int_equal(recovered_len, sizeof(plain));
	assert_memory_equal(recovered, plain, sizeof(plain));
	assert_int_equal(recovered_tag, plain[0]);
}

int main(void) {
	const struct CMUnitTest tests[] = {
	    cmocka_unit_test(test_glo_e5_vector),
	    cmocka_unit_test(test_glo_peer_iv_decrypt),
	    cmocka_unit_test(test_glo_get_roundtrip),
	    cmocka_unit_test(test_ded_get_roundtrip),
	    cmocka_unit_test(test_gen_ciphering_roundtrip),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
