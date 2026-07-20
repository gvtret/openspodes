/**
 * hdlc_session.h — HDLC link-layer session (IEC 62056-46)
 *
 * Manages the HDLC connection lifecycle:
 *   - SNRM/UA negotiation with XID parameter exchange
 *   - I-frame send/receive with N(S)/N(R) sequence tracking
 *   - LLC header add/strip (E6 E6 00 / E6 E7 00)
 *   - DISC/DM teardown
 *
 * Built on top of the HAL transport + frame codec.
 */

#ifndef OSP_HDLC_SESSION_H
#define OSP_HDLC_SESSION_H

#include "transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* IEC 62056-46 / Blue Book HDLC timeout limits */
#define OSP_HDLC_INTER_OCTET_MIN_MS 20
#define OSP_HDLC_INTER_OCTET_MAX_MS 6000
#define OSP_HDLC_INTER_OCTET_DEFAULT_MS 25
#define OSP_HDLC_INACTIVITY_MAX_S 120
/** Pass as frame wait timeout to block until a complete frame (inactivity disabled). */
#define OSP_HDLC_WAIT_FOREVER_MS 0

/* HDLC session state */
typedef enum {
	OSP_HDLC_STATE_IDLE,      /* Not connected */
	OSP_HDLC_STATE_CONNECTING, /* SNRM sent, waiting UA */
	OSP_HDLC_STATE_CONNECTED,  /* Link is up, I-frames allowed */
	OSP_HDLC_STATE_DISCONNECTING, /* DISC sent, waiting UA */
} osp_hdlc_state_t;

/** Optional callback invoked when HDLC session state changes. */
typedef void (*osp_hdlc_state_fn)(void *ctx, osp_hdlc_state_t old_state, osp_hdlc_state_t new_state);

/** HDLC timeout kinds reported via on_timeout. */
typedef enum {
	OSP_HDLC_TIMEOUT_INTER_OCTET, /* межсимвольный, param = ms */
	OSP_HDLC_TIMEOUT_INTER_FRAME, /* межкадровый, param = s */
} osp_hdlc_timeout_kind_t;

typedef void (*osp_hdlc_timeout_fn)(void *ctx, osp_hdlc_timeout_kind_t kind, uint32_t param);

/* XID parameters negotiated during SNRM/UA exchange */
typedef struct {
	uint16_t max_info_tx;   /* Max info field length, transmit */
	uint16_t max_info_rx;   /* Max info field length, receive */
	uint8_t window_tx;      /* Window size, transmit (1-7) */
	uint8_t window_rx;      /* Window size, receive (1-7) */
} osp_hdlc_xid_params_t;

/* HDLC session context */
typedef struct {
	osp_transport_t *transport;

	/* Addresses */
	osp_hdlc_address_t client_addr;
	osp_hdlc_address_t server_addr;
	osp_hdlc_address_t received_client_addr; /* Actual client addr from SNRM (server side) */
	bool is_client; /* true = client side, false = server side */

	/* State */
	osp_hdlc_state_t state;
	osp_hdlc_state_fn on_state_change;
	void *state_change_ctx;
	osp_hdlc_timeout_fn on_timeout;
	void *timeout_ctx;

	/* Sequence numbers (mod 8) */
	uint8_t send_seq; /* N(S) — next send sequence */
	uint8_t recv_seq; /* N(R) — next expected receive sequence */

	/* XID: configured ceiling + values negotiated on last SNRM */
	osp_hdlc_xid_params_t xid_configured;
	osp_hdlc_xid_params_t xid;

	/* Timeouts (IEC HDLC setup / Blue Book) */
	uint16_t inter_octet_timeout_ms; /* межсимвольный: 20–6000 ms (default 25) */
	uint16_t inactivity_timeout_s;   /* межкадровый: 0–120 s, 0 = disabled */

	/* Retransmission buffer (last sent I-frame info field) */
	uint8_t last_sent_info[OSP_HDLC_MAX_FRAME_SIZE];
	uint16_t last_sent_info_len;
	uint8_t last_sent_seq; /* N(S) of the last sent frame */
	bool has_pending_retransmit; /* true if retransmission buffer is valid */

	/* Retransmission limits */
	uint8_t max_retransmits; /* 0 = no retransmission, default 3 */

	/* HDLC MSDU reassembly (segmentation bit in format field) */
	uint8_t reassembly[OSP_HDLC_MAX_FRAME_SIZE];
	uint16_t reassembly_len;

	/* Buffers */
	uint8_t tx_buf[OSP_HDLC_MAX_FRAME_SIZE + 64];
	uint8_t rx_buf[OSP_HDLC_MAX_FRAME_SIZE + 64];
	uint32_t rx_pending_len; /* Unprocessed bytes retained in rx_buf */
} osp_hdlc_session_t;

/** @brief Initialize an HDLC session as client (SNRM initiator). */
void osp_hdlc_session_init_client(osp_hdlc_session_t *s, osp_transport_t *t,
                                   uint32_t client_addr, uint8_t client_addr_len,
                                   uint32_t server_addr, uint8_t server_addr_len);
/** @brief Initialize an HDLC session as server (SNRM responder). */
void osp_hdlc_session_init_server(osp_hdlc_session_t *s, osp_transport_t *t,
                                   uint32_t server_addr, uint8_t server_addr_len,
                                   uint32_t client_addr, uint8_t client_addr_len);

/** @brief Connect: send SNRM, wait for UA. Returns OSP_OK on success. */
osp_err_t osp_hdlc_session_connect(osp_hdlc_session_t *s, uint32_t timeout_ms);

/** @brief Disconnect: send DISC, wait for UA/DM. */
osp_err_t osp_hdlc_session_disconnect(osp_hdlc_session_t *s, uint32_t timeout_ms);

/** @brief Send an APDU as an I-frame (with LLC header and N(S)/N(R)). */
osp_err_t osp_hdlc_session_send_apdu(osp_hdlc_session_t *s, const uint8_t *apdu, uint32_t apdu_len);

/** @brief Receive an APDU from an I-frame (strips LLC header, updates N(R)). */
osp_err_t osp_hdlc_session_recv_apdu(osp_hdlc_session_t *s, uint8_t *buf, uint32_t buf_size,
                                      uint32_t *apdu_len, uint32_t timeout_ms);

/** @brief Get current HDLC session state. */
osp_hdlc_state_t osp_hdlc_session_state(const osp_hdlc_session_t *s);

/** @brief Human-readable HDLC mode name (NDM/NRM/…). */
const char *osp_hdlc_state_name(osp_hdlc_state_t state);

/** @brief Register a callback for HDLC state transitions. */
void osp_hdlc_session_set_state_callback(osp_hdlc_session_t *s, osp_hdlc_state_fn cb, void *ctx);

/** @brief Register a callback for HDLC timeout events. */
void osp_hdlc_session_set_timeout_callback(osp_hdlc_session_t *s, osp_hdlc_timeout_fn cb, void *ctx);

/** @brief Drop to NDM without closing transport (inactivity / local reset). */
void osp_hdlc_session_enter_ndm(osp_hdlc_session_t *s);

/** @brief Clamp inter-octet timeout to 20–6000 ms (0 → default 25). */
uint16_t osp_hdlc_normalize_inter_octet_ms(uint16_t ms);

/** @brief Clamp inter-frame inactivity to 0–120 s (0 = disabled). */
uint16_t osp_hdlc_normalize_inactivity_s(uint16_t seconds);

/** @brief Inter-frame wait in ms from inactivity_s (0 → OSP_HDLC_WAIT_FOREVER_MS). */
uint32_t osp_hdlc_inactivity_wait_ms(uint16_t inactivity_s);

#ifdef __cplusplus
}
#endif

#endif /* OSP_HDLC_SESSION_H */
