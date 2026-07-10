#include "tasks.h"
#include <string.h>

static void octets(osp_value_t *v, const uint8_t *data, uint8_t len) {
	v->tag = OSP_TAG_OCTETSTRING; v->as.octetstring.len = len; memcpy(v->as.octetstring.data, data, len);
}
void osp_spodus_exchange_tasks_init(osp_spodus_exchange_tasks_t *tasks) { if (tasks) memset(tasks, 0, sizeof(*tasks)); }
osp_err_t osp_spodus_exchange_tasks_add(osp_spodus_exchange_tasks_t *tasks, const osp_spodus_exchange_task_t *task) {
	if (!tasks || !task || task->script_count > OSP_SPODUS_MAX_TASK_SCRIPTS) return OSP_ERR_INVALID;
	if (tasks->count >= OSP_SPODUS_MAX_TASKS) return OSP_ERR_NOMEM;
	tasks->entries[tasks->count++] = *task; return OSP_OK;
}
osp_err_t osp_spodus_exchange_tasks_build_value(const osp_spodus_exchange_tasks_t *tasks, osp_value_t *out) {
	if (!tasks || !out) return OSP_ERR_INVALID;
	osp_spodus_exchange_tasks_t *s = (osp_spodus_exchange_tasks_t *)tasks;
	s->value.tag = OSP_TAG_ARRAY; s->value.as.array.elements.items = s->rows; s->value.as.array.elements.count = s->count; s->value.as.array.elements.capacity = OSP_SPODUS_MAX_TASKS;
	for (uint8_t i=0;i<s->count;i++) {
		const osp_spodus_exchange_task_t *t=&s->entries[i]; s->rows[i].tag=OSP_TAG_STRUCTURE; s->rows[i].as.structure.elements.items=s->fields[i]; s->rows[i].as.structure.elements.count=6; s->rows[i].as.structure.elements.capacity=6;
		s->fields[i][0]=osp_val_u16(t->task_id); octets(&s->fields[i][1],t->meter_id,t->meter_id_len);
		s->scripts[i].tag=OSP_TAG_ARRAY; s->scripts[i].as.array.elements.items=s->script_rows[i]; s->scripts[i].as.array.elements.count=t->script_count; s->scripts[i].as.array.elements.capacity=OSP_SPODUS_MAX_TASK_SCRIPTS;
		for(uint8_t j=0;j<t->script_count;j++){ const osp_spodus_task_script_t *x=&t->scripts[j]; osp_value_t *f=s->script_fields[i][j]; s->script_rows[i][j].tag=OSP_TAG_STRUCTURE; s->script_rows[i][j].as.structure.elements.items=f; s->script_rows[i][j].as.structure.elements.count=7; s->script_rows[i][j].as.structure.elements.capacity=7; f[0]=osp_val_u8(x->service_id); f[1]=osp_val_u8(x->class_id); octets(&f[2],(const uint8_t *)&x->obis,6); f[3]=osp_val_u8(x->attribute_id); f[4]=osp_val_null(); f[5]=osp_val_null(); f[6]=osp_val_null(); }
		s->fields[i][2]=s->scripts[i]; s->fields[i][3]=osp_val_u8(t->execution_type); s->fields[i][4].tag=OSP_TAG_ARRAY; s->fields[i][4].as.array.elements.items=NULL; s->fields[i][4].as.array.elements.count=0; s->fields[i][4].as.array.elements.capacity=0; s->fields[i][5]=osp_val_u16(t->priority);
	} *out=s->value; return OSP_OK;
}
