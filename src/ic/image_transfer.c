#include "image_transfer.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t img_attrs[] = {1, 2, 5, 6};

static osp_err_t img_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_image_transfer_t *i = (const osp_ic_image_transfer_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &i->logical_name);
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

static osp_err_t img_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_image_transfer_t *i = (osp_ic_image_transfer_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			i->image_block_size = osp_get_u32(value);
			return OSP_OK;
		case 5:
			i->image_transfer_enabled = osp_get_bool(value);
			return OSP_OK;
		case 6:
			i->image_transfer_status = (osp_image_transfer_status_t)osp_get_u8(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t img_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->execute) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, obis, method_id, param, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

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

static osp_err_t img_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_image_transfer_class(), inst, buf, img_attrs, 4);
}

static osp_err_t img_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_image_transfer_class(), inst, buf, img_attrs, 4);
}

static const osp_ic_class_t ic_img = {
    .name = "Image Transfer",
    .class_id = 18,
    .version = 0,
    .get_attr = img_get,
    .set_attr = img_set,
    .invoke = img_invoke,
    .serialize = img_serialize,
    .deserialize = img_deserialize,
    .instance_size = sizeof(osp_ic_image_transfer_t),
};

const osp_ic_class_t *osp_ic_image_transfer_class(void) {
	return &ic_img;
}

void osp_ic_image_transfer_init(osp_ic_image_transfer_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
