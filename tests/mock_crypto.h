#ifndef OSP_MOCK_CRYPTO_H
#define OSP_MOCK_CRYPTO_H
#include "../src/security/security.h"
/* Initialize mock GCM HAL for testing (no real crypto, dummy tag) */
void mock_crypto_init(void);
#endif
