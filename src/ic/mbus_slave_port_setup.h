#ifndef OSP_IC_MBUS_SLAVE_PORT_SETUP_H
#define OSP_IC_MBUS_SLAVE_PORT_SETUP_H

#include "../openspodes.h"

typedef struct {
	osp_obis_t logical_name;
	uint8_t default_baud;
	uint8_t avail_baud;
	uint8_t addr_state;
	uint8_t bus_address;
} osp_ic_mbus_slave_port_setup_t;

void osp_ic_mbus_slave_port_setup_init(osp_ic_mbus_slave_port_setup_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_mbus_slave_port_setup_class(void);

#endif
