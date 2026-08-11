#include "image_transfer.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t img_attrs[] = {1, 2, 3, 4, 5, 6, 7};

static osp_value_t img_blocks_status_val(const osp_ic_image_transfer_t *i) {
	osp_value_t v = {0};
	v.tag = OSP_TAG_BITSTRING;
	/* 64 bytes of status → up to 512 bits; osp_value bitstring caps at OSP_MAX_BITSTRING_LEN. */
	v.as.bitstring.num_bits = OSP_MAX_BITSTRING_LEN * 8u;
	memcpy(v.as.bitstring.bits, i->image_transferred_blocks_status,
	       OSP_MAX_BITSTRING_LEN < 64 ? OSP_MAX_BITSTRING_LEN : 64);
	return v;
}

static osp_value_t img_to_activate_val(const osp_ic_image_transfer_t *i) {
	osp_value_t v = {0};
	uint8_t n = i->image_to_activate_count;
	if (n > OSP_MAX_IMAGE_TO_ACTIVATE) {
		n = OSP_MAX_IMAGE_TO_ACTIVATE;
	}
	if (n == 0) {
		return osp_ic_val_empty_array();
	}
	/* One structure per image: { signature, size, date, identification } — use scratch slots. */
	osp_value_t *scratch = osp_ic_val_scratch_buf();
	/* Layout: [0..n) structures, then fields starting at n. Need 1 + 4*n slots; n<=3 with scratch=16. */
	if (n > 3) {
		n = 3;
	}
	osp_value_t *rows = scratch;
	osp_value_t *fields = &scratch[n];
	for (uint8_t k = 0; k < n; k++) {
		const osp_image_info_t *info = &i->image_to_activate[k];
		osp_value_t *f = &fields[k * 4];
		f[0].tag = OSP_TAG_OCTETSTRING;
		f[0].as.octetstring.len = info->signature_len;
		if (f[0].as.octetstring.len > OSP_MAX_OCTET_LEN) {
			f[0].as.octetstring.len = OSP_MAX_OCTET_LEN;
		}
		if (f[0].as.octetstring.len > sizeof(info->signature)) {
			f[0].as.octetstring.len = sizeof(info->signature);
		}
		memcpy(f[0].as.octetstring.data, info->signature, f[0].as.octetstring.len);
		f[1] = osp_val_u32(info->size);
		f[2].tag = OSP_TAG_OCTETSTRING;
		f[2].as.octetstring.len = 5;
		memcpy(f[2].as.octetstring.data, info->date, 5);
		f[3] = osp_val_u32(info->actual);
		rows[k].tag = OSP_TAG_STRUCTURE;
		rows[k].as.structure.elements.items = f;
		rows[k].as.structure.elements.count = 4;
		rows[k].as.structure.elements.capacity = 4;
	}
	v.tag = OSP_TAG_ARRAY;
	v.as.array.elements.items = rows;
	v.as.array.elements.count = n;
	v.as.array.elements.capacity = n;
	return v;
}

static osp_err_t img_read_to_activate(const osp_value_t *value, osp_ic_image_transfer_t *i) {
	if (!value || value->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = value->as.array.elements.count;
	if (n > OSP_MAX_IMAGE_TO_ACTIVATE) {
		n = OSP_MAX_IMAGE_TO_ACTIVATE;
	}
	for (uint8_t k = 0; k < n; k++) {
		const osp_value_t *row = &value->as.array.elements.items[k];
		if (row->tag != OSP_TAG_STRUCTURE || row->as.structure.elements.count < 3) {
			return OSP_ERR_INVALID;
		}
		osp_image_info_t *info = &i->image_to_activate[k];
		memset(info, 0, sizeof(*info));
		const osp_value_t *f = row->as.structure.elements.items;
		if (f[0].tag == OSP_TAG_OCTETSTRING) {
			info->signature_len = f[0].as.octetstring.len;
			if (info->signature_len > sizeof(info->signature)) {
				info->signature_len = sizeof(info->signature);
			}
			memcpy(info->signature, f[0].as.octetstring.data, info->signature_len);
		}
		info->size = osp_get_u32(&f[1]);
		if (f[2].tag == OSP_TAG_OCTETSTRING && f[2].as.octetstring.len >= 5) {
			memcpy(info->date, f[2].as.octetstring.data, 5);
		}
		if (row->as.structure.elements.count >= 4) {
			info->actual = osp_get_u32(&f[3]);
		}
	}
	i->image_to_activate_count = n;
	return OSP_OK;
}

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
		case 3:
			*result = img_blocks_status_val(i);
			return OSP_OK;
		case 4:
			*result = osp_val_u32(i->image_first_not_transferred);
			return OSP_OK;
		case 5:
			*result = osp_val_bool(i->image_transfer_enabled);
			return OSP_OK;
		case 6:
			*result = osp_val_u8((uint8_t)i->image_transfer_status);
			return OSP_OK;
		case 7:
			*result = img_to_activate_val(i);
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
		case 3:
			if (value->tag != OSP_TAG_BITSTRING) {
				return OSP_ERR_INVALID;
			}
			{
				uint32_t nbytes = (value->as.bitstring.num_bits + 7u) / 8u;
				if (nbytes > sizeof(i->image_transferred_blocks_status)) {
					nbytes = sizeof(i->image_transferred_blocks_status);
				}
				memset(i->image_transferred_blocks_status, 0, sizeof(i->image_transferred_blocks_status));
				memcpy(i->image_transferred_blocks_status, value->as.bitstring.bits, nbytes);
			}
			return OSP_OK;
		case 4:
			i->image_first_not_transferred = osp_get_u32(value);
			return OSP_OK;
		case 5:
			i->image_transfer_enabled = osp_get_bool(value);
			return OSP_OK;
		case 6:
			i->image_transfer_status = (osp_image_transfer_status_t)osp_get_u8(value);
			return OSP_OK;
		case 7:
			return img_read_to_activate(value, i);
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

	(void)inst;
	(void)param;
	(void)method_id;
	*result = osp_val_null();
	/* Block store / verify / activate require a signed image pipeline — not stubbed as success. */
	return OSP_ERR_UNSUPPORTED;
}

static osp_err_t img_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_image_transfer_class(), inst, buf, img_attrs, 7);
}

static osp_err_t img_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_image_transfer_class(), inst, buf, img_attrs, 7);
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
