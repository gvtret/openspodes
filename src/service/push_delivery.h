/**
 * push_delivery.h — Outbound Push (DataNotification) scheduling hook
 */
#ifndef OSP_PUSH_DELIVERY_H
#define OSP_PUSH_DELIVERY_H

#include "../openspodes.h"
#include "../server/server.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_PUSH_DEST_MAX 128
#define OSP_PUSH_BODY_MAX OSP_SERVER_PENDING_MAX

typedef struct {
	uint8_t destination[OSP_PUSH_DEST_MAX];
	uint32_t destination_len;
	uint8_t transport_service;
	int8_t client_sap;
	uint8_t body[OSP_PUSH_BODY_MAX];
	uint32_t body_len;
} osp_push_delivery_request_t;

typedef osp_err_t (*osp_push_delivery_fn)(const osp_push_delivery_request_t *req, void *ctx);

void osp_push_set_delivery_fn(osp_push_delivery_fn fn, void *ctx);

/** Returns OSP_OK if a handler accepted the job, OSP_ERR_NOT_FOUND if none. */
osp_err_t osp_push_schedule_delivery(const osp_push_delivery_request_t *req);

#ifdef __cplusplus
}
#endif

#endif
