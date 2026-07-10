#ifndef OSP_GENERAL_CIPHERING_H
#define OSP_GENERAL_CIPHERING_H

#include "security.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint8_t transaction_id[32];
	uint32_t transaction_id_len;
	uint8_t originator_st[OSP_SEC_SYSTEM_TITLE_SIZE];
	uint8_t recipient_st[OSP_SEC_SYSTEM_TITLE_SIZE];
	uint32_t recipient_st_len;
	uint8_t date_time[32];
	uint32_t date_time_len;
	uint8_t other_information[64];
	uint32_t other_information_len;
	uint8_t content[OSP_GLO_MAX_PLAIN];
	uint32_t content_len;
	uint8_t signature[OSP_SEC_HLS_AUTH_MAX];
	uint32_t signature_len;
} osp_gen_signing_t;

int osp_gen_signing_encode(const osp_gen_signing_t *apdu, uint8_t *out, uint32_t *out_len);
int osp_gen_signing_decode(const uint8_t *apdu, uint32_t apdu_len, osp_gen_signing_t *decoded);

int osp_gen_signing_protect(const osp_sec_context_t *ctx, const uint8_t *transaction_id, uint32_t tx_id_len,
                            const uint8_t *recipient_st, uint32_t recipient_len, const uint8_t *content, uint32_t content_len,
                            uint8_t *out, uint32_t *out_len);
int osp_gen_signing_unprotect(osp_sec_context_t *ctx, const uint8_t *apdu, uint32_t apdu_len, uint8_t *content,
                              uint32_t *content_len);

#ifdef __cplusplus
}
#endif

#endif /* OSP_GENERAL_CIPHERING_H */
