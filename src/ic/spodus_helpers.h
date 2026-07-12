/**
 * spodus_helpers.h — Shared helpers for SPODUS IC classes 8200/8201
 */

#ifndef OSP_SPODUS_HELPERS_H
#define OSP_SPODUS_HELPERS_H

#include "../openspodes.h"
#include "../codec/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_IC_ROW_MAX_FIELDS OSP_MAX_STRUCT_LEN

typedef struct {
	osp_value_t fields[OSP_IC_ROW_MAX_FIELDS];
	uint8_t field_count;
} osp_ic_row_t;

bool osp_ic_value_eq(const osp_value_t *a, const osp_value_t *b);
int osp_ic_value_compare(const osp_value_t *a, const osp_value_t *b);

const osp_value_t *osp_ic_row_key(const osp_ic_row_t *row, uint8_t key_index);
osp_err_t osp_ic_spodus_parse_entries_list(const osp_value_t *param, osp_value_t *entries, uint8_t capacity, uint8_t *count);
osp_err_t osp_ic_spodus_parse_filter_request(const osp_value_t *param, osp_value_t *selected, uint8_t sel_cap, uint8_t *sel_count, osp_value_t *filters,
                                           uint8_t filt_cap, uint8_t *filt_count);

osp_err_t osp_ic_row_to_value(const osp_ic_row_t *row, osp_value_t *out);
osp_err_t osp_ic_value_to_row(const osp_value_t *val, osp_ic_row_t *row);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SPODUS_HELPERS_H */
