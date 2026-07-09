#ifndef OSP_IC_ASSOCIATION_LN_H
#define OSP_IC_ASSOCIATION_LN_H
#include "../openspodes.h"
#include "../codec/structures.h"
#define OSP_MAX_OBJECT_LIST_ENTRIES 64

typedef struct {
	osp_obis_t logical_name;
	osp_object_list_t object_list;
	osp_xdms_context_t xdms_context;
	uint8_t association_status; /* 0=idle, 1=associated, 2=pending */
	osp_context_name_t app_context_name;
	osp_user_list_item_t user_list[16];
	uint8_t user_count;
	osp_user_list_item_t current_user;
} osp_ic_association_ln_t;

void osp_ic_association_ln_init(osp_ic_association_ln_t *a, osp_obis_t ln);
const osp_ic_class_t *osp_ic_association_ln_class(void);

/* Object list management */
osp_err_t osp_ic_association_ln_add_object(osp_ic_association_ln_t *a, const osp_object_list_element_t *elem);
osp_err_t osp_ic_association_ln_remove_object(osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln);
const osp_object_list_element_t *osp_ic_association_ln_find_object(const osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln);
bool osp_ic_association_ln_can_read(const osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln, int8_t attr_id);
bool osp_ic_association_ln_can_write(const osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln, int8_t attr_id);
bool osp_ic_association_ln_can_invoke(const osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln, int8_t method_id);

#endif
