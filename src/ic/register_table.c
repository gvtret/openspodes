#include "register_table.h"
#include <string.h>
static const osp_ic_class_t ic_register_table = {
    .name = "Register Table", .class_id = 61, .version = 0, .get_attr = NULL, .set_attr = NULL, .invoke = NULL, .instance_size = sizeof(osp_ic_register_table_t)
};

const osp_ic_class_t *osp_ic_register_table_class(void) {
	return &ic_register_table;
}

void osp_ic_register_table_init(osp_ic_register_table_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
