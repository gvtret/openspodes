/**
 * data.h — Data interface class (class_id = 1, IEC 62056-6-2)
 */

#ifndef OSP_IC_DATA_H
#define OSP_IC_DATA_H

#include "../openspodes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    osp_obis_t   logical_name;
    osp_value_t  value;
} osp_ic_data_t;

void osp_ic_data_init(osp_ic_data_t *data, osp_obis_t ln);
const osp_ic_class_t *osp_ic_data_class(void);

#ifdef __cplusplus
}
#endif

#endif /* OSP_IC_DATA_H */
