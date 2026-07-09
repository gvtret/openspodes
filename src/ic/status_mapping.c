#include "status_mapping.h"
#include <string.h>
static const osp_ic_class_t ic_status_mapping = {
    .name = "Status Mapping", .class_id = 63, .version = 0, .get_attr = NULL, .set_attr = NULL, .invoke = NULL, .instance_size = sizeof(osp_ic_status_mapping_t)
};

const osp_ic_class_t *osp_ic_status_mapping_class(void) {
	return &ic_status_mapping;
}

void osp_ic_status_mapping_init(osp_ic_status_mapping_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
