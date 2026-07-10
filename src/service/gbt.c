#include "gbt.h"
#include "../codec/codec.h"
#include <string.h>

#define OSP_GBT_LAST_BLOCK 0x80u
#define OSP_GBT_STREAMING  0x40u

static osp_err_t gbt_send_block(osp_transport_t *transport, osp_framing_type_t framing, const osp_general_block_transfer_t *gbt,
                                uint8_t *tx_scratch, uint32_t tx_scratch_size) {
	osp_buf_t buf;
	osp_buf_init(&buf, tx_scratch, tx_scratch_size);
	if (osp_gbt_encode(&buf, gbt) != 0) {
		return OSP_ERR_INVALID;
	}
	return osp_transport_send_apdu(transport, framing, buf.buf, buf.wr);
}

static osp_err_t gbt_recv_block(osp_transport_t *transport, osp_framing_type_t framing, osp_general_block_transfer_t *gbt,
                                uint8_t *rx_scratch, uint32_t rx_scratch_size, uint32_t timeout_ms) {
	uint32_t rx_len = 0;
	osp_err_t r = osp_transport_recv_apdu(transport, framing, rx_scratch, rx_scratch_size, &rx_len, timeout_ms);
	if (r != OSP_OK) {
		return r;
	}
	if (rx_len == 0 || rx_scratch[0] != OSP_TAG_GENERAL_BLOCK_TRANSFER) {
		return OSP_ERR_INVALID;
	}
	osp_buf_t buf;
	osp_buf_init(&buf, rx_scratch, rx_len);
	buf.wr = rx_len;
	if (osp_gbt_decode(&buf, gbt) != 0) {
		return OSP_ERR_INVALID;
	}
	return OSP_OK;
}

static osp_err_t gbt_send_ack(osp_transport_t *transport, osp_framing_type_t framing, uint8_t window, uint16_t block_number,
                              uint16_t block_number_ack, uint8_t *tx_scratch, uint32_t tx_scratch_size) {
	osp_general_block_transfer_t ack = {0};
	ack.last_block = true;
	ack.streaming = false;
	ack.window = window & OSP_GBT_WINDOW_MASK;
	ack.block_number = block_number;
	ack.block_number_ack = block_number_ack;
	ack.block_data_len = 0;
	return gbt_send_block(transport, framing, &ack, tx_scratch, tx_scratch_size);
}

static osp_err_t gbt_wait_for_ack(osp_transport_t *transport, osp_framing_type_t framing, uint16_t block_number_ack, uint8_t *rx_scratch,
                                  uint32_t rx_scratch_size, uint32_t timeout_ms, osp_general_block_transfer_t *ack_out) {
	osp_general_block_transfer_t ack;
	osp_err_t r = gbt_recv_block(transport, framing, &ack, rx_scratch, rx_scratch_size, timeout_ms);
	if (r != OSP_OK) {
		return r;
	}
	if (ack.block_number_ack < block_number_ack) {
		return OSP_ERR_INVALID;
	}
	if (ack_out) {
		*ack_out = ack;
	}
	return OSP_OK;
}

osp_err_t osp_gbt_transport_send(osp_transport_t *transport, osp_framing_type_t framing, const uint8_t *apdu, uint32_t apdu_len,
                                 uint32_t block_payload_max, uint8_t window, uint8_t *tx_scratch, uint32_t tx_scratch_size,
                                 uint8_t *rx_scratch, uint32_t rx_scratch_size, uint32_t timeout_ms) {
	if (!transport || !apdu || apdu_len == 0 || apdu_len > OSP_GBT_MAX_APDU || block_payload_max == 0 || block_payload_max > OSP_MAX_OCTET_LEN ||
	    !tx_scratch || !rx_scratch) {
		return OSP_ERR_INVALID;
	}

	uint8_t win = window & OSP_GBT_WINDOW_MASK;
	uint32_t offset = 0;
	uint16_t block_number = 1;
	uint16_t blocks_in_window = 0;

	while (offset < apdu_len) {
		uint32_t chunk = apdu_len - offset;
		if (chunk > block_payload_max) {
			chunk = block_payload_max;
		}
		bool last = (offset + chunk >= apdu_len);

		osp_general_block_transfer_t gbt = {0};
		gbt.last_block = last;
		gbt.streaming = false;
		gbt.window = win;
		gbt.block_number = block_number;
		gbt.block_number_ack = 0;
		gbt.block_data_len = chunk;
		memcpy(gbt.block_data, &apdu[offset], chunk);

		osp_err_t r = gbt_send_block(transport, framing, &gbt, tx_scratch, tx_scratch_size);
		if (r != OSP_OK) {
			return r;
		}

		offset += chunk;
		blocks_in_window++;
		block_number++;

		/* Confirmed mode: wait for ack after each window (not after the last block). */
		if (win > 0 && !last && blocks_in_window >= win) {
			osp_general_block_transfer_t ack;
			r = gbt_wait_for_ack(transport, framing, block_number - 1, rx_scratch, rx_scratch_size, timeout_ms, &ack);
			if (r != OSP_OK) {
				return r;
			}
			/* Peer reports missing blocks — retransmit from the first unconfirmed block. */
			if (ack.block_number_ack + 1 < block_number) {
				block_number = (uint16_t)(ack.block_number_ack + 1);
				offset = (uint32_t)(block_number - 1) * block_payload_max;
				if (offset >= apdu_len) {
					offset = 0;
					block_number = 1;
				}
				blocks_in_window = 0;
				continue;
			}
			blocks_in_window = 0;
		}
	}
	return OSP_OK;
}

osp_err_t osp_gbt_transport_recv(osp_transport_t *transport, osp_framing_type_t framing, uint8_t *rx_scratch, uint32_t rx_scratch_size,
                                 uint8_t *apdu, uint32_t apdu_size, uint32_t *apdu_len, uint8_t *tx_scratch, uint32_t tx_scratch_size,
                                 uint32_t timeout_ms, const uint8_t *first_block, uint32_t first_block_len) {
	if (!transport || !rx_scratch || !apdu || !apdu_len || !tx_scratch) {
		return OSP_ERR_INVALID;
	}

	uint32_t acc_len = 0;
	uint16_t expected_block = 1;
	uint8_t peer_window = 0;
	bool have_first = (first_block != NULL && first_block_len > 0);

	while (true) {
		osp_general_block_transfer_t gbt;
		osp_err_t r;
		if (have_first) {
			if (first_block[0] != OSP_TAG_GENERAL_BLOCK_TRANSFER) {
				return OSP_ERR_INVALID;
			}
			osp_buf_t buf;
			osp_buf_init(&buf, (uint8_t *)first_block, first_block_len);
			buf.wr = first_block_len;
			if (osp_gbt_decode(&buf, &gbt) != 0) {
				return OSP_ERR_INVALID;
			}
			have_first = false;
		} else {
			r = gbt_recv_block(transport, framing, &gbt, rx_scratch, rx_scratch_size, timeout_ms);
			if (r != OSP_OK) {
				return r;
			}
		}
		if (gbt.block_number > expected_block) {
			/* Gap: request retransmission of missing block(s). */
			uint16_t gap = (uint16_t)(gbt.block_number - expected_block);
			uint8_t win = gap > OSP_GBT_WINDOW_MASK ? OSP_GBT_WINDOW_MASK : (uint8_t)gap;
			if (win == 0) {
				win = 1;
			}
			uint16_t bna = expected_block > 0 ? (uint16_t)(expected_block - 1) : 0;
			r = gbt_send_ack(transport, framing, win, 1, bna, tx_scratch, tx_scratch_size);
			if (r != OSP_OK) {
				return r;
			}
			continue;
		}
		if (gbt.block_number < expected_block) {
			/* Duplicate or late block — ack and ignore payload. */
			uint8_t win = gbt.window > 0 ? gbt.window : peer_window;
			if (win > 0) {
				r = gbt_send_ack(transport, framing, win, 1, gbt.block_number, tx_scratch, tx_scratch_size);
				if (r != OSP_OK) {
					return r;
				}
			}
			continue;
		}
		if (gbt.window > 0) {
			peer_window = gbt.window;
		}
		if (acc_len + gbt.block_data_len > apdu_size) {
			return OSP_ERR_NOMEM;
		}
		memcpy(&apdu[acc_len], gbt.block_data, gbt.block_data_len);
		acc_len += gbt.block_data_len;

		if (gbt.last_block) {
			*apdu_len = acc_len;
			return OSP_OK;
		}

		if (peer_window > 0) {
			r = gbt_send_ack(transport, framing, peer_window, 1, expected_block, tx_scratch, tx_scratch_size);
			if (r != OSP_OK) {
				return r;
			}
		}
		expected_block++;
	}
}

bool osp_gbt_applies_to_apdu(const uint8_t *apdu, uint32_t len) {
	if (!apdu || len == 0) {
		return false;
	}
	switch (apdu[0]) {
		case OSP_TAG_GET_REQUEST:
		case OSP_TAG_GET_RESPONSE:
		case OSP_TAG_SET_REQUEST:
		case OSP_TAG_SET_RESPONSE:
		case OSP_TAG_ACTION_REQUEST:
		case OSP_TAG_ACTION_RESPONSE:
		case OSP_TAG_DATA_NOTIFICATION:
		case OSP_TAG_EVENT_NOTIFICATION_REQ:
			return true;
		default:
			return false;
	}
}

int osp_gbt_encode(osp_buf_t *buf, const osp_general_block_transfer_t *gbt) {
	if (!buf || !gbt) {
		return -1;
	}
	uint8_t ctrl = (gbt->last_block ? OSP_GBT_LAST_BLOCK : 0) | (gbt->streaming ? OSP_GBT_STREAMING : 0) |
	               (gbt->window & OSP_GBT_WINDOW_MASK);
	osp_axdr_write_u8(buf, OSP_TAG_GENERAL_BLOCK_TRANSFER);
	osp_axdr_write_u8(buf, ctrl);
	osp_axdr_write_u16(buf, gbt->block_number);
	osp_axdr_write_u16(buf, gbt->block_number_ack);
	if (osp_axdr_push_length(buf, gbt->block_data_len) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < gbt->block_data_len; i++) {
		osp_axdr_write_u8(buf, gbt->block_data[i]);
	}
	return 0;
}

int osp_gbt_decode(osp_buf_t *buf, osp_general_block_transfer_t *gbt) {
	if (!buf || !gbt) {
		return -1;
	}
	uint8_t tag, ctrl;
	if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_GENERAL_BLOCK_TRANSFER) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &ctrl) != OSP_OK) {
		return -1;
	}
	gbt->last_block = (ctrl & OSP_GBT_LAST_BLOCK) != 0;
	gbt->streaming = (ctrl & OSP_GBT_STREAMING) != 0;
	gbt->window = ctrl & OSP_GBT_WINDOW_MASK;
	if (osp_axdr_read_u16(buf, &gbt->block_number) != OSP_OK) {
		return -1;
	}
	if (osp_axdr_read_u16(buf, &gbt->block_number_ack) != OSP_OK) {
		return -1;
	}
	uint32_t len;
	if (osp_axdr_read_length(buf, &len) != 0 || len > OSP_MAX_OCTET_LEN) {
		return -1;
	}
	gbt->block_data_len = len;
	for (uint32_t i = 0; i < len; i++) {
		osp_axdr_read_u8(buf, &gbt->block_data[i]);
	}
	return 0;
}
