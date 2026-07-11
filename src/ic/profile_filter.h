#ifndef OSP_IC_PROFILE_FILTER_H
#define OSP_IC_PROFILE_FILTER_H
#include "../openspodes.h"
#include "../codec/structures.h"

/**
 * @file profile_filter.h
 * @brief Profile Filter interface class (class_id = 31, IEC 62056-6-2)
 *
 * @par Attributes
 * | # | Name              | Type              | Access |
 * |---|-------------------|-------------------|--------|
 * | 1 | logical_name      | octet-string      | RO     |
 * | 2 | filter_enable     | boolean           | RW     |
 * | 3 | filter_name       | octet-string      | RW     |
 * | 4 | capture_objects   | array             | RO     |
 *
 * @par Methods
 * None.
 */

#define OSP_MAX_FILTER_NAME_LEN 64

typedef struct {
	osp_obis_t logical_name;
	osp_profile_filter_entry_t entries[OSP_MAX_FILTER_ENTRIES];
	uint8_t entry_count;
	bool filter_enable;
	uint8_t filter_name[OSP_MAX_FILTER_NAME_LEN];
	uint8_t filter_name_len;
	osp_capture_object_list_t capture_objects;
} osp_ic_profile_filter_t;

void osp_ic_profile_filter_init(osp_ic_profile_filter_t *f, osp_obis_t ln);
const osp_ic_class_t *osp_ic_profile_filter_class(void);
#endif
