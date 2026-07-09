#include "profile_filter.h"
#include <string.h>
static const osp_ic_class_t ic_pf = {
    .name = "Profile Filter", .class_id = 31, .version = 0, .get_attr = NULL, .set_attr = NULL, .invoke = NULL, .instance_size = sizeof(osp_ic_profile_filter_t)
};

const osp_ic_class_t *osp_ic_profile_filter_class(void) {
	return &ic_pf;
}

void osp_ic_profile_filter_init(osp_ic_profile_filter_t *f, osp_obis_t ln) {
	memset(f, 0, sizeof(*f));
	f->logical_name = ln;
}
