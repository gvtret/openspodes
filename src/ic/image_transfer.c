#include "image_transfer.h"
#include <string.h>

static osp_err_t img_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_image_transfer_t *i = (const osp_ic_image_transfer_t *)inst;
	switch (attr_id) {
		case 2:
			*result = osp_val_u32(i->image_block_size);
			return OSP_OK;
		case 5:
			*result = osp_val_bool(i->image_transfer_enabled);
			return OSP_OK;
		case 6:
			*result = osp_val_u8((uint8_t)i->image_transfer_status);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t img_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_image_transfer_t *i = (osp_ic_image_transfer_t *)inst;
	(void)param;
	*result = osp_val_null();
	switch (method_id) {
		case 1:
			i->image_transfer_status = OSP_IMAGE_TRANSFER_INITIATED;
			return OSP_OK;
		case 2:
			i->image_transfer_status = OSP_IMAGE_TRANSFER_RUNNING;
			return OSP_OK;
		case 3:
			i->image_transfer_status = OSP_IMAGE_VERIFICATION_OK;
			return OSP_OK;
		case 4:
			i->image_transfer_status = OSP_IMAGE_ACTIVATION_OK;
			return OSP_OK;
		default:
			return OSP_ERR_UNSUPPORTED;
	}
}

static const osp_ic_class_t ic_img = {
    .name = "Image Transfer",
    .class_id = 18,
    .version = 0,
    .get_attr = img_get,
    .set_attr = NULL,
    .invoke = img_invoke,
    .instance_size = sizeof(osp_ic_image_transfer_t)
};

const osp_ic_class_t *osp_ic_image_transfer_class(void) {
	return &ic_img;
}

void osp_ic_image_transfer_init(osp_ic_image_transfer_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
