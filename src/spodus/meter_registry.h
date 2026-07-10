/**
 * meter_registry.h — Configured meter list and aggregation cache (СТО §10.2)
 */

#ifndef OSP_SPODUS_METER_REGISTRY_H
#define OSP_SPODUS_METER_REGISTRY_H

#include "../openspodes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_SPODUS_MAX_METERS              16
#define OSP_SPODUS_MAX_CHANNELS_PER_METER  4
#define OSP_SPODUS_MAX_METER_ID_LEN        32
#define OSP_SPODUS_MAX_METER_MODEL_LEN     32
#define OSP_SPODUS_MAX_CHANNEL_ADDR_LEN    32
#define OSP_SPODUS_MAX_CACHE_ENTRIES       32

typedef struct {
	uint8_t id;
	uint8_t address_len;
	uint8_t address[OSP_SPODUS_MAX_CHANNEL_ADDR_LEN];
} osp_spodus_meter_channel_t;

typedef struct {
	uint8_t meter_id_len;
	uint8_t meter_id[OSP_SPODUS_MAX_METER_ID_LEN];
	uint8_t meter_model_len;
	uint8_t meter_model[OSP_SPODUS_MAX_METER_MODEL_LEN];
	uint8_t channel_count;
	osp_spodus_meter_channel_t channels[OSP_SPODUS_MAX_CHANNELS_PER_METER];
} osp_spodus_meter_descriptor_t;

typedef struct {
	bool used;
	uint8_t meter_id_len;
	uint8_t meter_id[OSP_SPODUS_MAX_METER_ID_LEN];
	osp_obis_t obis;
	uint8_t attribute_id;
	osp_value_t value;
	uint8_t value_blob[OSP_MAX_OCTET_LEN];
} osp_spodus_cache_entry_t;

typedef struct {
	osp_spodus_meter_descriptor_t meters[OSP_SPODUS_MAX_METERS];
	uint8_t meter_count;
	osp_spodus_cache_entry_t cache[OSP_SPODUS_MAX_CACHE_ENTRIES];

	/* Scratch rebuilt by osp_spodus_registry_build_meter_list */
	osp_value_t list_value;
	osp_value_t meter_structs[OSP_SPODUS_MAX_METERS];
	osp_value_t meter_fields[OSP_SPODUS_MAX_METERS][3];
	osp_value_t channel_arrays[OSP_SPODUS_MAX_METERS];
	osp_value_t channel_structs[OSP_SPODUS_MAX_METERS][OSP_SPODUS_MAX_CHANNELS_PER_METER];
	osp_value_t channel_fields[OSP_SPODUS_MAX_METERS][OSP_SPODUS_MAX_CHANNELS_PER_METER][2];
} osp_spodus_meter_registry_t;

void osp_spodus_registry_init(osp_spodus_meter_registry_t *reg);

osp_err_t osp_spodus_registry_add(osp_spodus_meter_registry_t *reg, const osp_spodus_meter_descriptor_t *meter);
void osp_spodus_registry_remove(osp_spodus_meter_registry_t *reg, const uint8_t *meter_id, uint8_t meter_id_len);

const osp_spodus_meter_descriptor_t *osp_spodus_registry_find(const osp_spodus_meter_registry_t *reg, const uint8_t *meter_id,
                                                              uint8_t meter_id_len);

osp_err_t osp_spodus_registry_store(osp_spodus_meter_registry_t *reg, const uint8_t *meter_id, uint8_t meter_id_len, osp_obis_t obis,
                                    uint8_t attribute_id, const osp_value_t *value);

const osp_value_t *osp_spodus_registry_cached(const osp_spodus_meter_registry_t *reg, const uint8_t *meter_id, uint8_t meter_id_len,
                                              const osp_obis_t *obis, uint8_t attribute_id);

osp_err_t osp_spodus_registry_build_meter_list(const osp_spodus_meter_registry_t *reg, osp_value_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SPODUS_METER_REGISTRY_H */
