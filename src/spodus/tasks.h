#ifndef OSP_SPODUS_TASKS_H
#define OSP_SPODUS_TASKS_H

#include "meter_registry.h"

#define OSP_SPODUS_MAX_TASKS 16
#define OSP_SPODUS_MAX_TASK_SCRIPTS 8

typedef struct {
	uint8_t service_id, class_id, attribute_id;
	osp_obis_t obis;
} osp_spodus_task_script_t;

typedef struct {
	uint16_t task_id;
	uint8_t meter_id_len, meter_id[OSP_SPODUS_MAX_METER_ID_LEN];
	uint8_t script_count, execution_type;
	osp_spodus_task_script_t scripts[OSP_SPODUS_MAX_TASK_SCRIPTS];
	uint16_t priority;
} osp_spodus_exchange_task_t;

typedef struct {
	osp_spodus_exchange_task_t entries[OSP_SPODUS_MAX_TASKS];
	uint8_t count;
	osp_value_t value, rows[OSP_SPODUS_MAX_TASKS], fields[OSP_SPODUS_MAX_TASKS][6];
	osp_value_t scripts[OSP_SPODUS_MAX_TASKS], script_rows[OSP_SPODUS_MAX_TASKS][OSP_SPODUS_MAX_TASK_SCRIPTS];
	osp_value_t script_fields[OSP_SPODUS_MAX_TASKS][OSP_SPODUS_MAX_TASK_SCRIPTS][7];
} osp_spodus_exchange_tasks_t;

void osp_spodus_exchange_tasks_init(osp_spodus_exchange_tasks_t *tasks);
osp_err_t osp_spodus_exchange_tasks_add(osp_spodus_exchange_tasks_t *tasks, const osp_spodus_exchange_task_t *task);
osp_err_t osp_spodus_exchange_tasks_build_value(const osp_spodus_exchange_tasks_t *tasks, osp_value_t *out);
#endif
