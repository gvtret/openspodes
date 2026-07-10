#include "discovered.h"
#include "spodus_obis.h"
#include <string.h>

void osp_spodus_discovered_list_init(osp_spodus_discovered_list_t *list) {
	if (list) {
		memset(list, 0, sizeof(*list));
	}
}

osp_err_t osp_spodus_discovered_list_add(osp_spodus_discovered_list_t *list, const osp_spodus_discovered_meter_t *entry) {
	if (!list || !entry || entry->meter_id_len == 0 || entry->first_seen_len > OSP_SPODUS_DISCOVERED_TIME_MAX ||
	    entry->last_seen_len > OSP_SPODUS_DISCOVERED_TIME_MAX) {
		return OSP_ERR_INVALID;
	}
	if (list->count >= OSP_SPODUS_MAX_METERS) {
		return OSP_ERR_NOMEM;
	}
	list->entries[list->count++] = *entry;
	return OSP_OK;
}

static void discovered_octet(osp_value_t *value, const uint8_t *data, uint8_t len) {
	value->tag = OSP_TAG_OCTETSTRING;
	value->as.octetstring.len = len;
	memcpy(value->as.octetstring.data, data, len);
}

osp_err_t osp_spodus_discovered_list_build_profile(const osp_spodus_discovered_list_t *list, osp_ic_profile_generic_t *profile) {
	if (!list || !profile) {
		return OSP_ERR_INVALID;
	}
	osp_ic_profile_generic_init(profile, osp_spodus_obis_discovered_meters());
	profile->profile_entries = OSP_SPODUS_MAX_METERS;
	profile->entries_in_use = list->count;
	profile->buffer.row_count = list->count;
	for (uint8_t i = 0; i < list->count; i++) {
		const osp_spodus_discovered_meter_t *entry = &list->entries[i];
		osp_profile_row_t *row = &profile->buffer.rows[i];
		row->cell_count = 6;
		discovered_octet(&row->cells[0], entry->meter_id, entry->meter_id_len);
		discovered_octet(&row->cells[1], entry->meter_model, entry->meter_model_len);
		row->cells[2] = osp_val_u8(entry->channel_id);
		row->cells[3] = osp_val_u16(entry->address);
		discovered_octet(&row->cells[4], entry->first_seen, entry->first_seen_len);
		discovered_octet(&row->cells[5], entry->last_seen, entry->last_seen_len);
	}
	return OSP_OK;
}
