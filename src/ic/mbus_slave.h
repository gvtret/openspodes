#ifndef OSP_IC_MBUS_SLAVE_H
#define OSP_IC_MBUS_SLAVE_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
} osp_ic_mbus_slave_t;

void osp_ic_mbus_slave_init(osp_ic_mbus_slave_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_mbus_slave_class(void);
#endif
