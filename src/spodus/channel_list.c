#include "channel_list.h"
#include "spodus_obis.h"
#include <string.h>

void osp_spodus_channel_list_init(osp_spodus_channel_list_t *list) {
	if (list) {
		memset(list, 0, sizeof(*list));
	}
}

osp_err_t osp_spodus_channel_list_add(osp_spodus_channel_list_t *list, const osp_spodus_channel_t *entry) {
	if (!list || !entry || entry->interface_len > OSP_SPODUS_MAX_INTERFACE_LEN) {
		return OSP_ERR_INVALID;
	}
	if (list->count >= OSP_SPODUS_MAX_CHANNELS) {
		return OSP_ERR_NOMEM;
	}
	list->entries[list->count++] = *entry;
	return OSP_OK;
}

osp_err_t osp_spodus_channel_list_build_profile(const osp_spodus_channel_list_t *list, osp_ic_profile_generic_t *profile) {
	if (!list || !profile) {
		return OSP_ERR_INVALID;
	}
	osp_ic_profile_generic_init(profile, osp_spodus_obis_channel_list());
	profile->profile_entries = OSP_SPODUS_MAX_CHANNELS;
	profile->entries_in_use = list->count;
	profile->buffer.row_count = list->count;
	for (uint8_t i = 0; i < list->count; i++) {
		osp_profile_row_t *row = &profile->buffer.rows[i];
		row->cell_count = 2;
		row->cells[0] = osp_val_u8(list->entries[i].channel_id);
		row->cells[1].tag = OSP_TAG_OCTETSTRING;
		row->cells[1].as.octetstring.len = list->entries[i].interface_len;
		memcpy(row->cells[1].as.octetstring.data, list->entries[i].interface, list->entries[i].interface_len);
	}
	return OSP_OK;
}
