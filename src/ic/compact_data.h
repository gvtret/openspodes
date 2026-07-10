#ifndef OSP_IC_COMPACT_DATA_H
#define OSP_IC_COMPACT_DATA_H

#include "../openspodes.h"
#include "../codec/structures.h"
#include "../server/dispatcher.h"

#ifndef OSP_COMPACT_DATA_BUFFER_MAX
#define OSP_COMPACT_DATA_BUFFER_MAX 512
#endif

typedef enum {
	OSP_COMPACT_CAPTURE_ALL = 0,
	OSP_COMPACT_CAPTURE_LAST = 1,
} osp_compact_capture_method_t;

typedef struct {
	osp_obis_t logical_name;
	uint8_t compact_buffer[OSP_COMPACT_DATA_BUFFER_MAX];
	uint32_t compact_buffer_len;
	osp_capture_object_list_t capture_objects;
	uint8_t template_id;
	uint8_t template_description[64];
	uint32_t template_description_len;
	osp_compact_capture_method_t capture_method;
	osp_value_t capture_values[OSP_MAX_CAPTURE_OBJECTS];
	uint8_t capture_value_count;
	osp_dispatcher_t *dispatcher;
} osp_ic_compact_data_t;

void osp_ic_compact_data_init(osp_ic_compact_data_t *i, osp_obis_t ln);
void osp_ic_compact_data_bind_dispatcher(osp_ic_compact_data_t *i, osp_dispatcher_t *disp);
void osp_ic_compact_data_set_capture_values(osp_ic_compact_data_t *i, const osp_value_t *values, uint8_t count);
const osp_ic_class_t *osp_ic_compact_data_class(void);

#endif
