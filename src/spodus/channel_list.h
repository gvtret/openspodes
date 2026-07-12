/**
 * channel_list.h — IVCV communication channel list (STO §10.4)
 */

#ifndef OSP_SPODUS_CHANNEL_LIST_H
#define OSP_SPODUS_CHANNEL_LIST_H

#include "../ic/profile_generic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_SPODUS_MAX_CHANNELS 16
#define OSP_SPODUS_MAX_INTERFACE_LEN 32

typedef struct {
	uint8_t channel_id;
	uint8_t interface_len;
	uint8_t interface[OSP_SPODUS_MAX_INTERFACE_LEN];
} osp_spodus_channel_t;

typedef struct {
	osp_spodus_channel_t entries[OSP_SPODUS_MAX_CHANNELS];
	uint8_t count;
} osp_spodus_channel_list_t;

void osp_spodus_channel_list_init(osp_spodus_channel_list_t *list);
osp_err_t osp_spodus_channel_list_add(osp_spodus_channel_list_t *list, const osp_spodus_channel_t *entry);
osp_err_t osp_spodus_channel_list_build_profile(const osp_spodus_channel_list_t *list, osp_ic_profile_generic_t *profile);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SPODUS_CHANNEL_LIST_H */
