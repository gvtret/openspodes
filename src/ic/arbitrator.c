#include "arbitrator.h"
#include "ic_common.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t arb_attrs[] = {1, 2, 3, 4, 5, 6};

static osp_value_t arb_val_bitstring_array(const osp_arbitrator_actions_t *a) {
	osp_value_t v = {0};
	if (!a || a->group_count == 0) {
		return osp_ic_val_empty_array();
	}
	osp_value_t *items = osp_ic_val_scratch_buf();
	uint8_t n = a->group_count;
	if (n > OSP_MAX_ARBITRATOR_ACTIONS) {
		n = OSP_MAX_ARBITRATOR_ACTIONS;
	}
	if (n > OSP_IC_VAL_SCRATCH_LEN) {
		n = OSP_IC_VAL_SCRATCH_LEN;
	}
	for (uint8_t i = 0; i < n; i++) {
		uint32_t bits = a->group_bits[i];
		uint32_t nbytes = (bits + 7u) / 8u;
		if (nbytes > OSP_MAX_BITSTRING_LEN) {
			nbytes = OSP_MAX_BITSTRING_LEN;
			bits = nbytes * 8u;
		}
		items[i].tag = OSP_TAG_BITSTRING;
		items[i].as.bitstring.num_bits = bits;
		memset(items[i].as.bitstring.bits, 0, sizeof(items[i].as.bitstring.bits));
		memcpy(items[i].as.bitstring.bits, a->groups[i], nbytes);
	}
	v.tag = OSP_TAG_ARRAY;
	v.as.array.elements.items = items;
	v.as.array.elements.count = n;
	v.as.array.elements.capacity = n;
	return v;
}

static osp_err_t arb_read_bitstring_array(const osp_value_t *value, osp_arbitrator_actions_t *a) {
	if (!value || !a || value->tag != OSP_TAG_ARRAY) {
		return OSP_ERR_INVALID;
	}
	uint8_t n = value->as.array.elements.count;
	if (n > OSP_MAX_ARBITRATOR_ACTIONS) {
		n = OSP_MAX_ARBITRATOR_ACTIONS;
	}
	memset(a, 0, sizeof(*a));
	for (uint8_t i = 0; i < n; i++) {
		const osp_value_t *bs = &value->as.array.elements.items[i];
		if (bs->tag != OSP_TAG_BITSTRING) {
			return OSP_ERR_INVALID;
		}
		uint32_t bits = bs->as.bitstring.num_bits;
		uint32_t nbytes = (bits + 7u) / 8u;
		if (nbytes > 32) {
			nbytes = 32;
			bits = nbytes * 8u;
		}
		a->group_bits[i] = (uint8_t)(bits > 255 ? 255 : bits);
		memcpy(a->groups[i], bs->as.bitstring.bits, nbytes);
	}
	a->group_count = n;
	return OSP_OK;
}

static const osp_arbitrator_actions_t *arb_table_for_attr(const osp_ic_arbitrator_t *a, uint8_t attr_id) {
	switch (attr_id) {
		case 2:
			return &a->actions;
		case 3:
			return &a->permissions_table;
		case 4:
			return &a->weightings_table;
		case 5:
			return &a->most_recent_requests_table;
		default:
			return NULL;
	}
}

static osp_arbitrator_actions_t *arb_table_mut(osp_ic_arbitrator_t *a, uint8_t attr_id) {
	switch (attr_id) {
		case 2:
			return &a->actions;
		case 3:
			return &a->permissions_table;
		case 4:
			return &a->weightings_table;
		case 5:
			return &a->most_recent_requests_table;
		default:
			return NULL;
	}
}

static osp_err_t arb_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_arbitrator_t *a = (const osp_ic_arbitrator_t *)inst;
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &a->logical_name);
		case 2:
		case 3:
		case 4:
		case 5:
			*result = arb_val_bitstring_array(arb_table_for_attr(a, attr_id));
			return OSP_OK;
		case 6:
			*result = osp_val_u8(a->last_outcome);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t arb_set(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_arbitrator_t *a = (osp_ic_arbitrator_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
		case 3:
		case 4:
		case 5:
			return arb_read_bitstring_array(value, arb_table_mut(a, attr_id));
		case 6:
			a->last_outcome = osp_get_u8(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t arb_invoke(void *inst, uint8_t method_id, const osp_value_t *param, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->execute) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->execute(osp_hal_data->ctx, obis, method_id, param, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

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
