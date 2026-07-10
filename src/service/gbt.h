#ifndef OSP_GBT_H
#define OSP_GBT_H

#include "../openspodes.h"
#include "service.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_TAG_GENERAL_BLOCK_TRANSFER 0xE0

typedef struct {
	bool last_block;
	bool streaming;
	uint8_t window;
	uint16_t block_number;
	uint16_t block_number_ack;
	uint8_t block_data[OSP_MAX_OCTET_LEN];
	uint32_t block_data_len;
} osp_general_block_transfer_t;

int osp_gbt_encode(osp_buf_t *buf, const osp_general_block_transfer_t *gbt);
int osp_gbt_decode(osp_buf_t *buf, osp_general_block_transfer_t *gbt);

#ifdef __cplusplus
}
#endif

#endif /* OSP_GBT_H */
