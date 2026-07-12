/**
 * concentrator.h — SPODUS IVCV runtime: registry, direct proxy, downstream poll
 */

#ifndef OSP_SPODUS_CONCENTRATOR_H
#define OSP_SPODUS_CONCENTRATOR_H

#include "direct_channel.h"
#include "channel_list.h"
#include "discovered.h"
#include "access_policy.h"
#include "tasks.h"
#include "../client/client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct osp_spodus_concentrator osp_spodus_concentrator_t;

typedef enum {
	OSP_SPODUS_DATA_METER_LIST = 0,
	OSP_SPODUS_DATA_DIRECT_TABLE = 1,
	OSP_SPODUS_DATA_ACCESS_POLICIES = 2,
	OSP_SPODUS_DATA_EXCHANGE_TASKS = 3,
} osp_spodus_data_kind_t;

typedef struct {
	osp_obis_t logical_name;
	osp_spodus_concentrator_t *conc;
	osp_spodus_data_kind_t kind;
} osp_ic_spodus_data_t;

typedef struct {
	osp_ic_spodus_data_t meter_list;
	osp_ic_spodus_data_t direct_table;
	osp_ic_spodus_data_t access_policies;
	osp_ic_spodus_data_t exchange_tasks;
	osp_ic_profile_generic_t channel_list;
	osp_ic_profile_generic_t discovered_meters;
} osp_spodus_server_objects_t;

typedef struct {
	uint8_t meter_id_len;
	uint8_t meter_id[OSP_SPODUS_MAX_METER_ID_LEN];
	osp_transport_t *transport;
	osp_framing_type_t framing;
	osp_client_t client;
	bool connected;
} osp_spodus_downstream_t;

struct osp_spodus_concentrator {
	osp_spodus_meter_registry_t registry;
	osp_spodus_direct_channel_table_t direct;
	osp_spodus_channel_list_t channels;
	osp_spodus_discovered_list_t discovered;
	osp_spodus_access_policies_t access_policies;
	osp_spodus_exchange_tasks_t exchange_tasks;
	osp_spodus_downstream_t downstream[OSP_SPODUS_MAX_METERS];
	uint8_t downstream_count;
	osp_spodus_server_objects_t server_objects;
};

typedef struct {
	uint16_t class_id;
	osp_obis_t obis;
	uint8_t attribute_id;
} osp_spodus_attr_ref_t;

void osp_spodus_concentrator_init(osp_spodus_concentrator_t *c);

osp_err_t osp_spodus_concentrator_attach_downstream(osp_spodus_concentrator_t *c, const uint8_t *meter_id, uint8_t meter_id_len,
                                                    osp_transport_t *transport, osp_framing_type_t framing);

osp_spodus_downstream_t *osp_spodus_concentrator_downstream(osp_spodus_concentrator_t *c, const uint8_t *meter_id, uint8_t meter_id_len);

osp_err_t osp_spodus_concentrator_connect_downstream(osp_spodus_concentrator_t *c, const uint8_t *meter_id, uint8_t meter_id_len,
                                                     uint32_t timeout_ms);

uint32_t osp_spodus_poll_meter(osp_client_t *client, osp_spodus_meter_registry_t *registry, const uint8_t *meter_id, uint8_t meter_id_len,
                               const osp_spodus_attr_ref_t *attributes, uint8_t count);

osp_err_t osp_spodus_proxy_forward(osp_spodus_concentrator_t *c, uint16_t direct_id, const uint8_t *request, uint32_t request_len,
                                   uint8_t *response, uint32_t response_size, uint32_t *response_len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OSP_SPODUS_CONCENTRATOR_H */
