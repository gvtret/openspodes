/**
 * mbus_slave_port_setup.c — M-Bus slave port setup (class 25, IEC 62056-6-2 §4.8.2)
 */

#include "mbus_slave_port_setup.h"
#include "ic_common.h"
#include "../codec/serialize.h"
#include <string.h>
#include "../data_hal.h"

static const uint8_t mbus_attrs[] = {1, 2, 3, 4, 5};

static osp_err_t mbus_get_attr(const void *inst, uint8_t attr_id, osp_value_t *result) {
	if (osp_hal_data && osp_hal_data->read) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, attr_id, result);
		if (r == OSP_OK) return OSP_OK;
		if (r != OSP_ERR_NOT_FOUND) return r;
	}

	const osp_ic_mbus_slave_port_setup_t *m = (const osp_ic_mbus_slave_port_setup_t *)inst;
	if (!result) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 1:
			return osp_ic_get_logical_name(result, &m->logical_name);
		case 2:
			*result = osp_val_enum(m->default_baud);
			return OSP_OK;
		case 3:
			*result = osp_val_enum(m->avail_baud);
			return OSP_OK;
		case 4:
			*result = osp_val_enum(m->addr_state);
			return OSP_OK;
		case 5:
			*result = osp_val_u8(m->bus_address);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t mbus_set_attr(void *inst, uint8_t attr_id, const osp_value_t *value) {
	if (osp_hal_data && osp_hal_data->write) {
		const osp_obis_t *obis = (const osp_obis_t *)inst;
		osp_err_t r = osp_hal_data->write(osp_hal_data->ctx, obis, attr_id, value);
		if (r != OSP_OK && r != OSP_ERR_NOT_FOUND) return r;
	}

	osp_ic_mbus_slave_port_setup_t *m = (osp_ic_mbus_slave_port_setup_t *)inst;
	if (!value) {
		return OSP_ERR_INVALID;
	}
	switch (attr_id) {
		case 2:
			m->default_baud = osp_get_enum(value);
			return OSP_OK;
		case 3:
			m->avail_baud = osp_get_enum(value);
			return OSP_OK;
		case 4:
			m->addr_state = osp_get_enum(value);
			return OSP_OK;
		case 5:
			m->bus_address = osp_get_u8(value);
			return OSP_OK;
		default:
			return OSP_ERR_NOT_FOUND;
	}
}

static osp_err_t mbus_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_mbus_slave_port_setup_class(), inst, buf, mbus_attrs, 5);
}

static osp_err_t mbus_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_mbus_slave_port_setup_class(), inst, buf, mbus_attrs, 5);
}

static const osp_ic_class_t ic_mbus_slave_port_setup = {
    .name = "M-Bus Slave Port Setup",
    .class_id = 25,
    .version = 0,
    .get_attr = mbus_get_attr,
    .set_attr = mbus_set_attr,
    .invoke = NULL,
    .serialize = mbus_serialize,
    .deserialize = mbus_deserialize,
    .instance_size = sizeof(osp_ic_mbus_slave_port_setup_t),
};

const osp_ic_class_t *osp_ic_mbus_slave_port_setup_class(void) {
	return &ic_mbus_slave_port_setup;
}

void osp_ic_mbus_slave_port_setup_init(osp_ic_mbus_slave_port_setup_t *i, osp_obis_t ln) {
	if (!i) {
		return;
	}
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
