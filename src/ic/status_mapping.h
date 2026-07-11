#ifndef OSP_IC_STATUS_MAPPING_H
#define OSP_IC_STATUS_MAPPING_H
#include "../openspodes.h"

/**
 * @file status_mapping.h
 * @brief Status Mapping interface class (class_id = 63, IEC 62056-6-2)
 *
 * @par Attributes
 * | # | Name             | Type          | Access |
 * |---|------------------|---------------|--------|
 * | 1 | logical_name     | octet-string  | RO     |
 * | 2 | status_mappings  | array         | RO     |
 *
 * @par Methods
 * None.
 */

#define OSP_MAX_STATUS_MAPPINGS 16

typedef struct {
	uint8_t status_flag_id;
	uint8_t status_reference[6];
} osp_status_mapping_entry_t;

typedef struct {
	osp_obis_t logical_name;
	osp_status_mapping_entry_t entries[OSP_MAX_STATUS_MAPPINGS];
	uint8_t entry_count;
} osp_ic_status_mapping_t;

void osp_ic_status_mapping_init(osp_ic_status_mapping_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_status_mapping_class(void);
#endif
