#include "access_policy.h"
#include <string.h>

static void octets(osp_value_t *v, const uint8_t *data, uint8_t len) {
	v->tag = OSP_TAG_OCTETSTRING;
	v->as.octetstring.len = len;
	memcpy(v->as.octetstring.data, data, len);
}

void osp_spodus_access_policies_init(osp_spodus_access_policies_t *policies) {
	if (policies) {
		memset(policies, 0, sizeof(*policies));
	}
}

osp_err_t osp_spodus_access_policies_add(osp_spodus_access_policies_t *policies, const osp_spodus_access_policy_t *policy) {
	if (!policies || !policy || policy->meter_id_len == 0 || policy->security_count > OSP_SPODUS_MAX_SECURITY_ITEMS) {
		return OSP_ERR_INVALID;
	}
	if (policies->count >= OSP_SPODUS_MAX_ACCESS_POLICIES) {
		return OSP_ERR_NOMEM;
	}
	policies->entries[policies->count++] = *policy;
	return OSP_OK;
}

osp_err_t osp_spodus_access_policies_build_value(const osp_spodus_access_policies_t *policies, osp_value_t *out) {
	if (!policies || !out) {
		return OSP_ERR_INVALID;
	}
	osp_spodus_access_policies_t *scratch = (osp_spodus_access_policies_t *)policies;
	scratch->value.tag = OSP_TAG_ARRAY;
	scratch->value.as.array.elements.items = scratch->rows;
	scratch->value.as.array.elements.count = scratch->count;
	scratch->value.as.array.elements.capacity = OSP_SPODUS_MAX_ACCESS_POLICIES;
	for (uint8_t i = 0; i < scratch->count; i++) {
		const osp_spodus_access_policy_t *policy = &scratch->entries[i];
		scratch->rows[i].tag = OSP_TAG_STRUCTURE;
		scratch->rows[i].as.structure.elements.items = scratch->fields[i];
		scratch->rows[i].as.structure.elements.count = 4;
		scratch->rows[i].as.structure.elements.capacity = 4;
		octets(&scratch->fields[i][0], policy->meter_id, policy->meter_id_len);
		scratch->fields[i][1] = osp_val_u8(policy->policy_id);
		scratch->fields[i][2] = osp_val_u8(policy->suite_id);
		scratch->security_arrays[i].tag = OSP_TAG_ARRAY;
		scratch->security_arrays[i].as.array.elements.items = scratch->security_rows[i];
		scratch->security_arrays[i].as.array.elements.count = policy->security_count;
		scratch->security_arrays[i].as.array.elements.capacity = OSP_SPODUS_MAX_SECURITY_ITEMS;
		for (uint8_t j = 0; j < policy->security_count; j++) {
			scratch->security_rows[i][j].tag = OSP_TAG_STRUCTURE;
			scratch->security_rows[i][j].as.structure.elements.items = scratch->security_fields[i][j];
			scratch->security_rows[i][j].as.structure.elements.count = 2;
			scratch->security_rows[i][j].as.structure.elements.capacity = 2;
			scratch->security_fields[i][j][0] = osp_val_u8(policy->security[j].type);
			octets(&scratch->security_fields[i][j][1], policy->security[j].key, policy->security[j].key_len);
		}
		scratch->fields[i][3] = scratch->security_arrays[i];
	}
	*out = scratch->value;
	return OSP_OK;
}
