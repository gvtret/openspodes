#ifndef OSP_IC_PROFILE_DATA_FILTER_H
#define OSP_IC_PROFILE_DATA_FILTER_H

#include "../openspodes.h"
#include "spodus_helpers.h"

#define OSP_PROFILE_DATA_FILTER_MAX_COLUMNS 16
#define OSP_PROFILE_DATA_FILTER_MAX_ROWS    OSP_MAX_ARRAY_LEN

typedef struct {
	osp_obis_t logical_name;
	osp_obis_t columns[OSP_PROFILE_DATA_FILTER_MAX_COLUMNS];
	uint8_t column_count;
	osp_ic_row_t rows[OSP_PROFILE_DATA_FILTER_MAX_ROWS];
	uint8_t row_count;
	osp_value_t result_buf[OSP_MAX_ARRAY_LEN];
} osp_ic_profile_data_filter_t;

void osp_ic_profile_data_filter_init(osp_ic_profile_data_filter_t *f, osp_obis_t ln);
void osp_ic_profile_data_filter_set_columns(osp_ic_profile_data_filter_t *f, const osp_obis_t *columns, uint8_t count);
void osp_ic_profile_data_filter_set_rows(osp_ic_profile_data_filter_t *f, const osp_ic_row_t *rows, uint8_t count);
const osp_ic_class_t *osp_ic_profile_data_filter_class(void);

#endif
