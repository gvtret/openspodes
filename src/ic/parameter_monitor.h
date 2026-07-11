#ifndef OSP_IC_PARAMETER_MONITOR_H
#define OSP_IC_PARAMETER_MONITOR_H
#include "../openspodes.h"
#include "../codec/structures.h"

/**
 * @file parameter_monitor.h
 * @brief Parameter Monitor interface class (class_id = 65, IEC 62056-6-2)
 *
 * @par Attributes
 * | # | Name             | Type              | Access |
 * |---|------------------|-------------------|--------|
 * | 1 | logical_name     | octet-string      | RO     |
 * | 2 | monitored_value  | value_definition  | RO     |
 * | 3 | thresholds       | structure         | RO     |
 * | 4 | events           | array             | RO     |
 * | 5 | minimal_duration | double-long-unsigned | RO  |
 *
 * @par Methods
 * None.
 */

#define OSP_MAX_MONITOR_EVENTS 8

typedef struct {
	osp_obis_t logical_name;
	osp_value_definition_t monitored_value;
	osp_threshold_list_t thresholds;
	osp_value_t events[OSP_MAX_MONITOR_EVENTS];
	uint8_t event_count;
	uint32_t minimal_duration;
} osp_ic_parameter_monitor_t;

void osp_ic_parameter_monitor_init(osp_ic_parameter_monitor_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_parameter_monitor_class(void);
#endif
