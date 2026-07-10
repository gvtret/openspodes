#ifndef OSP_GBT_H
#define OSP_GBT_H

#include "../openspodes.h"
#include "../transport/transport.h"
#include "service.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_TAG_GENERAL_BLOCK_TRANSFER 0xE0
#define OSP_GBT_HEADER_MAX             8
#define OSP_GBT_MAX_APDU               4096
#define OSP_GBT_DEFAULT_BLOCK_SIZE     64
#define OSP_GBT_WINDOW_MASK            0x3Fu

typedef struct {
	bool last_block;
	bool streaming;
	uint8_t window;
	uint16_t block_number;
	uint16_t block_number_ack;
	uint8_t block_data[OSP_MAX_OCTET_LEN];
	uint32_t block_data_len;
} osp_general_block_transfer_t;

int osp_gbt_encode(osp_buf_t *buf, const osp_general_block_transfer_t *gbt);
int osp_gbt_decode(osp_buf_t *buf, osp_general_block_transfer_t *gbt);

/* True for xDLMS service APDUs that may use GBT (not ACSE/RLRQ). */
bool osp_gbt_applies_to_apdu(const uint8_t *apdu, uint32_t len);

/* Sequential GBT exchange. window=0: unconfirmed (no ack). window>0: sender waits for ack
 * after each window of blocks; receiver sends ack when peer advertises window>0. */
osp_err_t osp_gbt_transport_send(osp_transport_t *transport, osp_framing_type_t framing, const uint8_t *apdu, uint32_t apdu_len,
                                 uint32_t block_payload_max, uint8_t window, uint8_t *tx_scratch, uint32_t tx_scratch_size,
                                 uint8_t *rx_scratch, uint32_t rx_scratch_size, uint32_t timeout_ms);

/* Receive GBT blocks until last-block; sends ack after each non-final block.
 * Pass first_block/first_block_len when the first GBT APDU was already read. */
osp_err_t osp_gbt_transport_recv(osp_transport_t *transport, osp_framing_type_t framing, uint8_t *rx_scratch, uint32_t rx_scratch_size,
                                 uint8_t *apdu, uint32_t apdu_size, uint32_t *apdu_len, uint8_t *tx_scratch, uint32_t tx_scratch_size,
                                 uint32_t timeout_ms, const uint8_t *first_block, uint32_t first_block_len);

#ifdef __cplusplus
}
#endif

#endif /* OSP_GBT_H */
