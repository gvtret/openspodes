#include "sap_assignment.h"
#include <string.h>
static const osp_ic_class_t ic_sap = {
    .name = "SAP Assignment", .class_id = 17, .version = 0, .get_attr = NULL, .set_attr = NULL, .invoke = NULL, .instance_size = sizeof(osp_ic_sap_assignment_t)
};

const osp_ic_class_t *osp_ic_sap_assignment_class(void) {
	return &ic_sap;
}

void osp_ic_sap_assignment_init(osp_ic_sap_assignment_t *s, osp_obis_t ln) {
	memset(s, 0, sizeof(*s));
	s->logical_name = ln;
}
