/**
 * ic_common.h — Shared helpers for COSEM interface class implementations
 */

#ifndef OSP_IC_COMMON_H
#define OSP_IC_COMMON_H

#include "../openspodes.h"
#include "../codec/structures.h"

#ifdef __cplusplus
extern "C" {
#endif

osp_err_t osp_ic_get_logical_name(osp_value_t *result, const osp_obis_t *ln);
osp_err_t osp_ic_set_logical_name(osp_obis_t *ln, const osp_value_t *value);

osp_value_t osp_ic_val_scaler_unit(const osp_scaler_unit_t *su);
osp_err_t osp_ic_read_scaler_unit(const osp_value_t *val, osp_scaler_unit_t *su);

osp_value_t osp_ic_val_value_definition(const osp_value_definition_t *vd);
osp_err_t osp_ic_read_value_definition(const osp_value_t *val, osp_value_definition_t *vd);

osp_value_t osp_ic_val_empty_array(void);

osp_value_t osp_ic_val_xdms_context(const osp_xdms_context_t *ctx);
osp_err_t osp_ic_read_xdms_context(const osp_value_t *val, osp_xdms_context_t *ctx);

osp_value_t osp_ic_val_associated_partners(const osp_associated_partners_t *p);
osp_err_t osp_ic_read_associated_partners(const osp_value_t *val, osp_associated_partners_t *p);

osp_value_t osp_ic_val_threshold(const osp_threshold_t *t);
osp_err_t osp_ic_read_threshold(const osp_value_t *val, osp_threshold_t *t);

osp_value_t osp_ic_val_emergency_profile(const osp_emergency_profile_t *ep);
osp_err_t osp_ic_read_emergency_profile(const osp_value_t *val, osp_emergency_profile_t *ep);

osp_value_t osp_ic_val_capture_object(const osp_capture_object_t *co);
osp_err_t osp_ic_read_capture_object(const osp_value_t *val, osp_capture_object_t *co);

osp_value_t osp_ic_val_threshold_list(const osp_threshold_list_t *tl);
osp_err_t osp_ic_read_threshold_list(const osp_value_t *val, osp_threshold_list_t *tl);

osp_value_t osp_ic_val_object_list(const osp_object_list_t *ol);
osp_err_t osp_ic_read_object_list(const osp_value_t *val, osp_object_list_t *ol);

osp_value_t osp_ic_val_user_list_item(const osp_user_list_item_t *item);
osp_err_t osp_ic_read_user_list_item(const osp_value_t *val, osp_user_list_item_t *item);

osp_value_t osp_ic_val_context_name(const osp_context_name_t *cn);
osp_err_t osp_ic_read_context_name(const osp_value_t *val, osp_context_name_t *cn);

osp_value_t osp_ic_val_season(const osp_season_t *s);
osp_err_t osp_ic_read_season(const osp_value_t *val, osp_season_t *s);

osp_value_t osp_ic_val_week_profile(const osp_week_profile_t *wp);
osp_err_t osp_ic_read_week_profile(const osp_value_t *val, osp_week_profile_t *wp);

osp_value_t osp_ic_val_day_profile(const osp_day_profile_t *dp);
osp_err_t osp_ic_read_day_profile(const osp_value_t *val, osp_day_profile_t *dp);

osp_err_t osp_ic_ln_only_get_attr(const void *inst, uint8_t attr_id, osp_value_t *result);

osp_err_t osp_ic_serialize_attrs(const osp_ic_class_t *cls, const void *inst, osp_buf_t *buf, const uint8_t *attr_ids, uint8_t attr_count);
osp_err_t osp_ic_deserialize_attrs(const osp_ic_class_t *cls, void *inst, osp_buf_t *buf, const uint8_t *attr_ids, uint8_t attr_count);

#ifdef __cplusplus
}
#endif

#endif /* OSP_IC_COMMON_H */
