#ifndef OSP_MOCK_CRYPTO_H
#define OSP_MOCK_CRYPTO_H

#include "../src/security/security.h"

void mock_crypto_init(void);
void mock_crypto_init_real_gcm(void);
void mock_crypto_init_real_hashes(void);
void mock_crypto_init_real_gost_ecdsa(void);

#endif
