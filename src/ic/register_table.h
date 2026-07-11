#ifndef OSP_IC_REGISTER_TABLE_H
#define OSP_IC_REGISTER_TABLE_H
#include "../openspodes.h"
#include "../codec/structures.h"

/**
 * @file register_table.h
 * @brief Register Table interface class (class_id = 61, IEC 62056-6-2)
 *
 * @par Attributes
 * | # | Name                    | Type              | Access |
 * |---|-------------------------|-------------------|--------|
 * | 1 | logical_name            | octet-string      | RO     |
 * | 2 | table_cell_values       | compact-array     | RO     |
 * | 3 | table_cell_definition   | structure         | RO     |
 * | 4 | scaler_unit             | scal_unit_type    | RO     |
 *
 * @par Methods
 * | # | Name   | Argument | Description                              |
 * |---|--------|----------|------------------------------------------|
 * | 1 | reset  | null     | Resets the register table                |
 * | 2 | capture| null     | Captures current values into the table   |
 */

typedef struct {
	osp_obis_t logical_name;
	osp_value_t table_cell_values[OSP_MAX_REGISTER_TABLE];
	uint8_t table_cell_count;
	osp_table_cell_t table_cell_definition;
	osp_scaler_unit_t scaler_unit;
} osp_ic_register_table_t;

void osp_ic_register_table_init(osp_ic_register_table_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_register_table_class(void);
#endif
