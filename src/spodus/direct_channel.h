/**
 * direct_channel.h — Direct-channel table (STO §10.3, 0.0.94.7.129.255)
 */

#ifndef OSP_SPODUS_DIRECT_CHANNEL_H
#define OSP_SPODUS_DIRECT_CHANNEL_H

#include "meter_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_SPODUS_DIRECT_ID_MIN 200
#define OSP_SPODUS_DIRECT_ID_MAX 16381
#define OSP_SPODUS_MAX_DIRECT_CHANNELS 32

typedef struct {
	uint16_t direct_id;
	uint8_t meter_id_len;
	uint8_t meter_id[OSP_SPODUS_MAX_METER_ID_LEN];
	uint8_t channel_id;
} osp_spodus_direct_channel_t;

typedef struct {
	osp_spodus_direct_channel_t channels[OSP_SPODUS_MAX_DIRECT_CHANNELS];
	uint8_t count;

	osp_value_t table_value;
	osp_value_t row_structs[OSP_SPODUS_MAX_DIRECT_CHANNELS];
	osp_value_t row_fields[OSP_SPODUS_MAX_DIRECT_CHANNELS][3];
} osp_spodus_direct_channel_table_t;

void osp_spodus_direct_table_init(osp_spodus_direct_channel_table_t *table);
osp_err_t osp_spodus_direct_table_add(osp_spodus_direct_channel_table_t *table, const osp_spodus_direct_channel_t *entry);

const osp_spodus_direct_channel_t *osp_spodus_direct_table_find(const osp_spodus_direct_channel_table_t *table, uint16_t direct_id);

osp_err_t osp_spodus_direct_table_build_value(const osp_spodus_direct_channel_table_t *table, osp_value_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SPODUS_DIRECT_CHANNEL_H */
