/**
 * @file notification.h
 * @brief DLMS/COSEM notification APDUs (unsolicited messages).
 *
 * Provides encode/decode for:
 * - Data Notification (0x0F): unsolicited data push from server
 * - Event Notification (0xC2): unsolicited event push from server
 */

#ifndef OSP_NOTIFICATION_H
#define OSP_NOTIFICATION_H

#include "../openspodes.h"
#include "service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint8_t has_time;
	uint8_t time[OSP_COSEM_DATETIME_LEN];
	uint8_t time_len;
	osp_attribute_descriptor_t attribute;
	osp_value_t value;
} osp_event_notification_t;

typedef struct {
	uint32_t long_invoke_id_and_priority;
	uint8_t date_time[OSP_COSEM_DATETIME_LEN];
	uint8_t date_time_len;
	osp_value_t notification_body;
} osp_data_notification_t;

int osp_event_notification_encode(osp_buf_t *buf, const osp_event_notification_t *ev);
int osp_event_notification_decode(osp_buf_t *buf, osp_event_notification_t *ev);
int osp_data_notification_encode(osp_buf_t *buf, const osp_data_notification_t *dn);
int osp_data_notification_decode(osp_buf_t *buf, osp_data_notification_t *dn);

#ifdef __cplusplus
}
#endif

#endif /* OSP_NOTIFICATION_H */
