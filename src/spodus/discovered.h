/**
 * discovered.h — Discovered meter list (STO §10.5)
 */

#ifndef OSP_SPODUS_DISCOVERED_H
#define OSP_SPODUS_DISCOVERED_H

#include "meter_registry.h"
#include "../ic/profile_generic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_SPODUS_DISCOVERED_TIME_MAX 12

typedef struct {
	uint8_t meter_id_len;
	uint8_t meter_id[OSP_SPODUS_MAX_METER_ID_LEN];
	uint8_t meter_model_len;
	uint8_t meter_model[OSP_SPODUS_MAX_METER_MODEL_LEN];
	uint8_t channel_id;
	uint16_t address;
	uint8_t first_seen_len;
	uint8_t first_seen[OSP_SPODUS_DISCOVERED_TIME_MAX];
	uint8_t last_seen_len;
	uint8_t last_seen[OSP_SPODUS_DISCOVERED_TIME_MAX];
} osp_spodus_discovered_meter_t;

typedef struct {
	osp_spodus_discovered_meter_t entries[OSP_SPODUS_MAX_METERS];
	uint8_t count;
} osp_spodus_discovered_list_t;

void osp_spodus_discovered_list_init(osp_spodus_discovered_list_t *list);
osp_err_t osp_spodus_discovered_list_add(osp_spodus_discovered_list_t *list, const osp_spodus_discovered_meter_t *entry);
osp_err_t osp_spodus_discovered_list_build_profile(const osp_spodus_discovered_list_t *list, osp_ic_profile_generic_t *profile);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SPODUS_DISCOVERED_H */
