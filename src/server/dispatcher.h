/**
 * dispatcher.h — Server request dispatcher (spodes-rs RequestDispatcher)
 *
 * Routes incoming GET/SET/ACTION requests to the addressed IC instance
 * by (class_id, OBIS code).
 */

#ifndef OSP_DISPATCHER_H
#define OSP_DISPATCHER_H

#include "../openspodes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_MAX_OBJECTS 32

typedef struct {
    const osp_ic_class_t *class_def;
    void                 *instance;
} osp_object_entry_t;

typedef struct {
    osp_object_entry_t objects[OSP_MAX_OBJECTS];
    uint8_t            count;
} osp_dispatcher_t;

void osp_dispatcher_init(osp_dispatcher_t *disp);

osp_err_t osp_dispatcher_register(osp_dispatcher_t *disp,
                                  const osp_ic_class_t *class_def,
                                  void *instance);

/* Dispatch a GET/SET/ACTION request */
osp_err_t osp_dispatcher_get(const osp_dispatcher_t *disp,
                             uint16_t class_id, const osp_obis_t *obis,
                             uint8_t attr_id, osp_buf_t *result);

osp_err_t osp_dispatcher_set(osp_dispatcher_t *disp,
                             uint16_t class_id, const osp_obis_t *obis,
                             uint8_t attr_id, const osp_buf_t *data);

osp_err_t osp_dispatcher_action(osp_dispatcher_t *disp,
                                uint16_t class_id, const osp_obis_t *obis,
                                uint8_t method_id, const osp_buf_t *param,
                                osp_buf_t *result);

#ifdef __cplusplus
}
#endif

#endif /* OSP_DISPATCHER_H */
