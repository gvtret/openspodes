#include "arbitrator.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t arb_attrs[] = {1, 2, 3, 4, 5, 6};

static osp_err_t arb_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	const osp_ic_arbitrator_t *a = (const osp_ic_arbitrator_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &a->logical_name);
		case 2:
		case 3:
		case 4:
		case 5:
			*result = osp_ic_val_empty_array();
			return OSP_OK;
		case 6:
			*result = osp_val_u8(a->last_outcome);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t arb_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	osp_ic_arbitrator_t *a = (osp_ic_arbitrator_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
		case 3:
		case 4:
		case 5:
			return OSP_OK;
		case 6:
			a->last_outcome = osp_get_u8(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t arb_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	osp_ic_arbitrator_t *a = (osp_ic_arbitrator_t *)inst;
	(void)param;
	*result = osp_val_null();
	if (method_id == 2) {
		a->last_outcome = 0;
		return OSP_OK;
	}
	return (method_id == 1) ? OSP_OK : OSP_ERR_UNSUPPORTED;
}

static osp_err_t arb_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_arbitrator_class(), inst, buf, arb_attrs, 6);
}

static osp_err_t arb_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_arbitrator_class(), inst, buf, arb_attrs, 6);
}

static const osp_ic_class_t ic_arb = {
    .name = "Arbitrator",
    .class_id = 68,
    .version = 0,
    .get_attr = arb_get,
    .set_attr = arb_set,
    .invoke = arb_invoke,
    .serialize = arb_serialize,
    .deserialize = arb_deserialize,
    .instance_size = sizeof(osp_ic_arbitrator_t),
};

const osp_ic_class_t *osp_ic_arbitrator_class(void) {
	return &ic_arb;
}

void osp_ic_arbitrator_init(osp_ic_arbitrator_t *a, osp_obis_t ln) {
	memset(a, 0, sizeof(*a));
	a->logical_name = ln;
}
