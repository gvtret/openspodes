#include "compact_data.h"
#include <string.h>
static const osp_ic_class_t ic_compact_data = {
    .name = "Compact Data", .class_id = 62, .version = 0, .get_attr = NULL, .set_attr = NULL, .invoke = NULL, .instance_size = sizeof(osp_ic_compact_data_t)
};

const osp_ic_class_t *osp_ic_compact_data_class(void) {
	return &ic_compact_data;
}

void osp_ic_compact_data_init(osp_ic_compact_data_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
