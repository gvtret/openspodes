/**
 * @file profile_generic.h
 * @brief Profile Generic interface class (class_id = 7, version 1, IEC 62056-6-2)
 *
 * @par Attributes
 * | # | Name              | Type                              | Access |
 * |---|-------------------|-----------------------------------|--------|
 * | 1 | logical_name      | octet-string                      | RO     |
 * | 2 | buffer            | compact-array / array             | RW     |
 * | 3 | capture_objects   | array                             | RW     |
 * | 4 | capture_period    | double-long-unsigned              | RW     |
 * | 5 | sort_method       | enum                              | RW     |
 * | 6 | sort_object       | capture_object_definition         | RW     |
 * | 7 | entries_in_use    | double-long-unsigned              | RO     |
 * | 8 | profile_entries   | double-long-unsigned              | RO     |
 *
 * @par Methods
 * | # | Name    | Argument | Description                                        |
 * |---|---------|----------|----------------------------------------------------|
 * | 1 | reset   | data     | Resets the buffer and entries_in_use to zero        |
 * | 2 | capture | data     | Triggers an immediate capture of all capture objects |
 */
#ifndef OSP_IC_PROFILE_GENERIC_H
#define OSP_IC_PROFILE_GENERIC_H
#include "../openspodes.h"
#include "../codec/structures.h"

/**
 * @brief Profile Generic object structure
 *
 * Stores captured data in a circular buffer with configurable
 * capture objects and capture period.
 */
typedef struct {
	osp_obis_t logical_name;                  /**< OBIS logical name */
	osp_profile_buffer_t buffer;              /**< Data buffer (compact-array or array) */
	osp_capture_object_list_t capture_objects; /**< List of objects to capture */
	uint32_t capture_period;                  /**< Capture period in seconds */
	osp_sort_method_t sort_method;            /**< Sort method for buffer entries */
	osp_capture_object_t sort_object;         /**< Object used for sorting */
	uint32_t entries_in_use;                  /**< Current number of entries in buffer */
	uint32_t profile_entries;                 /**< Maximum number of buffer entries */
} osp_ic_profile_generic_t;

/**
 * @brief Initialize a Profile Generic IC object
 * @param p  Pointer to profile generic object to initialize
 * @param ln OBIS logical name
 */
void osp_ic_profile_generic_init(osp_ic_profile_generic_t *p, osp_obis_t ln);

/**
 * @brief Get the class descriptor for the Profile Generic IC
 * @return Pointer to the class descriptor structure
 */
const osp_ic_class_t *osp_ic_profile_generic_class(void);
#endif