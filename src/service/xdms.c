/**
 * xdms.c — xDLMS GET/SET/ACTION encode/decode (IEC 62056-5-3)
 */

#include "service.h"
#include "../codec/codec.h"
#include "../codec/serialize.h"

static int encode_attr_descriptor(osp_buf_t *buf, const osp_attribute_descriptor_t *ad) {
	if (!buf || !ad) {
		return -1;
	}
	if (osp_axdr_write_u16(buf, ad->class_id) != OSP_OK)
		return -1;
	if (osp_obis_write(buf, &ad->instance_id) != OSP_OK)
		return -1;
	if (osp_axdr_write_u8(buf, (uint8_t)ad->attribute_id) != OSP_OK)
		return -1;
	return 0;
}

static int decode_attr_descriptor(osp_buf_t *buf, osp_attribute_descriptor_t *ad) {
	if (!buf || !ad) {
		return -1;
	}
	if (osp_axdr_read_u16(buf, &ad->class_id) != OSP_OK)
		return -1;
	if (osp_obis_read(buf, &ad->instance_id) != OSP_OK)
		return -1;
	if (osp_axdr_read_i8(buf, &ad->attribute_id) != OSP_OK)
		return -1;
	return 0;
}

static int encode_method_descriptor(osp_buf_t *buf, const osp_method_descriptor_t *md) {
	if (!buf || !md) {
		return -1;
	}
	if (osp_axdr_write_u16(buf, md->class_id) != OSP_OK)
		return -1;
	if (osp_obis_write(buf, &md->instance_id) != OSP_OK)
		return -1;
	if (osp_axdr_write_u8(buf, (uint8_t)md->method_id) != OSP_OK)
		return -1;
	return 0;
}

static int decode_method_descriptor(osp_buf_t *buf, osp_method_descriptor_t *md) {
	if (!buf || !md) {
		return -1;
	}
	if (osp_axdr_read_u16(buf, &md->class_id) != OSP_OK)
		return -1;
	if (osp_obis_read(buf, &md->instance_id) != OSP_OK)
		return -1;
	if (osp_axdr_read_i8(buf, &md->method_id) != OSP_OK)
		return -1;
	return 0;
}

static int encode_access_none(osp_buf_t *buf) {
	return osp_axdr_write_u8(buf, 0);
}

static int skip_access_selection(osp_buf_t *buf) {
	uint8_t sel;
	if (osp_axdr_read_u8(buf, &sel) != OSP_OK) {
		return -1;
	}
	if (sel == 1) {
		uint8_t selector;
		if (osp_axdr_read_u8(buf, &selector) != OSP_OK) {
			return -1;
		}
		if (osp_value_skip(buf) != OSP_OK) {
			return -1;
		}
	}
	return 0;
}

int osp_data_block_sa_encode(osp_buf_t *buf, const osp_data_block_t *block) {
	if (!buf || !block) {
		return -1;
	}
	if (osp_axdr_write_u8(buf, block->last_block ? 1 : 0) != OSP_OK)
		return -1;
	if (osp_axdr_write_u32(buf, block->block_number) != OSP_OK)
		return -1;
	if (osp_axdr_push_length(buf, block->raw_data_len) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < block->raw_data_len; i++) {
		osp_axdr_write_u8(buf, block->raw_data[i]);
	}
	return 0;
}

int osp_data_block_sa_decode(osp_buf_t *buf, osp_data_block_t *block) {
	if (!buf || !block) {
		return -1;
	}
	uint8_t last;
	if (osp_axdr_read_u8(buf, &last) != OSP_OK) {
		return -1;
	}
	block->last_block = last != 0;
	if (osp_axdr_read_u32(buf, &block->block_number) != OSP_OK) {
		return -1;
	}
	uint32_t len;
	if (osp_axdr_read_length(buf, &len) != 0) {
		return -1;
	}
	if (len > OSP_MAX_OCTET_LEN || osp_buf_unread(buf) < len) {
		return -1;
	}
	block->raw_data_len = len;
	for (uint32_t i = 0; i < len; i++) {
		osp_axdr_read_u8(buf, &block->raw_data[i]);
	}
	return 0;
}

static int encode_get_result(osp_buf_t *buf, const osp_get_result_item_t *item) {
	if (!item->is_data) {
		osp_axdr_write_u8(buf, 1);
		osp_axdr_write_u8(buf, (uint8_t)item->access_result);
		return 0;
	}
	osp_axdr_write_u8(buf, 0);
	return osp_value_write(buf, &item->data);
}

static int decode_get_result(osp_buf_t *buf, osp_get_result_item_t *item) {
	uint8_t choice;
	if (osp_axdr_read_u8(buf, &choice) != OSP_OK) {
		return -1;
	}
	if (choice == 0) {
		item->is_data = 1;
		return osp_value_read(buf, &item->data);
	}
	if (choice == 1) {
		item->is_data = 0;
		uint8_t code;
		if (osp_axdr_read_u8(buf, &code) != OSP_OK) {
			return -1;
		}
		item->access_result = (osp_dar_t)code;
		return 0;
	}
	return -1;
}

int osp_get_request_encode(osp_buf_t *buf, const osp_get_request_t *req) {
	if (!buf || !req) {
		return -1;
	}
	osp_axdr_write_u8(buf, OSP_TAG_GET_REQUEST);
	switch (req->type) {
		case OSP_GET_NORMAL:
			osp_axdr_write_u8(buf, 1);
			osp_axdr_write_u8(buf, req->invoke_id_priority);
			encode_attr_descriptor(buf, &req->as.normal.attr);
			encode_access_none(buf);
			break;
		case OSP_GET_WITH_BLOCK:
			osp_axdr_write_u8(buf, 2);
			osp_axdr_write_u8(buf, req->invoke_id_priority);
			osp_axdr_write_u32(buf, req->as.next.block_number);
			break;
		case OSP_GET_WITH_LIST:
			osp_axdr_write_u8(buf, 3);
			osp_axdr_write_u8(buf, req->invoke_id_priority);
			osp_axdr_push_length(buf, req->as.with_list.count);
			for (uint8_t i = 0; i < req->as.with_list.count; i++) {
				encode_attr_descriptor(buf, &req->as.with_list.items[i].attr);
				encode_access_none(buf);
			}
			break;
		default:
			return -1;
	}
	return 0;
}

int osp_get_request_decode(osp_buf_t *buf, osp_get_request_t *req) {
	if (!buf || !req) {
		return -1;
	}
	uint8_t tag, type;
	if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_GET_REQUEST) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &type) != OSP_OK) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &req->invoke_id_priority) != OSP_OK) {
		return -1;
	}
	switch (type) {
		case 1:
			req->type = OSP_GET_NORMAL;
			decode_attr_descriptor(buf, &req->as.normal.attr);
			skip_access_selection(buf);
			break;
		case 2:
			req->type = OSP_GET_WITH_BLOCK;
			osp_axdr_read_u32(buf, &req->as.next.block_number);
			break;
		case 3: {
			uint32_t count;
			if (osp_axdr_read_length(buf, &count) != 0 || count > OSP_XDLMS_MAX_LIST) {
				return -1;
			}
			req->type = OSP_GET_WITH_LIST;
			req->as.with_list.count = (uint8_t)count;
			for (uint8_t i = 0; i < req->as.with_list.count; i++) {
				decode_attr_descriptor(buf, &req->as.with_list.items[i].attr);
				skip_access_selection(buf);
			}
			break;
		}
		default:
			return -1;
	}
	return 0;
}

int osp_get_response_encode(osp_buf_t *buf, const osp_get_response_t *resp) {
	if (!buf || !resp) {
		return -1;
	}
	osp_axdr_write_u8(buf, OSP_TAG_GET_RESPONSE);
	switch (resp->type) {
		case OSP_GET_RESP_DATA:
			osp_axdr_write_u8(buf, 1);
			osp_axdr_write_u8(buf, resp->invoke_id_priority);
			osp_axdr_write_u8(buf, 0);
			osp_value_write(buf, &resp->data);
			break;
		case OSP_GET_RESP_DATA_ERROR:
			osp_axdr_write_u8(buf, 1);
			osp_axdr_write_u8(buf, resp->invoke_id_priority);
			osp_axdr_write_u8(buf, 1);
			osp_axdr_write_u8(buf, (uint8_t)resp->data_access_result);
			break;
		case OSP_GET_RESP_BLOCK:
		case OSP_GET_RESP_BLOCK_LAST:
			osp_axdr_write_u8(buf, 2);
			osp_axdr_write_u8(buf, resp->invoke_id_priority);
			osp_axdr_write_u8(buf, resp->type == OSP_GET_RESP_BLOCK_LAST ? 1 : 0);
			osp_axdr_write_u32(buf, resp->data_block.block_number);
			osp_axdr_write_u8(buf, 0);
			osp_axdr_push_length(buf, resp->data_block.raw_data_len);
			for (uint32_t i = 0; i < resp->data_block.raw_data_len; i++) {
				osp_axdr_write_u8(buf, resp->data_block.raw_data[i]);
			}
			break;
		case OSP_GET_RESP_WITH_LIST:
			osp_axdr_write_u8(buf, 3);
			osp_axdr_write_u8(buf, resp->invoke_id_priority);
			osp_axdr_push_length(buf, resp->with_list.count);
			for (uint8_t i = 0; i < resp->with_list.count; i++) {
				encode_get_result(buf, &resp->with_list.items[i]);
			}
			break;
		default:
			return -1;
	}
	return 0;
}

int osp_get_response_decode(osp_buf_t *buf, osp_get_response_t *resp) {
	if (!buf || !resp) {
		return -1;
	}
	uint8_t tag, type;
	if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_GET_RESPONSE) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &type) != OSP_OK) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &resp->invoke_id_priority) != OSP_OK) {
		return -1;
	}
	switch (type) {
		case 1: {
			uint8_t choice;
			if (osp_axdr_read_u8(buf, &choice) != OSP_OK) {
				return -1;
			}
			if (choice == 0) {
				resp->type = OSP_GET_RESP_DATA;
				osp_value_read(buf, &resp->data);
			} else {
				uint8_t code;
				if (osp_axdr_read_u8(buf, &code) != OSP_OK) {
					return -1;
				}
				resp->type = OSP_GET_RESP_DATA_ERROR;
				resp->data_access_result = (osp_dar_t)code;
			}
			break;
		}
		case 2: {
			uint8_t last;
			if (osp_axdr_read_u8(buf, &last) != OSP_OK) {
				return -1;
			}
			resp->type = last ? OSP_GET_RESP_BLOCK_LAST : OSP_GET_RESP_BLOCK;
			osp_axdr_read_u32(buf, &resp->data_block.block_number);
			uint8_t choice;
			if (osp_axdr_read_u8(buf, &choice) != OSP_OK) {
				return -1;
			}
			if (choice != 0) {
				return -1;
			}
			uint32_t len;
			if (osp_axdr_read_length(buf, &len) != 0 || len > OSP_MAX_OCTET_LEN) {
				return -1;
			}
			resp->data_block.last_block = last != 0;
			resp->data_block.raw_data_len = len;
			for (uint32_t i = 0; i < len; i++) {
				osp_axdr_read_u8(buf, &resp->data_block.raw_data[i]);
			}
			break;
		}
		case 3: {
			uint32_t count;
			if (osp_axdr_read_length(buf, &count) != 0 || count > OSP_XDLMS_MAX_LIST) {
				return -1;
			}
			resp->type = OSP_GET_RESP_WITH_LIST;
			resp->with_list.count = (uint8_t)count;
			for (uint8_t i = 0; i < resp->with_list.count; i++) {
				decode_get_result(buf, &resp->with_list.items[i]);
			}
			break;
		}
		default:
			return -1;
	}
	return 0;
}

int osp_set_request_encode(osp_buf_t *buf, const osp_set_request_t *req) {
	if (!buf || !req) {
		return -1;
	}
	osp_axdr_write_u8(buf, OSP_TAG_SET_REQUEST);
	switch (req->type) {
		case OSP_SET_NORMAL:
			osp_axdr_write_u8(buf, 1);
			osp_axdr_write_u8(buf, req->invoke_id_priority);
			encode_attr_descriptor(buf, &req->as.normal.items[0].attr);
			encode_access_none(buf);
			osp_value_write(buf, &req->as.normal.items[0].data);
			break;
		case OSP_SET_WITH_FIRST_DATABLOCK:
			osp_axdr_write_u8(buf, 2);
			osp_axdr_write_u8(buf, req->invoke_id_priority);
			encode_attr_descriptor(buf, &req->as.first_datablock.attr);
			encode_access_none(buf);
			osp_data_block_sa_encode(buf, &req->as.first_datablock.datablock);
			break;
		case OSP_SET_WITH_DATABLOCK:
			osp_axdr_write_u8(buf, 3);
			osp_axdr_write_u8(buf, req->invoke_id_priority);
			osp_data_block_sa_encode(buf, &req->as.datablock.datablock);
			break;
		case OSP_SET_WITH_LIST:
			osp_axdr_write_u8(buf, 4);
			osp_axdr_write_u8(buf, req->invoke_id_priority);
			osp_axdr_push_length(buf, req->as.with_list.count);
			for (uint8_t i = 0; i < req->as.with_list.count; i++) {
				encode_attr_descriptor(buf, &req->as.with_list.items[i].attr);
				encode_access_none(buf);
			}
			osp_axdr_push_length(buf, req->as.with_list.count);
			for (uint8_t i = 0; i < req->as.with_list.count; i++) {
				osp_value_write(buf, &req->as.with_list.items[i].data);
			}
			break;
		default:
			return -1;
	}
	return 0;
}

int osp_set_request_decode(osp_buf_t *buf, osp_set_request_t *req) {
	if (!buf || !req) {
		return -1;
	}
	uint8_t tag, type;
	if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_SET_REQUEST) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &type) != OSP_OK) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &req->invoke_id_priority) != OSP_OK) {
		return -1;
	}
	switch (type) {
		case 1:
			req->type = OSP_SET_NORMAL;
			req->as.normal.item_count = 1;
			decode_attr_descriptor(buf, &req->as.normal.items[0].attr);
			skip_access_selection(buf);
			osp_value_read(buf, &req->as.normal.items[0].data);
			break;
		case 2:
			req->type = OSP_SET_WITH_FIRST_DATABLOCK;
			decode_attr_descriptor(buf, &req->as.first_datablock.attr);
			skip_access_selection(buf);
			osp_data_block_sa_decode(buf, &req->as.first_datablock.datablock);
			break;
		case 3:
			req->type = OSP_SET_WITH_DATABLOCK;
			osp_data_block_sa_decode(buf, &req->as.datablock.datablock);
			break;
		case 4: {
			uint32_t count;
			if (osp_axdr_read_length(buf, &count) != 0 || count > OSP_XDLMS_MAX_LIST) {
				return -1;
			}
			req->type = OSP_SET_WITH_LIST;
			req->as.with_list.count = (uint8_t)count;
			for (uint8_t i = 0; i < req->as.with_list.count; i++) {
				decode_attr_descriptor(buf, &req->as.with_list.items[i].attr);
				skip_access_selection(buf);
			}
			uint32_t vcount;
			if (osp_axdr_read_length(buf, &vcount) != 0 || vcount != count) {
				return -1;
			}
			for (uint8_t i = 0; i < req->as.with_list.count; i++) {
				osp_value_read(buf, &req->as.with_list.items[i].data);
			}
			break;
		}
		default:
			return -1;
	}
	return 0;
}

int osp_set_response_encode(osp_buf_t *buf, const osp_set_response_t *resp) {
	if (!buf || !resp) {
		return -1;
	}
	osp_axdr_write_u8(buf, OSP_TAG_SET_RESPONSE);
	switch (resp->type) {
		case OSP_SET_RESP_NORMAL:
			osp_axdr_write_u8(buf, 1);
			osp_axdr_write_u8(buf, resp->invoke_id_priority);
			osp_axdr_write_u8(buf, (uint8_t)resp->as.normal.result);
			break;
		case OSP_SET_RESP_DATABLOCK:
			osp_axdr_write_u8(buf, 2);
			osp_axdr_write_u8(buf, resp->invoke_id_priority);
			osp_axdr_write_u32(buf, resp->as.datablock.block_number);
			break;
		case OSP_SET_RESP_LAST_DATABLOCK:
			osp_axdr_write_u8(buf, 3);
			osp_axdr_write_u8(buf, resp->invoke_id_priority);
			osp_axdr_write_u8(buf, (uint8_t)resp->as.last_datablock.result);
			osp_axdr_write_u32(buf, resp->as.last_datablock.block_number);
			break;
		case OSP_SET_RESP_WITH_LIST:
			osp_axdr_write_u8(buf, 5);
			osp_axdr_write_u8(buf, resp->invoke_id_priority);
			osp_axdr_push_length(buf, resp->as.with_list.count);
			for (uint8_t i = 0; i < resp->as.with_list.count; i++) {
				osp_axdr_write_u8(buf, (uint8_t)resp->as.with_list.results[i]);
			}
			break;
		default:
			return -1;
	}
	return 0;
}

int osp_set_response_decode(osp_buf_t *buf, osp_set_response_t *resp) {
	if (!buf || !resp) {
		return -1;
	}
	uint8_t tag, type;
	if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_SET_RESPONSE) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &type) != OSP_OK) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &resp->invoke_id_priority) != OSP_OK) {
		return -1;
	}
	switch (type) {
		case 1: {
			uint8_t dar;
			if (osp_axdr_read_u8(buf, &dar) != OSP_OK) {
				return -1;
			}
			resp->type = OSP_SET_RESP_NORMAL;
			resp->as.normal.result = (osp_dar_t)dar;
			break;
		}
		case 2:
			resp->type = OSP_SET_RESP_DATABLOCK;
			osp_axdr_read_u32(buf, &resp->as.datablock.block_number);
			break;
		case 3: {
			uint8_t dar;
			if (osp_axdr_read_u8(buf, &dar) != OSP_OK) {
				return -1;
			}
			resp->type = OSP_SET_RESP_LAST_DATABLOCK;
			resp->as.last_datablock.result = (osp_dar_t)dar;
			osp_axdr_read_u32(buf, &resp->as.last_datablock.block_number);
			break;
		}
		case 5: {
			uint32_t count;
			if (osp_axdr_read_length(buf, &count) != 0 || count > OSP_XDLMS_MAX_LIST) {
				return -1;
			}
			resp->type = OSP_SET_RESP_WITH_LIST;
			resp->as.with_list.count = (uint8_t)count;
			for (uint8_t i = 0; i < resp->as.with_list.count; i++) {
				uint8_t dar;
				if (osp_axdr_read_u8(buf, &dar) != OSP_OK) {
					return -1;
				}
				resp->as.with_list.results[i] = (osp_dar_t)dar;
			}
			break;
		}
		default:
			return -1;
	}
	return 0;
}

int osp_action_request_encode(osp_buf_t *buf, const osp_action_request_t *req) {
	if (!buf || !req) {
		return -1;
	}
	osp_axdr_write_u8(buf, OSP_TAG_ACTION_REQUEST);
	switch (req->type) {
		case OSP_ACTION_NORMAL:
			osp_axdr_write_u8(buf, 1);
			osp_axdr_write_u8(buf, req->invoke_id_priority);
			encode_method_descriptor(buf, &req->as.normal.items[0].method);
			if (req->as.normal.items[0].data.tag == OSP_TAG_NULL) {
				osp_axdr_write_u8(buf, 0);
			} else {
				osp_axdr_write_u8(buf, 1);
				osp_value_write(buf, &req->as.normal.items[0].data);
			}
			break;
		case OSP_ACTION_WITH_LIST:
			osp_axdr_write_u8(buf, 3);
			osp_axdr_write_u8(buf, req->invoke_id_priority);
			osp_axdr_push_length(buf, req->as.with_list.count);
			for (uint8_t i = 0; i < req->as.with_list.count; i++) {
				encode_method_descriptor(buf, &req->as.with_list.items[i].method);
				if (req->as.with_list.items[i].data.tag == OSP_TAG_NULL) {
					osp_axdr_write_u8(buf, 0);
				} else {
					osp_axdr_write_u8(buf, 1);
					osp_value_write(buf, &req->as.with_list.items[i].data);
				}
			}
			break;
		case OSP_ACTION_NEXT_PARAM_BLOCK:
			osp_axdr_write_u8(buf, 2);
			osp_axdr_write_u8(buf, req->invoke_id_priority);
			osp_axdr_write_u32(buf, req->as.next_param_block.block_number);
			break;
		case OSP_ACTION_WITH_FIRST_PARAM_BLOCK:
			osp_axdr_write_u8(buf, 4);
			osp_axdr_write_u8(buf, req->invoke_id_priority);
			encode_method_descriptor(buf, &req->as.first_param_block.method);
			osp_data_block_sa_encode(buf, &req->as.first_param_block.param_block);
			break;
		case OSP_ACTION_WITH_PARAM_BLOCK:
			osp_axdr_write_u8(buf, 6);
			osp_axdr_write_u8(buf, req->invoke_id_priority);
			osp_data_block_sa_encode(buf, &req->as.with_param_block.param_block);
			break;
		default:
			return -1;
	}
	return 0;
}

int osp_action_request_decode(osp_buf_t *buf, osp_action_request_t *req) {
	if (!buf || !req) {
		return -1;
	}
	uint8_t tag, type;
	if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_ACTION_REQUEST) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &type) != OSP_OK) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &req->invoke_id_priority) != OSP_OK) {
		return -1;
	}
	switch (type) {
		case 1:
			req->type = OSP_ACTION_NORMAL;
			req->as.normal.item_count = 1;
			decode_method_descriptor(buf, &req->as.normal.items[0].method);
			{
				uint8_t have_data;
				if (osp_axdr_read_u8(buf, &have_data) != OSP_OK) {
					return -1;
				}
				if (have_data) {
					osp_value_read(buf, &req->as.normal.items[0].data);
				} else {
					req->as.normal.items[0].data = osp_val_null();
				}
			}
			break;
		case 3: {
			uint32_t count;
			if (osp_axdr_read_length(buf, &count) != 0 || count > OSP_XDLMS_MAX_LIST) {
				return -1;
			}
			req->type = OSP_ACTION_WITH_LIST;
			req->as.with_list.count = (uint8_t)count;
			for (uint8_t i = 0; i < req->as.with_list.count; i++) {
				decode_method_descriptor(buf, &req->as.with_list.items[i].method);
				uint8_t have_data;
				if (osp_axdr_read_u8(buf, &have_data) != OSP_OK) {
					return -1;
				}
				if (have_data) {
					osp_value_read(buf, &req->as.with_list.items[i].data);
				} else {
					req->as.with_list.items[i].data = osp_val_null();
				}
			}
			break;
		}
		case 2:
			req->type = OSP_ACTION_NEXT_PARAM_BLOCK;
			osp_axdr_read_u32(buf, &req->as.next_param_block.block_number);
			break;
		case 4:
			req->type = OSP_ACTION_WITH_FIRST_PARAM_BLOCK;
			decode_method_descriptor(buf, &req->as.first_param_block.method);
			osp_data_block_sa_decode(buf, &req->as.first_param_block.param_block);
			break;
		case 6:
			req->type = OSP_ACTION_WITH_PARAM_BLOCK;
			osp_data_block_sa_decode(buf, &req->as.with_param_block.param_block);
			break;
		default:
			return -1;
	}
	return 0;
}

int osp_action_response_encode(osp_buf_t *buf, const osp_action_response_t *resp) {
	if (!buf || !resp) {
		return -1;
	}
	osp_axdr_write_u8(buf, OSP_TAG_ACTION_RESPONSE);
	switch (resp->type) {
		case OSP_ACTION_RESP_NORMAL:
			osp_axdr_write_u8(buf, 1);
			osp_axdr_write_u8(buf, resp->invoke_id_priority);
			osp_axdr_write_u8(buf, (uint8_t)resp->as.normal.items[0].result);
			if (resp->as.normal.items[0].return_data.tag != OSP_TAG_NULL) {
				osp_axdr_write_u8(buf, 1);
				osp_axdr_write_u8(buf, 0);
				osp_value_write(buf, &resp->as.normal.items[0].return_data);
			} else {
				osp_axdr_write_u8(buf, 0);
			}
			break;
		case OSP_ACTION_RESP_WITH_LIST:
			osp_axdr_write_u8(buf, 3);
			osp_axdr_write_u8(buf, resp->invoke_id_priority);
			osp_axdr_push_length(buf, resp->as.with_list.count);
			for (uint8_t i = 0; i < resp->as.with_list.count; i++) {
				osp_axdr_write_u8(buf, (uint8_t)resp->as.with_list.items[i].result);
				if (resp->as.with_list.items[i].return_data.tag != OSP_TAG_NULL) {
					osp_axdr_write_u8(buf, 1);
					osp_axdr_write_u8(buf, 0);
					osp_value_write(buf, &resp->as.with_list.items[i].return_data);
				} else {
					osp_axdr_write_u8(buf, 0);
				}
			}
			break;
		case OSP_ACTION_RESP_WITH_PARAM_BLOCK:
			osp_axdr_write_u8(buf, 2);
			osp_axdr_write_u8(buf, resp->invoke_id_priority);
			osp_data_block_sa_encode(buf, &resp->as.with_param_block.param_block);
			break;
		case OSP_ACTION_RESP_NEXT_PARAM_BLOCK:
			osp_axdr_write_u8(buf, 4);
			osp_axdr_write_u8(buf, resp->invoke_id_priority);
			osp_axdr_write_u32(buf, resp->as.next_param_block.block_number);
			break;
		default:
			return -1;
	}
	return 0;
}

int osp_action_response_decode(osp_buf_t *buf, osp_action_response_t *resp) {
	if (!buf || !resp) {
		return -1;
	}
	uint8_t tag, type;
	if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_ACTION_RESPONSE) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &type) != OSP_OK) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &resp->invoke_id_priority) != OSP_OK) {
		return -1;
	}
	switch (type) {
		case 1:
			resp->type = OSP_ACTION_RESP_NORMAL;
			resp->as.normal.item_count = 1;
			{
				uint8_t dar;
				if (osp_axdr_read_u8(buf, &dar) != OSP_OK) {
					return -1;
				}
				resp->as.normal.items[0].result = (osp_dar_t)dar;
				uint8_t have_return;
				if (osp_axdr_read_u8(buf, &have_return) != OSP_OK) {
					return -1;
				}
				if (have_return) {
					uint8_t data_tag;
					if (osp_axdr_read_u8(buf, &data_tag) != OSP_OK) {
						return -1;
					}
					(void)data_tag;
					osp_value_read(buf, &resp->as.normal.items[0].return_data);
				} else {
					resp->as.normal.items[0].return_data = osp_val_null();
				}
			}
			break;
		case 3: {
			uint32_t count;
			if (osp_axdr_read_length(buf, &count) != 0 || count > OSP_XDLMS_MAX_LIST) {
				return -1;
			}
			resp->type = OSP_ACTION_RESP_WITH_LIST;
			resp->as.with_list.count = (uint8_t)count;
			for (uint8_t i = 0; i < resp->as.with_list.count; i++) {
				uint8_t dar;
				if (osp_axdr_read_u8(buf, &dar) != OSP_OK) {
					return -1;
				}
				resp->as.with_list.items[i].result = (osp_dar_t)dar;
				uint8_t have_return;
				if (osp_axdr_read_u8(buf, &have_return) != OSP_OK) {
					return -1;
				}
				if (have_return) {
					uint8_t data_tag;
					if (osp_axdr_read_u8(buf, &data_tag) != OSP_OK) {
						return -1;
					}
					(void)data_tag;
					osp_value_read(buf, &resp->as.with_list.items[i].return_data);
				} else {
					resp->as.with_list.items[i].return_data = osp_val_null();
				}
			}
			break;
		}
		case 2:
			resp->type = OSP_ACTION_RESP_WITH_PARAM_BLOCK;
			osp_data_block_sa_decode(buf, &resp->as.with_param_block.param_block);
			break;
		case 4:
			resp->type = OSP_ACTION_RESP_NEXT_PARAM_BLOCK;
			osp_axdr_read_u32(buf, &resp->as.next_param_block.block_number);
			break;
		default:
			return -1;
	}
	return 0;
}
