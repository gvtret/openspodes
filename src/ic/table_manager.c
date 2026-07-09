#include "table_manager.h"
#include <string.h>
static const osp_ic_class_t ic_table_manager = {
    .name = "Table Manager", .class_id = 8200, .version = 0, .get_attr = NULL, .set_attr = NULL, .invoke = NULL, .instance_size = sizeof(osp_ic_table_manager_t)
};

const osp_ic_class_t *osp_ic_table_manager_class(void) {
	return &ic_table_manager;
}

void osp_ic_table_manager_init(osp_ic_table_manager_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
