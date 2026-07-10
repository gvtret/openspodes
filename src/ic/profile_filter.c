#include "profile_filter.h"
#include "ic_common.h"
#include <string.h>

static const uint8_t pf_attrs[] = {1};

static osp_err_t pf_get(const void *inst, uint8_t attr_id, osp_value_t *result) {
	return osp_ic_ln_only_get_attr(inst, attr_id, result);
}

static osp_err_t pf_serialize(const void *inst, osp_buf_t *buf) {
	return osp_ic_serialize_attrs(osp_ic_profile_filter_class(), inst, buf, pf_attrs, 1);
}

static osp_err_t pf_deserialize(void *inst, osp_buf_t *buf) {
	return osp_ic_deserialize_attrs(osp_ic_profile_filter_class(), inst, buf, pf_attrs, 1);
}

static const osp_ic_class_t ic_pf = {
    .name = "Profile Filter",
    .class_id = 31,
    .version = 0,
    .get_attr = pf_get,
    .set_attr = NULL,
    .invoke = NULL,
    .serialize = pf_serialize,
    .deserialize = pf_deserialize,
    .instance_size = sizeof(osp_ic_profile_filter_t),
};

const osp_ic_class_t *osp_ic_profile_filter_class(void) {
	return &ic_pf;
}

void osp_ic_profile_filter_init(osp_ic_profile_filter_t *f, osp_obis_t ln) {
	memset(f, 0, sizeof(*f));
	f->logical_name = ln;
}
