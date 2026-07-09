#ifndef OSP_IC_ARBITRATOR_H
#define OSP_IC_ARBITRATOR_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_arbitrator_actions_t actions;
	osp_arbitrator_actions_t permissions_table;
	osp_arbitrator_actions_t weightings_table;
	osp_arbitrator_actions_t most_recent_requests_table;
	uint8_t last_outcome;
} osp_ic_arbitrator_t;

void osp_ic_arbitrator_init(osp_ic_arbitrator_t *a, osp_obis_t ln);
const osp_ic_class_t *osp_ic_arbitrator_class(void);
#endif
