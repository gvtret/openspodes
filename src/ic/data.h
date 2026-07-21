/**
 * data.h — Data interface class (class_id = 1, IEC 62056-6-2)
 *
 * The simplest COSEM interface class. Holds a single value of any COSEM data type.
 * Used for configuration parameters, status flags, and generic data objects.
 *
 * @par Attributes
 * | # | Name          | Type          | Access | Description |
 * |---|---------------|---------------|--------|-------------|
 * | 1 | logical_name  | octet-string  | RO     | OBIS code identifying this object instance |
 * | 2 | value         | CHOICE        | RO/RW  | The data value (any COSEM type) |
 *
 * @par Methods
 * None.
 *
 * @note attr_id = 1 is logical_name; attr_id = 2 is value (IEC 62056-6-2).
 */

#ifndef OSP_IC_DATA_H
#define OSP_IC_DATA_H

#include "../openspodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Data IC instance. Stores logical_name (OBIS) and a single value. */
typedef struct {
	osp_obis_t logical_name; /**< OBIS code (attr 1) */
	osp_value_t value;       /**< Data value (attr 2) — any COSEM type */
} osp_ic_data_t;

/** Initialize a Data IC instance with the given OBIS logical name. */
void osp_ic_data_init(osp_ic_data_t *data, osp_obis_t ln);

/** Get the IC vtable for Data (class_id = 1). */
const osp_ic_class_t *osp_ic_data_class(void);

#ifdef __cplusplus
}
#endif

#endif /* OSP_IC_DATA_H */
