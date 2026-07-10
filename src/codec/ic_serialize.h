/**
 * ic_serialize.h — Whole-object BER serialization for COSEM interface classes
 */

#ifndef OSP_IC_SERIALIZE_H
#define OSP_IC_SERIALIZE_H

#include "../openspodes.h"

#ifdef __cplusplus
extern "C" {
#endif

osp_err_t osp_ic_serialize(const osp_ic_class_t *cls, const void *inst, osp_buf_t *buf);
osp_err_t osp_ic_deserialize(const osp_ic_class_t *cls, void *inst, osp_buf_t *buf);

osp_err_t osp_ic_write_object_header(osp_buf_t *buf, uint16_t class_id, const osp_obis_t *ln, uint8_t field_count);

#ifdef __cplusplus
}
#endif

#endif /* OSP_IC_SERIALIZE_H */
