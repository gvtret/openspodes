#ifndef OSP_IC_UTILITY_TABLES_H
#define OSP_IC_UTILITY_TABLES_H
#include "../openspodes.h"

/**
 * @file utility_tables.h
 * @brief Utility Tables interface class (class_id = 26, IEC 62056-6-2)
 *
 * @par Attributes
 * | # | Name         | Type          | Access |
 * |---|--------------|---------------|--------|
 * | 1 | logical_name | octet-string  | RO     |
 * | 2 | table_ID     | long-unsigned | RO     |
 * | 3 | length       | double-long-unsigned | RO |
 * | 4 | buffer       | octet-string  | RO     |
 *
 * @par Methods
 * None.
 */

#define OSP_MAX_UTILITY_TABLE_BUF 256

typedef struct {
	osp_obis_t logical_name;
	uint16_t table_id;
	uint32_t length;
	uint8_t buffer[OSP_MAX_UTILITY_TABLE_BUF];
	uint32_t buffer_len;
} osp_ic_utility_tables_t;

void osp_ic_utility_tables_init(osp_ic_utility_tables_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_utility_tables_class(void);
#endif
