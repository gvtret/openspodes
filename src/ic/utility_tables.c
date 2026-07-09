#include "utility_tables.h"
#include <string.h>
static const osp_ic_class_t ic_utility_tables = {
    .name = "Utility Tables", .class_id = 26, .version = 0, .get_attr = NULL, .set_attr = NULL, .invoke = NULL, .instance_size = sizeof(osp_ic_utility_tables_t)
};

const osp_ic_class_t *osp_ic_utility_tables_class(void) {
	return &ic_utility_tables;
}

void osp_ic_utility_tables_init(osp_ic_utility_tables_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
