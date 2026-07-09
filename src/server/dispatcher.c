/**
 * dispatcher.c — Server request dispatcher
 *
 * Routes requests to IC instances by (class_id, OBIS code).
 * Zero dynamic allocation: objects are pre-registered.
 */

#include "dispatcher.h"
#include <string.h>

void osp_dispatcher_init(osp_dispatcher_t *disp)
{
    if (!disp) return;
    memset(disp, 0, sizeof(*disp));
}

osp_err_t osp_dispatcher_register(osp_dispatcher_t *disp,
                                  const osp_ic_class_t *class_def,
                                  void *instance)
{
    if (!disp || !class_def || !instance) return OSP_ERR_INVALID;
    if (disp->count >= OSP_MAX_OBJECTS) return OSP_ERR_NOMEM;

    disp->objects[disp->count].class_def = class_def;
    disp->objects[disp->count].instance  = instance;
    disp->count++;
    return OSP_OK;
}

static osp_object_entry_t *find_object(osp_dispatcher_t *disp,
                                        uint16_t class_id,
                                        const osp_obis_t *obis)
{
    for (uint8_t i = 0; i < disp->count; i++) {
        osp_object_entry_t *e = &disp->objects[i];
        if (e->class_def->class_id == class_id) {
            return e;
        }
    }
    return NULL;
}

osp_err_t osp_dispatcher_get(const osp_dispatcher_t *disp,
                             uint16_t class_id, const osp_obis_t *obis,
                             uint8_t attr_id, osp_value_t *result)
{
    (void) obis;
    const osp_object_entry_t *e = find_object((osp_dispatcher_t *)disp, class_id, obis);
    if (!e || !e->class_def->get_attr) return OSP_ERR_NOT_FOUND;
    return e->class_def->get_attr(e->instance, attr_id, result);
}

osp_err_t osp_dispatcher_set(osp_dispatcher_t *disp,
                             uint16_t class_id, const osp_obis_t *obis,
                             uint8_t attr_id, const osp_value_t *value)
{
    (void) obis;
    osp_object_entry_t *e = find_object(disp, class_id, obis);
    if (!e || !e->class_def->set_attr) return OSP_ERR_NOT_FOUND;
    return e->class_def->set_attr((void *)e->instance, attr_id, value);
}

osp_err_t osp_dispatcher_action(osp_dispatcher_t *disp,
                                uint16_t class_id, const osp_obis_t *obis,
                                uint8_t method_id, const osp_value_t *param,
                                osp_value_t *result)
{
    (void) obis;
    osp_object_entry_t *e = find_object(disp, class_id, obis);
    if (!e || !e->class_def->invoke) return OSP_ERR_NOT_FOUND;
    return e->class_def->invoke((void *)e->instance, method_id, param, result);
}
