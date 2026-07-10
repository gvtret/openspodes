#include "direct_channel.h"
#include <string.h>

static void direct_fill_octet(osp_value_t *v, const uint8_t *data, uint32_t len) {
	if (len > OSP_MAX_OCTET_LEN) {
		len = OSP_MAX_OCTET_LEN;
	}
	v->tag = OSP_TAG_OCTETSTRING;
	v->as.octetstring.len = len;
	memcpy(v->as.octetstring.data, data, len);
}

void osp_spodus_direct_table_init(osp_spodus_direct_channel_table_t *table) {
	if (table) {
		memset(table, 0, sizeof(*table));
	}
}

osp_err_t osp_spodus_direct_table_add(osp_spodus_direct_channel_table_t *table, const osp_spodus_direct_channel_t *entry) {
	if (!table || !entry || entry->meter_id_len == 0) {
		return OSP_ERR_INVALID;
	}
	if (entry->direct_id < OSP_SPODUS_DIRECT_ID_MIN || entry->direct_id > OSP_SPODUS_DIRECT_ID_MAX) {
		return OSP_ERR_INVALID;
	}
	if (table->count >= OSP_SPODUS_MAX_DIRECT_CHANNELS) {
		return OSP_ERR_NOMEM;
	}
	table->channels[table->count++] = *entry;
	return OSP_OK;
}

const osp_spodus_direct_channel_t *osp_spodus_direct_table_find(const osp_spodus_direct_channel_table_t *table, uint16_t direct_id) {
	if (!table) {
		return NULL;
	}
	for (uint8_t i = 0; i < table->count; i++) {
		if (table->channels[i].direct_id == direct_id) {
			return &table->channels[i];
		}
	}
	return NULL;
}

osp_err_t osp_spodus_direct_table_build_value(const osp_spodus_direct_channel_table_t *table, osp_value_t *out) {
	if (!table || !out) {
		return OSP_ERR_INVALID;
	}
	osp_spodus_direct_channel_table_t *scratch = (osp_spodus_direct_channel_table_t *)table;

	scratch->table_value.tag = OSP_TAG_ARRAY;
	scratch->table_value.as.array.elements.items = scratch->row_structs;
	scratch->table_value.as.array.elements.count = table->count;
	scratch->table_value.as.array.elements.capacity = OSP_SPODUS_MAX_DIRECT_CHANNELS;

	for (uint8_t i = 0; i < table->count; i++) {
		const osp_spodus_direct_channel_t *row = &table->channels[i];
		scratch->row_fields[i][0] = osp_val_u16(row->direct_id);
		direct_fill_octet(&scratch->row_fields[i][1], row->meter_id, row->meter_id_len);
		scratch->row_fields[i][2] = osp_val_u8(row->channel_id);
		scratch->row_structs[i].tag = OSP_TAG_STRUCTURE;
		scratch->row_structs[i].as.structure.elements.items = scratch->row_fields[i];
		scratch->row_structs[i].as.structure.elements.count = 3;
		scratch->row_structs[i].as.structure.elements.capacity = 3;
	}

	*out = scratch->table_value;
	return OSP_OK;
}
