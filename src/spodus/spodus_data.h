/**
 * spodus_data.h — Register concentrator Data objects on the server dispatcher
 */

#ifndef OSP_SPODUS_DATA_H
#define OSP_SPODUS_DATA_H

#include "concentrator.h"
#include "../server/server.h"

#ifdef __cplusplus
extern "C" {
#endif

void osp_ic_spodus_data_init(osp_ic_spodus_data_t *obj, osp_obis_t ln, osp_spodus_concentrator_t *conc, osp_spodus_data_kind_t kind);

const osp_ic_class_t *osp_ic_spodus_data_class(void);

osp_err_t osp_spodus_concentrator_register_server(osp_server_t *server, osp_spodus_concentrator_t *conc);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SPODUS_DATA_H */
