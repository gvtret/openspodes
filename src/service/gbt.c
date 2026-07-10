#include "gbt.h"
#include "../codec/codec.h"

#define OSP_GBT_LAST_BLOCK 0x80u
#define OSP_GBT_STREAMING  0x40u
#define OSP_GBT_WINDOW_MASK 0x3Fu

int osp_gbt_encode(osp_buf_t *buf, const osp_general_block_transfer_t *gbt) {
	if (!buf || !gbt) {
		return -1;
	}
	uint8_t ctrl = (gbt->last_block ? OSP_GBT_LAST_BLOCK : 0) | (gbt->streaming ? OSP_GBT_STREAMING : 0) |
	               (gbt->window & OSP_GBT_WINDOW_MASK);
	osp_axdr_write_u8(buf, OSP_TAG_GENERAL_BLOCK_TRANSFER);
	osp_axdr_write_u8(buf, ctrl);
	osp_axdr_write_u16(buf, gbt->block_number);
	osp_axdr_write_u16(buf, gbt->block_number_ack);
	if (osp_axdr_push_length(buf, gbt->block_data_len) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < gbt->block_data_len; i++) {
		osp_axdr_write_u8(buf, gbt->block_data[i]);
	}
	return 0;
}

int osp_gbt_decode(osp_buf_t *buf, osp_general_block_transfer_t *gbt) {
	if (!buf || !gbt) {
		return -1;
	}
	uint8_t tag, ctrl;
	if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_GENERAL_BLOCK_TRANSFER) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &ctrl) != OSP_OK) {
		return -1;
	}
	gbt->last_block = (ctrl & OSP_GBT_LAST_BLOCK) != 0;
	gbt->streaming = (ctrl & OSP_GBT_STREAMING) != 0;
	gbt->window = ctrl & OSP_GBT_WINDOW_MASK;
	if (osp_axdr_read_u16(buf, &gbt->block_number) != OSP_OK) {
		return -1;
	}
	if (osp_axdr_read_u16(buf, &gbt->block_number_ack) != OSP_OK) {
		return -1;
	}
	uint32_t len;
	if (osp_axdr_read_length(buf, &len) != 0 || len > OSP_MAX_OCTET_LEN) {
		return -1;
	}
	gbt->block_data_len = len;
	for (uint32_t i = 0; i < len; i++) {
		osp_axdr_read_u8(buf, &gbt->block_data[i]);
	}
	return 0;
}
