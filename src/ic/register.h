/**
 * @file register.h
 * @brief Register interface class (class_id = 3, IEC 62056-6-2)
 *
 * @par Attributes
 * | # | Name         | Type            | Access |
 * |---|--------------|-----------------|--------|
 * | 1 | logical_name | octet-string    | RO     |
 * | 2 | value        | CHOICE          | RW     |
 * | 3 | scaler_unit  | scal_unit_type  | RO     |
 *
 * @par Methods
 * | # | Name  | Argument | Description                              |
 * |---|-------|----------|------------------------------------------|
 * | 1 | reset | data     | Resets the register value to the default |
 */
#ifndef OSP_IC_REGISTER_H
#define OSP_IC_REGISTER_H
#include "../openspodes.h"
#include "../codec/structures.h"

/**
 * @brief Register object structure
 *
 * Represents a register IC object with logical name, current value,
 * and scaler/unit information for the value.
 */
typedef struct {
	osp_obis_t logical_name;   /**< OBIS logical name of the register */
	osp_value_t value;         /**< Current value of the register */
	osp_scaler_unit_t scaler_unit; /**< Scaler and unit information */
} osp_ic_register_t;

/**
 * @brief Initialize a register IC object
 * @param r     Pointer to register object to initialize
 * @param ln    OBIS logical name
 * @param val   Initial value
 */
void osp_ic_register_init(osp_ic_register_t *r, osp_obis_t ln, osp_value_t val);

/**
 * @brief Get the class descriptor for the register IC
 * @return Pointer to the class descriptor structure
 */
const osp_ic_class_t *osp_ic_register_class(void);
#endif