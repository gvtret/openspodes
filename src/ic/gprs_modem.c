#include "gprs_modem.h"
#include <string.h>
static const osp_ic_class_t ic_gprs_modem = {
    .name = "GPRS Modem Setup", .class_id = 45, .version = 0, .get_attr = NULL, .set_attr = NULL, .invoke = NULL, .instance_size = sizeof(osp_ic_gprs_modem_t)
};

const osp_ic_class_t *osp_ic_gprs_modem_class(void) {
	return &ic_gprs_modem;
}

void osp_ic_gprs_modem_init(osp_ic_gprs_modem_t *i, osp_obis_t ln) {
	memset(i, 0, sizeof(*i));
	i->logical_name = ln;
}
