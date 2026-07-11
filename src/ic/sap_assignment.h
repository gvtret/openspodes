/**
 * @file sap_assignment.h
 * @brief SAP Assignment interface class (class_id = 17, IEC 62056-6-2)
 *
 * @par Attributes
 * | # | Name           | Type              | Access |
 * |---|----------------|-------------------|--------|
 * | 1 | logical_name   | octet-string      | RO     |
 * | 2 | port_assignment| array of {SAP, logical_name} | RW |
 *
 * @par Methods
 * | # | Name | Argument | Description |
 * |---|------|----------|-------------|
 * | --| --   | --       | --          |
 */
#ifndef OSP_IC_SAP_ASSIGNMENT_H
#define OSP_IC_SAP_ASSIGNMENT_H
#include "../openspodes.h"
#include "../codec/structures.h"

/**
 * @brief SAP assignment item structure
 *
 * Maps a SAP (Service Access Point) identifier to a logical device name.
 */
typedef struct {
	uint16_t sap;                          /**< Service Access Point identifier */
	uint8_t logical_device_name[64];       /**< Logical device name string */
	uint8_t logical_device_name_len;       /**< Length of logical device name */
} osp_sap_assignment_item_t;

/**
 * @brief SAP assignment list structure
 *
 * Contains a list of SAP assignment items.
 */
typedef struct {
	osp_sap_assignment_item_t items[16];   /**< Array of SAP assignment items */
	uint8_t count;                         /**< Number of items in the list */
} osp_sap_assignment_list_t;

/**
 * @brief SAP Assignment object structure
 *
 * Maps communication ports (SAPs) to logical device names for
 * routing requests to the appropriate logical device.
 */
typedef struct {
	osp_obis_t logical_name;               /**< OBIS logical name */
	osp_sap_assignment_list_t sap_list;    /**< List of SAP assignments */
} osp_ic_sap_assignment_t;

/**
 * @brief Initialize a SAP Assignment IC object
 * @param s  Pointer to SAP assignment object to initialize
 * @param ln OBIS logical name
 */
void osp_ic_sap_assignment_init(osp_ic_sap_assignment_t *s, osp_obis_t ln);

/**
 * @brief Get the class descriptor for the SAP Assignment IC
 * @return Pointer to the class descriptor structure
 */
const osp_ic_class_t *osp_ic_sap_assignment_class(void);

#endif