#include "data_hal.h"
#include "server/server.h"
#include "server/dispatcher.h"

osp_hal_data_t *osp_hal_data = NULL;

osp_err_t osp_hal_data_poll(osp_server_t *s) {
	if (!osp_hal_data || !osp_hal_data->read) return OSP_OK;
	for (uint16_t i = 0; i < s->dispatcher.count; i++) {
		osp_object_entry_t *e = &s->dispatcher.objects[i];
		if (!e->class_def || !e->instance) continue;
		if (e->class_def->class_id == 15 || e->class_def->class_id == 17) continue;
		const osp_obis_t *obis = (const osp_obis_t *)e->instance;
		osp_value_t val;
		osp_err_t r = osp_hal_data->read(osp_hal_data->ctx, obis, 2, &val);
		if (r == OSP_OK && e->class_def->set_attr) {
			e->class_def->set_attr(e->instance, 2, &val);
		}
	}
	return OSP_OK;
}
