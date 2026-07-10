#ifndef OSP_IC_TABLE_MANAGER_H
#define OSP_IC_TABLE_MANAGER_H
#include "../openspodes.h"
#include "../codec/structures.h"
#include "spodus_helpers.h"

#define OSP_TABLE_MANAGER_MAX_ROWS OSP_MAX_ARRAY_LEN

typedef struct {
	osp_obis_t logical_name;
	uint8_t key_index;
	osp_ic_row_t rows[OSP_TABLE_MANAGER_MAX_ROWS];
	uint8_t row_count;
	osp_value_t retrieve_buf[OSP_MAX_ARRAY_LEN];
} osp_ic_table_manager_t;

void osp_ic_table_manager_init(osp_ic_table_manager_t *i, osp_obis_t ln);
void osp_ic_table_manager_set_key_index(osp_ic_table_manager_t *i, uint8_t key_index);
const osp_ic_class_t *osp_ic_table_manager_class(void);

#endif
