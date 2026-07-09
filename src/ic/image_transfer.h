#ifndef OSP_IC_IMAGE_TRANSFER_H
#define OSP_IC_IMAGE_TRANSFER_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	uint32_t image_block_size;
	uint8_t image_transferred_blocks_status[64];
	uint32_t image_first_not_transferred;
	bool image_transfer_enabled;
	osp_image_transfer_status_t image_transfer_status;
	osp_image_info_t image_to_activate[OSP_MAX_IMAGE_TO_ACTIVATE];
	uint8_t image_to_activate_count;
} osp_ic_image_transfer_t;

void osp_ic_image_transfer_init(osp_ic_image_transfer_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_image_transfer_class(void);
#endif
