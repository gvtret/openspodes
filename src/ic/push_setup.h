#ifndef OSP_IC_PUSH_SETUP_H
#define OSP_IC_PUSH_SETUP_H
#include "../openspodes.h"
#include "../codec/structures.h"

typedef struct {
	osp_obis_t logical_name;
	osp_push_object_t push_object_list[OSP_MAX_PUSH_OBJECTS];
	uint8_t push_object_count;
	osp_send_destination_t send_destination;
	osp_comm_window_t communication_window[OSP_MAX_COMM_WINDOW];
	uint8_t comm_window_count;
	uint16_t randomisation_start_interval;
	uint8_t number_of_retries;
	uint16_t repetition_delay;
} osp_ic_push_setup_t;

void osp_ic_push_setup_init(osp_ic_push_setup_t *p, osp_obis_t ln);
const osp_ic_class_t *osp_ic_push_setup_class(void);
#endif
