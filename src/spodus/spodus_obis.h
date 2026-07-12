/**
 * spodus_obis.h — OBIS codes of the SPODUS / IVCV model (STO 34.01-5.1-013-2023)
 */

#ifndef OSP_SPODUS_OBIS_H
#define OSP_SPODUS_OBIS_H

#include "../openspodes.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline osp_obis_t osp_spodus_obis_meter_list(void) {
	return (osp_obis_t){0, 0, 94, 7, 128, 255};
}

static inline osp_obis_t osp_spodus_obis_direct_channel_table(void) {
	return (osp_obis_t){0, 0, 94, 7, 129, 255};
}

static inline osp_obis_t osp_spodus_obis_channel_list(void) {
	return (osp_obis_t){0, 0, 94, 7, 130, 255};
}

static inline osp_obis_t osp_spodus_obis_discovered_meters(void) {
	return (osp_obis_t){0, 0, 94, 7, 131, 255};
}

static inline osp_obis_t osp_spodus_obis_access_policies(void) {
	return (osp_obis_t){0, 0, 94, 7, 132, 255};
}

static inline osp_obis_t osp_spodus_obis_exchange_tasks(void) {
	return (osp_obis_t){0, 0, 94, 7, 133, 255};
}

static inline osp_obis_t osp_spodus_obis_ivke_logical_name(void) {
	return (osp_obis_t){0, 0, 42, 0, 0, 255};
}

#ifdef __cplusplus
}
#endif

#endif /* OSP_SPODUS_OBIS_H */
