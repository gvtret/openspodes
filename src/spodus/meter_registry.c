#include "meter_registry.h"
#include <string.h>

static bool meter_id_eq(const uint8_t *a, uint8_t a_len, const uint8_t *b, uint8_t b_len) {
	return a_len == b_len && memcmp(a, b, a_len) == 0;
}

static void fill_octet(osp_value_t *v, const uint8_t *data, uint32_t len) {
	if (len > OSP_MAX_OCTET_LEN) {
		len = OSP_MAX_OCTET_LEN;
	}
	v->tag = OSP_TAG_OCTETSTRING;
	v->as.octetstring.len = len;
	memcpy(v->as.octetstring.data, data, len);
}

static osp_err_t value_copy(osp_value_t *dst, const osp_value_t *src) {
	if (!dst || !src) {
		return OSP_ERR_INVALID;
	}
	*dst = *src;
	return OSP_OK;
}

void osp_spodus_registry_init(osp_spodus_meter_registry_t *reg) {
	if (reg) {
		memset(reg, 0, sizeof(*reg));
	}
}

static void registry_drop_cache_for(osp_spodus_meter_registry_t *reg, const uint8_t *meter_id, uint8_t meter_id_len) {
	for (uint8_t i = 0; i < OSP_SPODUS_MAX_CACHE_ENTRIES; i++) {
		if (reg->cache[i].used && meter_id_eq(reg->cache[i].meter_id, reg->cache[i].meter_id_len, meter_id, meter_id_len)) {
			reg->cache[i].used = false;
		}
	}
}

osp_err_t osp_spodus_registry_add(osp_spodus_meter_registry_t *reg, const osp_spodus_meter_descriptor_t *meter) {
	if (!reg || !meter || meter->meter_id_len == 0) {
		return OSP_ERR_INVALID;
	}
	osp_spodus_registry_remove(reg, meter->meter_id, meter->meter_id_len);
	if (reg->meter_count >= OSP_SPODUS_MAX_METERS) {
		return OSP_ERR_NOMEM;
	}
	reg->meters[reg->meter_count++] = *meter;
	return OSP_OK;
}

void osp_spodus_registry_remove(osp_spodus_meter_registry_t *reg, const uint8_t *meter_id, uint8_t meter_id_len) {
	if (!reg || !meter_id || meter_id_len == 0) {
		return;
	}
	for (uint8_t i = 0; i < reg->meter_count; i++) {
		if (meter_id_eq(reg->meters[i].meter_id, reg->meters[i].meter_id_len, meter_id, meter_id_len)) {
			for (uint8_t j = i + 1; j < reg->meter_count; j++) {
				reg->meters[j - 1] = reg->meters[j];
			}
			reg->meter_count--;
			break;
		}
	}
	registry_drop_cache_for(reg, meter_id, meter_id_len);
}

const osp_spodus_meter_descriptor_t *osp_spodus_registry_find(const osp_spodus_meter_registry_t *reg, const uint8_t *meter_id,
                                                              uint8_t meter_id_len) {
	if (!reg || !meter_id) {
		return NULL;
	}
	for (uint8_t i = 0; i < reg->meter_count; i++) {
		if (meter_id_eq(reg->meters[i].meter_id, reg->meters[i].meter_id_len, meter_id, meter_id_len)) {
			return &reg->meters[i];
		}
	}
	return NULL;
}

osp_err_t osp_spodus_registry_store(osp_spodus_meter_registry_t *reg, const uint8_t *meter_id, uint8_t meter_id_len, osp_obis_t obis,
                                    uint8_t attribute_id, const osp_value_t *value) {
	if (!reg || !meter_id || !value) {
		return OSP_ERR_INVALID;
	}
	for (uint8_t i = 0; i < OSP_SPODUS_MAX_CACHE_ENTRIES; i++) {
		if (reg->cache[i].used && meter_id_eq(reg->cache[i].meter_id, reg->cache[i].meter_id_len, meter_id, meter_id_len) &&
		    osp_obis_eq(&reg->cache[i].obis, &obis) && reg->cache[i].attribute_id == attribute_id) {
			return value_copy(&reg->cache[i].value, value);
		}
	}
	for (uint8_t i = 0; i < OSP_SPODUS_MAX_CACHE_ENTRIES; i++) {
		if (!reg->cache[i].used) {
			reg->cache[i].used = true;
			reg->cache[i].meter_id_len = meter_id_len;
			memcpy(reg->cache[i].meter_id, meter_id, meter_id_len);
			reg->cache[i].obis = obis;
			reg->cache[i].attribute_id = attribute_id;
			return value_copy(&reg->cache[i].value, value);
		}
	}
	return OSP_ERR_NOMEM;
}

const osp_value_t *osp_spodus_registry_cached(const osp_spodus_meter_registry_t *reg, const uint8_t *meter_id, uint8_t meter_id_len,
                                              const osp_obis_t *obis, uint8_t attribute_id) {
	if (!reg || !meter_id || !obis) {
		return NULL;
	}
	for (uint8_t i = 0; i < OSP_SPODUS_MAX_CACHE_ENTRIES; i++) {
		if (reg->cache[i].used && meter_id_eq(reg->cache[i].meter_id, reg->cache[i].meter_id_len, meter_id, meter_id_len) &&
		    osp_obis_eq(&reg->cache[i].obis, obis) && reg->cache[i].attribute_id == attribute_id) {
			return &reg->cache[i].value;
		}
	}
	return NULL;
}

osp_err_t osp_spodus_registry_build_meter_list(const osp_spodus_meter_registry_t *reg, osp_value_t *out) {
	if (!reg || !out) {
		return OSP_ERR_INVALID;
	}
	osp_spodus_meter_registry_t *scratch = (osp_spodus_meter_registry_t *)reg;

	scratch->list_value.tag = OSP_TAG_ARRAY;
	scratch->list_value.as.array.elements.items = scratch->meter_structs;
	scratch->list_value.as.array.elements.count = reg->meter_count;
	scratch->list_value.as.array.elements.capacity = OSP_SPODUS_MAX_METERS;

	for (uint8_t m = 0; m < reg->meter_count; m++) {
		const osp_spodus_meter_descriptor_t *desc = &reg->meters[m];
		osp_value_t *meter_struct = &scratch->meter_structs[m];
		meter_struct->tag = OSP_TAG_STRUCTURE;
		meter_struct->as.structure.elements.items = scratch->meter_fields[m];
		meter_struct->as.structure.elements.count = 3;
		meter_struct->as.structure.elements.capacity = 3;

		scratch->meter_fields[m][0].tag = OSP_TAG_OCTETSTRING;
		fill_octet(&scratch->meter_fields[m][0], desc->meter_id, desc->meter_id_len);
		fill_octet(&scratch->meter_fields[m][1], desc->meter_model, desc->meter_model_len);

		scratch->channel_arrays[m].tag = OSP_TAG_ARRAY;
		scratch->channel_arrays[m].as.array.elements.items = scratch->channel_structs[m];
		scratch->channel_arrays[m].as.array.elements.count = desc->channel_count;
		scratch->channel_arrays[m].as.array.elements.capacity = OSP_SPODUS_MAX_CHANNELS_PER_METER;

		for (uint8_t c = 0; c < desc->channel_count; c++) {
			osp_value_t *ch = &scratch->channel_structs[m][c];
			ch->tag = OSP_TAG_STRUCTURE;
			ch->as.structure.elements.items = scratch->channel_fields[m][c];
			ch->as.structure.elements.count = 2;
			ch->as.structure.elements.capacity = 2;
			scratch->channel_fields[m][c][0] = osp_val_u8(desc->channels[c].id);
			fill_octet(&scratch->channel_fields[m][c][1], desc->channels[c].address, desc->channels[c].address_len);
		}
		scratch->meter_fields[m][2] = scratch->channel_arrays[m];
	}

	*out = scratch->list_value;
	return OSP_OK;
}
