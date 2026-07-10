#ifndef OSP_IC_GSM_DIAGNOSTIC_H
#define OSP_IC_GSM_DIAGNOSTIC_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	char operator_name[OSP_MAX_NAME_LEN];
	uint8_t operator_len;
	uint8_t status;
	uint8_t cs_attachment;
	uint8_t ps_status;
	osp_datetime_t capture_time;
} osp_ic_gsm_diagnostic_t;

void osp_ic_gsm_diagnostic_init(osp_ic_gsm_diagnostic_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_gsm_diagnostic_class(void);
#endif
