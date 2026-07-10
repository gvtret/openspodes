#include "notification.h"
#include "../codec/codec.h"
#include "../codec/serialize.h"

static int encode_attr_descriptor(osp_buf_t *buf, const osp_attribute_descriptor_t *ad) {
	osp_axdr_write_u16(buf, ad->class_id);
	osp_obis_write(buf, &ad->instance_id);
	osp_axdr_write_u8(buf, (uint8_t)ad->attribute_id);
	return 0;
}

static int decode_attr_descriptor(osp_buf_t *buf, osp_attribute_descriptor_t *ad) {
	osp_axdr_read_u16(buf, &ad->class_id);
	osp_obis_read(buf, &ad->instance_id);
	osp_axdr_read_i8(buf, &ad->attribute_id);
	return 0;
}

int osp_event_notification_encode(osp_buf_t *buf, const osp_event_notification_t *ev) {
	if (!buf || !ev) {
		return -1;
	}
	osp_axdr_write_u8(buf, OSP_TAG_EVENT_NOTIFICATION_REQ);
	if (!ev->has_time) {
		osp_axdr_write_u8(buf, 0);
	} else {
		osp_axdr_write_u8(buf, 1);
		osp_axdr_push_length(buf, ev->time_len);
		for (uint8_t i = 0; i < ev->time_len; i++) {
			osp_axdr_write_u8(buf, ev->time[i]);
		}
	}
	encode_attr_descriptor(buf, &ev->attribute);
	return osp_value_write(buf, &ev->value);
}

int osp_event_notification_decode(osp_buf_t *buf, osp_event_notification_t *ev) {
	if (!buf || !ev) {
		return -1;
	}
	uint8_t tag, time_sel;
	if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_EVENT_NOTIFICATION_REQ) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &time_sel) != OSP_OK) {
		return -1;
	}
	ev->has_time = 0;
	ev->time_len = 0;
	if (time_sel == 1) {
		uint32_t len;
		if (osp_axdr_read_length(buf, &len) != 0 || len > OSP_COSEM_DATETIME_LEN) {
			return -1;
		}
		ev->has_time = 1;
		ev->time_len = (uint8_t)len;
		for (uint32_t i = 0; i < len; i++) {
			osp_axdr_read_u8(buf, &ev->time[i]);
		}
	} else if (time_sel != 0) {
		return -1;
	}
	decode_attr_descriptor(buf, &ev->attribute);
	return osp_value_read(buf, &ev->value);
}

int osp_data_notification_encode(osp_buf_t *buf, const osp_data_notification_t *dn) {
	if (!buf || !dn) {
		return -1;
	}
	osp_axdr_write_u8(buf, OSP_TAG_DATA_NOTIFICATION);
	osp_axdr_write_u32(buf, dn->long_invoke_id_and_priority);
	osp_axdr_push_length(buf, dn->date_time_len);
	for (uint8_t i = 0; i < dn->date_time_len; i++) {
		osp_axdr_write_u8(buf, dn->date_time[i]);
	}
	return osp_value_write(buf, &dn->notification_body);
}

int osp_data_notification_decode(osp_buf_t *buf, osp_data_notification_t *dn) {
	if (!buf || !dn) {
		return -1;
	}
	uint8_t tag;
	if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_DATA_NOTIFICATION) {
		return -1;
	}
	if (osp_axdr_read_u32(buf, &dn->long_invoke_id_and_priority) != OSP_OK) {
		return -1;
	}
	uint32_t len;
	if (osp_axdr_read_length(buf, &len) != 0 || len > OSP_COSEM_DATETIME_LEN) {
		return -1;
	}
	dn->date_time_len = (uint8_t)len;
	for (uint32_t i = 0; i < len; i++) {
		osp_axdr_read_u8(buf, &dn->date_time[i]);
	}
	return osp_value_read(buf, &dn->notification_body);
}
