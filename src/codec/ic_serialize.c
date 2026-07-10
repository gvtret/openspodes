/**
 * ic_serialize.c — Whole-object BER serialization dispatch
 */

#include "ic_serialize.h"
#include "serialize.h"

osp_err_t osp_ic_write_object_header(osp_buf_t *buf, uint16_t class_id, const osp_obis_t *ln, uint8_t field_count) {
	if (!buf || !ln) {
		return OSP_ERR_INVALID;
	}

	osp_err_t r = osp_struct_begin(buf, field_count);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_value_write(buf, &((osp_value_t){.tag = OSP_TAG_LONG_UNSIGNED, .as.uint16 = {.value = class_id}}));
	if (r != OSP_OK) {
		return r;
	}
	return osp_obis_write(buf, ln);
}

osp_err_t osp_ic_serialize(const osp_ic_class_t *cls, const void *inst, osp_buf_t *buf) {
	if (!cls || !inst || !buf) {
		return OSP_ERR_INVALID;
	}
	if (!cls->serialize) {
		return OSP_ERR_UNSUPPORTED;
	}
	return cls->serialize(inst, buf);
}

osp_err_t osp_ic_deserialize(const osp_ic_class_t *cls, void *inst, osp_buf_t *buf) {
	if (!cls || !inst || !buf) {
		return OSP_ERR_INVALID;
	}
	if (!cls->deserialize) {
		return OSP_ERR_UNSUPPORTED;
	}
	return cls->deserialize(inst, buf);
}
