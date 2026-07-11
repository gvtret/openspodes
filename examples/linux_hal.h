/**
 * linux_hal.h — Linux HAL for OpenSPODES
 *
 * Provides all HAL interfaces needed for production use:
 *   - TCP transport with wrapper/HDLC/no-framing modes
 *   - AES-GCM crypto via OpenSSL
 *   - MD5/SHA1/SHA256 hashes via OpenSSL
 *   - Timer (clock_gettime)
 *   - Random (/dev/urandom)
 *   - System title + key store
 *
 * Usage:
 *   osp_hal_t hal;
 *   linux_hal_init(&hal);
 *   linux_hal_set_tcp(&hal, "192.168.1.100", 4059);
 *   // Use hal.transport, hal.crypto, etc.
 */

#ifndef OSP_LINUX_HAL_H
#define OSP_LINUX_HAL_H

#include "../src/openspodes.h"
#include "../src/transport/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_LINUX_HAL_MAX_KEYS 16
#define OSP_LINUX_HAL_MAX_KEY_LEN 32

/* Key store entry */
typedef struct {
	uint8_t sap;       /* SAP index */
	uint8_t key_id;    /* Key ID */
	uint8_t key[OSP_LINUX_HAL_MAX_KEY_LEN];
	uint8_t key_len;
} linux_hal_key_entry_t;

/* TCP transport context */
typedef struct {
	int fd;
	uint16_t wrapper_source;
	uint16_t wrapper_dest;
	osp_framing_type_t framing;
} linux_hal_tcp_ctx_t;

/* System title + key store */
typedef struct {
	uint8_t system_title[8];
	linux_hal_key_entry_t keys[OSP_LINUX_HAL_MAX_KEYS];
	uint8_t key_count;
} linux_hal_system_ctx_t;

/* Complete Linux HAL */
typedef struct {
	osp_transport_t transport;
	osp_crypto_t crypto;
	osp_random_t random;
	osp_timer_t timer;
	osp_system_t system;

	linux_hal_tcp_ctx_t tcp_ctx;
	linux_hal_system_ctx_t sys_ctx;
} linux_hal_t;

/* Initialize all HAL interfaces */
void linux_hal_init(osp_hal_t *hal);

/* Configure TCP transport (wrapper mode by default) */
void linux_hal_set_tcp(osp_hal_t *hal, const char *host, uint16_t port);

/* Configure TCP transport with HDLC framing */
void linux_hal_set_tcp_hdlc(osp_hal_t *hal, const char *host, uint16_t port,
                             uint32_t client_addr, uint32_t server_addr);

/* Configure system title */
void linux_hal_set_system_title(osp_hal_t *hal, const uint8_t title[8]);

/* Add a key to the key store */
void linux_hal_add_key(osp_hal_t *hal, uint8_t sap, uint8_t key_id,
                        const uint8_t *key, uint8_t key_len);

/* Initialize OpenSSL crypto (call once at startup) */
void linux_hal_init_crypto(void);

#ifdef __cplusplus
}
#endif

#endif /* OSP_LINUX_HAL_H */
