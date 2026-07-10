#include "mock_crypto.h"
#include <string.h>

/* Dummy GCM: XOR-based "tag" for testing only — NOT real crypto */
static uint8_t dummy_tag[12] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

static void mock_gcm_init(osp_sec_key_id key_id, uint32_t key_len, const uint8_t *iv, uint32_t iv_len, const uint8_t *aad, uint32_t aad_len) {
	(void)key_id;
	(void)key_len;
	(void)iv;
	(void)iv_len;
	(void)aad;
	(void)aad_len;
}

static void mock_gcm_update(const uint8_t *in, uint32_t len, uint8_t *out) {
	(void)in;
	(void)len;
	(void)out;
}

static void mock_gcm_finish(uint8_t *tag) {
	memcpy(tag, dummy_tag, 12);
}

void mock_crypto_init(void) {
	osp_hal_gcm_init = mock_gcm_init;
	osp_hal_gcm_update = mock_gcm_update;
	osp_hal_gcm_finish = mock_gcm_finish;
	osp_hal_gcm_crypt = NULL;
	osp_hal_md5 = NULL;
	osp_hal_sha1 = NULL;
	osp_hal_sha256 = NULL;
}
