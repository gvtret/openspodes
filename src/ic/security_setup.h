/**
 * @file security_setup.h
 * @brief Security Setup interface class (class_id = 64, IEC 62056-6-2)
 *
 * @par Attributes
 * | # | Name                       | Type                | Access |
 * |---|----------------------------|---------------------|--------|
 * | 1 | logical_name               | octet-string        | RO     |
 * | 2 | security_suite_reference   | unsigned            | RO     |
 * | 3 | client_system_title        | octet-string        | RW     |
 * | 4 | server_system_title        | octet-string        | RW     |
 * | 5 | security_invocation_counter| double-long-unsigned| RW     |
 *
 * @par Methods
 * | # | Name                     | Argument | Description                                 |
 * |---|--------------------------|----------|---------------------------------------------|
 * | 1 | activate_key_transplantation | data | Activates key transplantation for security  |
 */
#ifndef OSP_IC_SECURITY_SETUP_H
#define OSP_IC_SECURITY_SETUP_H
#include "../openspodes.h"
#include "../codec/structures.h"

/**
 * @brief Security Setup object structure
 *
 * Manages security configuration including system titles,
 * security suites, and invocation counters for secure communications.
 */
typedef struct {
	osp_obis_t logical_name;                /**< OBIS logical name */
	uint8_t security_policy;                /**< Security policy settings */
	uint8_t security_suite;                 /**< Security suite reference */
	osp_octetstring_t client_system_title;  /**< Client system title */
	osp_octetstring_t server_system_title;  /**< Server system title */
} osp_ic_security_setup_t;

/**
 * @brief Initialize a Security Setup IC object
 * @param s  Pointer to security setup object to initialize
 * @param ln OBIS logical name
 */
void osp_ic_security_setup_init(osp_ic_security_setup_t *s, osp_obis_t ln);

/**
 * @brief Get the class descriptor for the Security Setup IC
 * @return Pointer to the class descriptor structure
 */
const osp_ic_class_t *osp_ic_security_setup_class(void);
#endif