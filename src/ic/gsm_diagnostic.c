#include "gsm_diagnostic.h"
#include <string.h>
static const osp_ic_class_t ic_gsm_diagnostic = {
    .name = "GSM Diagnostic", .class_id = 47, .version = 0, .get_attr = NULL, .set_attr = NULL, .invoke = NULL, .instance_size = sizeof(osp_ic_gsm_diagnostic_t)
};

const osp_ic_class_t *osp_ic_gsm_diagnostic_class(void) {
	return &ic_gsm_diagnostic;
}

void osp_ic_gsm_diagnostic_init(osp_ic_gsm_diagnostic_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
