#ifndef OSP_IC_GPRS_MODEM_H
#define OSP_IC_GPRS_MODEM_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
} osp_ic_gprs_modem_t;

void osp_ic_gprs_modem_init(osp_ic_gprs_modem_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_gprs_modem_class(void);
#endif
