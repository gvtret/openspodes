/**
 * dispatcher.h — Server request dispatcher (spodes-rs RequestDispatcher)
 *
 * Routes incoming GET/SET/ACTION requests to the addressed IC instance
 * by (class_id, OBIS code).
 */

#ifndef OSP_DISPATCHER_H
#define OSP_DISPATCHER_H

#include "../openspodes.h"
#include "../ic/association_ln.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_MAX_OBJECTS 32

typedef struct {
	const osp_ic_class_t *class_def;
	void *instance;
} osp_object_entry_t;

typedef struct {
	osp_object_entry_t objects[OSP_MAX_OBJECTS];
	uint8_t count;
	osp_ic_association_ln_t *association;
} osp_dispatcher_t;

/** @brief Initialize a dispatcher to an empty state. */
void osp_dispatcher_init(osp_dispatcher_t *disp);

/** @brief Set the association object used for access control lookups. */
void osp_dispatcher_set_association(osp_dispatcher_t *disp, osp_ic_association_ln_t *association);

/** @brief Register an IC class and its instance for request dispatching. */
osp_err_t osp_dispatcher_register(osp_dispatcher_t *disp, const osp_ic_class_t *class_def, void *instance);

/** @brief Dispatch a GET request to read an attribute from the addressed object. */
osp_err_t osp_dispatcher_get(const osp_dispatcher_t *disp, uint16_t class_id, const osp_obis_t *obis, uint8_t attr_id, osp_value_t *result);

/** @brief Dispatch a SET request to update an attribute on the addressed object. */
osp_err_t osp_dispatcher_set(osp_dispatcher_t *disp, uint16_t class_id, const osp_obis_t *obis, uint8_t attr_id, const osp_value_t *value);

/** @brief Dispatch an ACTION request on the addressed object and return the result. */
osp_err_t
osp_dispatcher_action(osp_dispatcher_t *disp, uint16_t class_id, const osp_obis_t *obis, uint8_t method_id, const osp_value_t *param, osp_value_t *result);

#ifdef __cplusplus
}
#endif

#endif /* OSP_DISPATCHER_H */
