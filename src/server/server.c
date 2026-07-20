/**
 * server.c — DLMS/COSEM server request dispatcher
 */

#include "server.h"
#include "../service/initiate.h"
#include "../service/notification.h"
#include "../service/gbt.h"
#include "../security/security.h"
#include "../codec/serialize.h"
#include "../ic/compact_data.h"
#include "../ic/push_setup.h"
#include "../ic/association_ln.h"
#include "../transport/hdlc_session.h"
#include <string.h>

static osp_err_t server_send(osp_server_t *s, const uint8_t *data, uint32_t len);
static osp_err_t server_send_exception(osp_server_t *s, uint8_t state_error, uint8_t service_error);
static osp_err_t handle_get_next(osp_server_t *s, uint8_t invoke_id_priority, uint32_t acked_block);
static osp_err_t send_action_response(osp_server_t *s, const osp_action_response_t *resp);
static osp_err_t handle_action_resp_next(osp_server_t *s, uint8_t invoke_id_priority, uint32_t acked_block);
static osp_err_t send_action_invoke_result(osp_server_t *s, uint8_t invoke_id_priority, osp_dar_t dar, const osp_value_t *result);
static osp_err_t accumulate_action_param_block(osp_server_t *s, uint8_t invoke_id_priority, const osp_data_block_t *block);

void osp_server_set_max_pdu(osp_server_t *s, uint32_t max_pdu) {
	if (s) {
		s->max_pdu = max_pdu > 0 ? max_pdu : OSP_SERVER_MAX_PDU;
	}
}

void osp_server_enable_gbt(osp_server_t *s, uint32_t block_size) {
	if (!s) {
		return;
	}
	s->gbt_enabled = true;
	s->gbt_block_size = block_size > OSP_GBT_HEADER_MAX ? block_size : OSP_GBT_DEFAULT_BLOCK_SIZE;
	s->gbt_window = 0;
}

void osp_server_set_gbt_window(osp_server_t *s, uint8_t window) {
	if (!s) {
		return;
	}
	s->gbt_window = window & OSP_GBT_WINDOW_MASK;
}

void osp_server_set_gbt_streaming(osp_server_t *s, bool enabled) {
	if (s) {
		s->gbt_streaming = enabled;
	}
}

void osp_server_set_ciphering(osp_server_t *s, const osp_sec_context_t *tx, const osp_sec_context_t *rx) {
	if (!s || !tx || !rx) {
		return;
	}
	s->cipher_tx = *tx;
	s->cipher_rx = *rx;
	s->ciphering_enabled = true;
}

void osp_server_clear_ciphering(osp_server_t *s) {
	if (!s) {
		return;
	}
	s->ciphering_enabled = false;
}

void osp_server_set_acse_log_cb(osp_server_t *s, osp_server_acse_log_fn fn, void *ctx) {
	if (!s) {
		return;
	}
	s->acse_log_cb = fn;
	s->acse_log_ctx = ctx;
}

static void server_emit_acse_log(osp_server_t *s, const osp_acse_log_event_t *ev) {
	if (s && s->acse_log_cb && ev) {
		s->acse_log_cb(s->acse_log_ctx, ev);
	}
}

/** IEC 62056-5-3: app context 3/4 = LN/SN with ciphering. */
static bool app_context_requires_ciphering(uint8_t application_context) {
	return application_context == OSP_CTX_LN_CIPHERING ||
	       application_context == OSP_CTX_SN_CIPHERING;
}

/**
 * Enable APDU glo/ded-ciphering only when the AA negotiated a ciphering context.
 * If set_ciphering() already configured keys, keep them; otherwise seed from security.
 */
static void server_apply_ciphering_for_aarq(osp_server_t *s, const osp_aarq_t *aarq, bool aa_accepted) {
	if (!s) {
		return;
	}
	if (!aa_accepted || !aarq || !app_context_requires_ciphering(aarq->application_context)) {
		s->ciphering_enabled = false;
		return;
	}
	if (!s->ciphering_enabled) {
		s->cipher_tx = s->security;
		s->cipher_rx = s->security;
	}
	/* Fresh AA: reset IC replay state; adopt client system title for RX IV. */
	s->cipher_tx.ic_valid = false;
	s->cipher_tx.last_peer_ic = 0;
	s->cipher_rx.ic_valid = false;
	s->cipher_rx.last_peer_ic = 0;
	if (aarq->calling_ap_title_len == OSP_SEC_SYSTEM_TITLE_SIZE) {
		memcpy(s->cipher_rx.peer_system_title, aarq->calling_ap_title, OSP_SEC_SYSTEM_TITLE_SIZE);
		memcpy(s->cipher_tx.peer_system_title, aarq->calling_ap_title, OSP_SEC_SYSTEM_TITLE_SIZE);
	}
	s->ciphering_enabled = true;
}

static uint32_t server_gbt_payload_max(const osp_server_t *s) {
	uint32_t block = s->gbt_block_size > 0 ? s->gbt_block_size : OSP_GBT_DEFAULT_BLOCK_SIZE;
	return block > OSP_GBT_HEADER_MAX ? block - OSP_GBT_HEADER_MAX : 1;
}

osp_err_t osp_server_init(osp_server_t *s, osp_transport_t *transport, osp_framing_type_t framing) {
	if (!s || !transport) {
		return OSP_ERR_INVALID;
	}
	memset(s, 0, sizeof(*s));
	s->transport = transport;
	s->framing = framing;
	s->associated = false;
	s->max_pdu = OSP_SERVER_MAX_PDU;
	s->hdlc_active = false;
	osp_dispatcher_init(&s->dispatcher);

	if (framing == OSP_FRAMING_HDLC) {
		osp_hdlc_session_init_server(&s->hdlc, transport, 1, 1, 1, 1);
	}

	return OSP_OK;
}

osp_err_t osp_server_register(osp_server_t *s, const osp_ic_class_t *cls, void *instance) {
	if (!s || !cls || !instance) {
		return OSP_ERR_INVALID;
	}
	osp_err_t r = osp_dispatcher_register(&s->dispatcher, cls, instance);
	if (r != OSP_OK) {
		return r;
	}
	if (cls->class_id == 62) {
		osp_ic_compact_data_bind_dispatcher((osp_ic_compact_data_t *)instance, &s->dispatcher);
	}
	if (cls->class_id == 40) {
		osp_ic_push_setup_bind_server((osp_ic_push_setup_t *)instance, s);
	}
	return OSP_OK;
}

void osp_server_set_security(osp_server_t *s, const osp_sec_context_t *sec) {
	if (s && sec) {
		s->security = *sec;
	}
}

void osp_server_set_hdlc_addresses(osp_server_t *s, uint32_t server_addr, uint8_t server_addr_len,
                                    uint32_t client_addr, uint8_t client_addr_len) {
	if (!s || s->framing != OSP_FRAMING_HDLC) {
		return;
	}
	osp_hdlc_session_init_server(&s->hdlc, s->transport, server_addr, server_addr_len, client_addr, client_addr_len);
}

void osp_server_set_association(osp_server_t *s, osp_ic_association_ln_t *association) {
	if (s) {
		osp_dispatcher_set_association(&s->dispatcher, association);
	}
}

uint8_t osp_server_get_client_sap(osp_server_t *s) {
	if (!s || !s->hdlc_active) {
		return 0;
	}
	/* received_client_addr stores the actual client address from SNRM source.
	 * For 1-byte HDLC addressing: value is directly the SAP (e.g. 0x10, 0x20, 0x30).
	 * For 2-byte addressing: value = (logical << 7) | physical; we use the full value. */
	return (uint8_t)osp_hdlc_address_value(&s->hdlc.received_client_addr);
}

/* ── Send response ───────────────────────────────────────────────────────── */

static osp_err_t server_send(osp_server_t *s, const uint8_t *data, uint32_t len) {
	const uint8_t *send_data = data;
	uint32_t send_len = len;
	uint8_t cipher_buf[OSP_SERVER_MAX_PDU];

	if (s->ciphering_enabled && len > 0 && osp_gbt_applies_to_apdu(data, len)) {
		uint32_t cipher_len = 0;
		if (osp_glo_protect(&s->cipher_tx, osp_svc_cipher_tag_for_plain(&s->cipher_tx, data[0]), data, len, cipher_buf, &cipher_len) != 0) {
			return OSP_ERR_SECURITY;
		}
		/* IC overflow check — re-keying required */
		if (s->cipher_tx.invocation_counter == 0xFFFFFFFF) {
			return OSP_ERR_SECURITY;
		}
		s->cipher_tx.invocation_counter++;
		send_data = cipher_buf;
		send_len = cipher_len;
	}

	if (s->gbt_enabled && osp_gbt_applies_to_apdu(send_data, send_len) && send_len > server_gbt_payload_max(s)) {
		if (s->gbt_streaming) {
			return osp_gbt_transport_send_streaming(s->transport, s->framing, send_data, send_len, server_gbt_payload_max(s), s->gbt_window,
			                                        s->tx_buf, sizeof(s->tx_buf), s->rx_buf, sizeof(s->rx_buf), 5000);
		}
		return osp_gbt_transport_send(s->transport, s->framing, send_data, send_len, server_gbt_payload_max(s), s->gbt_window, s->tx_buf,
		                              sizeof(s->tx_buf), s->rx_buf, sizeof(s->rx_buf), 5000);
	}
	if (s->hdlc_active) {
		return osp_hdlc_session_send_apdu(&s->hdlc, send_data, send_len);
	}
	return osp_transport_send_apdu(s->transport, s->framing, send_data, send_len);
}

static osp_err_t server_send_exception(osp_server_t *s, uint8_t state_error, uint8_t service_error) {
	osp_buf_t buf;
	osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
	if (osp_exception_response_encode_simple(&buf, state_error, service_error) != 0) {
		return OSP_ERR_INVALID;
	}
	return server_send(s, buf.buf, buf.wr);
}

osp_err_t osp_server_send_event_notification(osp_server_t *s, const osp_event_notification_t *ev) {
	if (!s || !s->associated || !ev) {
		return OSP_ERR_INVALID;
	}
	osp_buf_t buf;
	osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
	if (osp_event_notification_encode(&buf, ev) != 0) {
		return OSP_ERR_INVALID;
	}
	return server_send(s, buf.buf, buf.wr);
}

osp_err_t osp_server_send_data_notification(osp_server_t *s, const osp_data_notification_t *dn) {
	if (!s || !s->associated || !dn) {
		return OSP_ERR_INVALID;
	}
	osp_buf_t buf;
	osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
	if (osp_data_notification_encode(&buf, dn) != 0) {
		return OSP_ERR_INVALID;
	}
	return server_send(s, buf.buf, buf.wr);
}

static osp_err_t server_flush_pending_push(osp_server_t *s) {
	if (!s || !s->pending_push.pending) {
		return OSP_OK;
	}
	s->pending_push.pending = false;
	osp_buf_t buf;
	osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
	if (osp_data_notification_encode(&buf, &s->pending_push.notification) != 0) {
		return OSP_ERR_INVALID;
	}
	return server_send(s, buf.buf, buf.wr);
}

static osp_err_t server_send_action_response(osp_server_t *s, const osp_action_response_t *resp) {
	osp_err_t r = send_action_response(s, resp);
	if (r != OSP_OK) {
		return r;
	}
	return server_flush_pending_push(s);
}

/* ── Handle AARQ ─────────────────────────────────────────────────────────── */

static bool secret_eq(const uint8_t *a, uint8_t alen, const uint8_t *b, uint8_t blen) {
	uint8_t diff = (uint8_t)(alen ^ blen);
	uint8_t maxlen = alen > blen ? alen : blen;
	for (uint8_t i = 0; i < maxlen; i++) {
		uint8_t av = (i < alen) ? a[i] : 0;
		uint8_t bv = (i < blen) ? b[i] : 0;
		diff |= (uint8_t)(av ^ bv);
	}
	return diff == 0;
}

static bool aarq_mechanism_matches_assoc(uint8_t mech, osp_auth_mechanism_t assoc_mech) {
	if (mech == (uint8_t)assoc_mech) {
		return true;
	}
	return mech == OSP_MECH_HLS && assoc_mech == OSP_MECH_HLS_GMAC;
}

/** Pick Association LN by client SAP (HDLC), else by matching mechanism.
 * Associations with client_sap==0 (e.g. current association 0.0.40.0.0.255) are not login targets. */
static osp_ic_association_ln_t *server_pick_association(osp_server_t *s, uint8_t mechanism) {
	uint8_t sap = osp_server_get_client_sap(s);
	osp_ic_association_ln_t *by_mech = NULL;

	for (uint16_t i = 0; i < s->dispatcher.count; i++) {
		const osp_object_entry_t *e = &s->dispatcher.objects[i];
		if (!e->class_def || e->class_def->class_id != 15 || !e->instance) {
			continue;
		}
		osp_ic_association_ln_t *a = (osp_ic_association_ln_t *)e->instance;
		if (a->associated_partners.client_sap == 0) {
			continue; /* current association — not used to establish AA */
		}
		if (sap != 0 && (uint8_t)a->associated_partners.client_sap == sap) {
			return a;
		}
		if (!by_mech && aarq_mechanism_matches_assoc(mechanism, a->authentication_mechanism)) {
			by_mech = a;
		}
	}
	return by_mech ? by_mech : s->dispatcher.association;
}

/** Current association object OBIS 0.0.40.0.0.255 */
static osp_ic_association_ln_t *server_find_current_association(osp_server_t *s) {
	static const osp_obis_t cur_ln = {0, 0, 40, 0, 0, 255};
	for (uint16_t i = 0; i < s->dispatcher.count; i++) {
		const osp_object_entry_t *e = &s->dispatcher.objects[i];
		if (!e->class_def || e->class_def->class_id != 15 || !e->instance) {
			continue;
		}
		osp_ic_association_ln_t *a = (osp_ic_association_ln_t *)e->instance;
		if (osp_obis_eq(&a->logical_name, &cur_ln)) {
			return a;
		}
	}
	return NULL;
}

static void server_mirror_current(osp_server_t *s, const osp_ic_association_ln_t *src) {
	osp_ic_association_ln_t *cur = server_find_current_association(s);
	if (cur && src && cur != src) {
		osp_ic_association_ln_mirror(cur, src);
	}
}

static void server_clear_current(osp_server_t *s) {
	osp_ic_association_ln_t *cur = server_find_current_association(s);
	if (cur) {
		osp_ic_association_ln_set_idle(cur);
	}
}

void osp_server_clear_current_association(osp_server_t *s) {
	if (!s) {
		return;
	}
	server_clear_current(s);
	s->associated = false;
	s->hls_pending = false;
	s->ciphering_enabled = false;
}

static bool aarq_context_supported(uint8_t ctx) {
	return ctx == OSP_CTX_LN || ctx == OSP_CTX_LN_CIPHERING;
}

static bool aarq_mechanism_known(uint8_t mechanism) {
	return mechanism == OSP_MECH_LOWEST || mechanism == OSP_MECH_LLS ||
	       osp_hls_requires_handshake((osp_auth_mechanism_t)mechanism);
}

/* Ciphering and title-bound HLS need an 8-octet client system title in calling-AP-title. */
static bool aarq_requires_calling_ap_title(const osp_aarq_t *aarq) {
	if (app_context_requires_ciphering(aarq->application_context)) {
		return true;
	}
	switch (aarq->mechanism) {
	case OSP_MECH_HLS_GMAC:
	case OSP_MECH_HLS_SHA256:
	case OSP_MECH_HLS_ECDSA:
	case OSP_MECH_HLS_GOST_CMAC:
	case OSP_MECH_HLS_GOST_STREEBOG:
	case OSP_MECH_HLS_GOST_SIG:
		return true;
	default:
		return false;
	}
}

typedef struct {
	uint8_t acse_diag;
	uint8_t acse_diag_is_provider;
	uint8_t initiate_err;
} aarq_reject_info_t;

static aarq_reject_info_t aarq_validate(const osp_aarq_t *aarq, const osp_ic_association_ln_t *assoc,
                                      const osp_initiate_request_t *ireq, bool ireq_ok) {
	aarq_reject_info_t r = {0, 0, 0};

	if (!aarq_context_supported(aarq->application_context)) {
		r.acse_diag = OSP_ACSE_DIAG_APP_CONTEXT_NOT_SUPPORTED;
		return r;
	}
	if (aarq->has_protocol_version && (aarq->protocol_version[0] != 0x02 || aarq->protocol_version[1] != 0x84)) {
		r.acse_diag = OSP_ACSE_PROVIDER_NO_COMMON_ACSE_VERSION;
		r.acse_diag_is_provider = 1;
		return r;
	}
	if (aarq_requires_calling_ap_title(aarq) && aarq->calling_ap_title_len != OSP_SEC_SYSTEM_TITLE_SIZE) {
		r.acse_diag = OSP_ACSE_DIAG_CALLING_AP_TITLE_NOT_RECOGNIZED;
		return r;
	}
	if (ireq_ok) {
		if (ireq->proposed_dlms_version != 0 && ireq->proposed_dlms_version != OSP_DLMS_VERSION_NUMBER) {
			r.initiate_err = OSP_INITIATE_ERR_DLMS_VERSION_TOO_LOW;
			r.acse_diag = OSP_ACSE_DIAG_NO_REASON;
			return r;
		}
		if (ireq->proposed_conformance == 0) {
			r.initiate_err = OSP_INITIATE_ERR_INCOMPATIBLE_CONFORMANCE;
			r.acse_diag = OSP_ACSE_DIAG_NO_REASON;
			return r;
		}
		if (ireq->client_max_receive_pdu_size > 0 && ireq->client_max_receive_pdu_size < OSP_INITIATE_MIN_CLIENT_PDU) {
			r.initiate_err = OSP_INITIATE_ERR_PDU_SIZE_TOO_SHORT;
			r.acse_diag = OSP_ACSE_DIAG_NO_REASON;
			return r;
		}
	}

	if (!assoc || assoc->authentication_mechanism == OSP_MECH_LOWEST) {
		return r;
	}

	if (!aarq->has_mechanism && !aarq->has_calling_auth) {
		r.acse_diag = OSP_ACSE_DIAG_AUTH_REQUIRED;
		return r;
	}
	if (aarq->has_sender_acse_requirement && aarq->sender_acse_requirement != 0x80) {
		r.acse_diag = OSP_ACSE_DIAG_AUTH_FAILURE;
		return r;
	}
	if (!aarq->has_mechanism) {
		r.acse_diag = OSP_ACSE_DIAG_AUTH_FAILURE;
		return r;
	}
	if (!aarq_mechanism_known(aarq->mechanism)) {
		r.acse_diag = OSP_ACSE_DIAG_AUTH_MECH_NOT_RECOGNISED;
		return r;
	}
	if (!aarq->has_calling_auth && aarq->mechanism != OSP_MECH_LOWEST) {
		r.acse_diag = OSP_ACSE_DIAG_AUTH_FAILURE;
		return r;
	}
	if (!aarq_mechanism_matches_assoc(aarq->mechanism, assoc->authentication_mechanism)) {
		r.acse_diag = OSP_ACSE_DIAG_AUTH_FAILURE;
		return r;
	}
	return r;
}

static osp_err_t aare_fill_initiate_response(osp_aare_t *aare, const osp_initiate_request_t *ireq, bool ireq_ok,
                                             uint32_t negotiated_conformance) {
	osp_initiate_response_t iresp;
	osp_initiate_response_default(&iresp);
	iresp.negotiated_conformance = negotiated_conformance;
	if (ireq_ok && ireq->proposed_conformance != 0) {
		iresp.negotiated_conformance &= ireq->proposed_conformance;
	}
	if (ireq_ok && ireq->client_max_receive_pdu_size > 0 &&
	    ireq->client_max_receive_pdu_size < iresp.server_max_receive_pdu_size) {
		iresp.server_max_receive_pdu_size = ireq->client_max_receive_pdu_size;
	}
	osp_buf_t ui;
	osp_buf_init(&ui, aare->user_info, sizeof(aare->user_info));
	if (osp_initiate_response_encode(&iresp, &ui) != OSP_OK) {
		return OSP_ERR_INVALID;
	}
	aare->user_info_len = ui.wr;
	return OSP_OK;
}

static osp_err_t handle_aarq(osp_server_t *s, const osp_aarq_t *aarq) {
	osp_aare_t aare;
	memset(&aare, 0, sizeof(aare));

	osp_initiate_request_t ireq;
	memset(&ireq, 0, sizeof(ireq));
	bool ireq_ok = false;
	if (aarq->user_info_len > 0) {
		osp_buf_t uibuf;
		osp_buf_init(&uibuf, (uint8_t *)aarq->user_info, aarq->user_info_len);
		uibuf.wr = aarq->user_info_len;
		ireq_ok = (osp_initiate_request_decode(&uibuf, &ireq) == OSP_OK);
	}

	osp_ic_association_ln_t *assoc = server_pick_association(s, aarq->mechanism);
	if (assoc) {
		osp_server_set_association(s, assoc);
	}

	aarq_reject_info_t reject = aarq_validate(aarq, assoc, &ireq, ireq_ok);
	bool rejected = (reject.acse_diag != 0 || reject.initiate_err != 0);

	s->associated = false;
	s->hls_pending = false;

	if (!rejected) {
		if (aarq->mechanism == OSP_MECH_LOWEST) {
			if (assoc && assoc->authentication_mechanism != OSP_MECH_LOWEST) {
				rejected = true;
				reject.acse_diag = OSP_ACSE_DIAG_AUTH_FAILURE;
			} else {
				aare.result = OSP_RESULT_ACCEPTED;
				s->associated = true;
				if (assoc) {
					server_mirror_current(s, assoc);
				}
			}
		} else if (aarq->mechanism == OSP_MECH_LLS) {
			if (!assoc || assoc->secret_len == 0 ||
			    !secret_eq(aarq->calling_auth_value, aarq->calling_auth_value_len, assoc->secret, assoc->secret_len)) {
				rejected = true;
				reject.acse_diag = OSP_ACSE_DIAG_AUTH_FAILURE;
			} else {
				aare.result = OSP_RESULT_ACCEPTED;
				s->associated = true;
				server_mirror_current(s, assoc);
			}
		} else if (osp_hls_requires_handshake((osp_auth_mechanism_t)aarq->mechanism)) {
			if (!osp_hal_random_fill) {
				rejected = true;
				reject.acse_diag = OSP_ACSE_DIAG_AUTH_FAILURE;
			} else {
				osp_auth_mechanism_t mech = osp_hls_effective_mechanism(&s->security, aarq->mechanism);
				if (mech == OSP_MECH_HLS && (!assoc || assoc->secret_len == 0)) {
					rejected = true;
					reject.acse_diag = OSP_ACSE_DIAG_AUTH_FAILURE;
				} else {
					s->security.mechanism = mech;
					s->security.hls_secret_len = 0;
					osp_memzero(s->security.hls_secret, sizeof(s->security.hls_secret));
					if (mech == OSP_MECH_HLS && assoc) {
						s->security.hls_secret_len = assoc->secret_len;
						memcpy(s->security.hls_secret, assoc->secret, assoc->secret_len);
					}

					uint8_t ctos_len = aarq->calling_auth_value_len;
					if (ctos_len > sizeof(s->security.ctos)) {
						ctos_len = (uint8_t)sizeof(s->security.ctos);
					}
					if (mech == OSP_MECH_HLS && ctos_len != 16) {
						rejected = true;
						reject.acse_diag = OSP_ACSE_DIAG_AUTH_FAILURE;
					} else {
						memcpy(s->security.ctos, aarq->calling_auth_value, ctos_len);
						s->security.ctos_len = ctos_len;
						if (aarq->calling_ap_title_len == OSP_SEC_SYSTEM_TITLE_SIZE) {
							memcpy(s->security.peer_system_title, aarq->calling_ap_title, OSP_SEC_SYSTEM_TITLE_SIZE);
						}

						uint8_t stoc_len = (mech == OSP_MECH_HLS) ? 16 : 8;
						s->security.stoc_len = stoc_len;
						osp_hal_random_fill(s->security.stoc, stoc_len);
						memcpy(aare.responding_auth_value, s->security.stoc, stoc_len);
						aare.responding_auth_value_len = stoc_len;
						aare.result = OSP_RESULT_ACCEPTED;
						s->hls_pending = true;
					}
				}
			}
		} else {
			rejected = true;
			reject.acse_diag = OSP_ACSE_DIAG_AUTH_MECH_NOT_RECOGNISED;
		}
	}

	if (rejected) {
		aare.result = OSP_RESULT_REJECTED_PERMANENT;
		aare.result_source_diagnostic = reject.acse_diag ? reject.acse_diag : OSP_ACSE_DIAG_NO_REASON;
		aare.result_source_is_provider = reject.acse_diag_is_provider;
	}

	aare.mechanism = aarq->mechanism;
	if (rejected && reject.acse_diag == OSP_ACSE_DIAG_APP_CONTEXT_NOT_SUPPORTED) {
		aare.application_context = OSP_CTX_LN;
	} else {
		aare.application_context = aarq->application_context ? aarq->application_context : OSP_CTX_LN;
	}
	aare.has_protocol_version = 1;
	aare.protocol_version[0] = 0x02;
	aare.protocol_version[1] = 0x84;

	if (aare.result != OSP_RESULT_ACCEPTED && aare.result_source_diagnostic == 0) {
		aare.result_source_diagnostic = OSP_ACSE_DIAG_NO_REASON;
	}

	if (osp_hls_requires_handshake((osp_auth_mechanism_t)aarq->mechanism) && aare.result == OSP_RESULT_ACCEPTED) {
		aare.include_authentication = 1;
		aare.responding_ap_title_len = OSP_SEC_SYSTEM_TITLE_SIZE;
		memcpy(aare.responding_ap_title, s->security.system_title, OSP_SEC_SYSTEM_TITLE_SIZE);
	} else {
		aare.include_authentication = 0;
		aare.responding_ap_title_len = 0;
	}

	server_apply_ciphering_for_aarq(s, aarq, aare.result == OSP_RESULT_ACCEPTED);

	if (reject.initiate_err != 0) {
		osp_buf_t ui;
		osp_buf_init(&ui, aare.user_info, sizeof(aare.user_info));
		if (osp_initiate_error_encode(&ui, reject.initiate_err) != OSP_OK) {
			return OSP_ERR_INVALID;
		}
		aare.user_info_len = ui.wr;
	} else if (aare.result == OSP_RESULT_ACCEPTED) {
		if (ireq_ok && ireq.has_dedicated_key && s->ciphering_enabled) {
			osp_sec_cipher_session_use_dedicated(&s->cipher_tx, &s->cipher_rx, ireq.dedicated_key, ireq.dedicated_key_len);
		}
		uint32_t conf = (aarq->mechanism == OSP_MECH_LOWEST) ? 0x001010u : 0x001015u;
		if (aare_fill_initiate_response(&aare, &ireq, ireq_ok, conf) != OSP_OK) {
			return OSP_ERR_INVALID;
		}
	} else if (ireq_ok && reject.acse_diag == OSP_ACSE_DIAG_APP_CONTEXT_NOT_SUPPORTED) {
		if (aare_fill_initiate_response(&aare, &ireq, ireq_ok, 0x001010u) != OSP_OK) {
			return OSP_ERR_INVALID;
		}
	}

	osp_buf_t buf;
	osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
	if (osp_aare_encode(&aare, &buf) != 0) {
		return OSP_ERR_INVALID;
	}

	{
		osp_acse_user_info_kind_t ui_kind = OSP_ACSE_USER_INFO_NONE;
		if (reject.initiate_err != 0) {
			ui_kind = OSP_ACSE_USER_INFO_INITIATE_ERROR;
		} else if (aare.user_info_len > 0) {
			ui_kind = OSP_ACSE_USER_INFO_INITIATE_RESPONSE;
		}
		osp_acse_log_event_t log_ev = {
		    .kind = OSP_ACSE_LOG_AARE,
		    .client_sap = osp_server_get_client_sap(s),
		    .aarq_application_context = aarq->application_context,
		    .aarq_mechanism = aarq->mechanism,
		    .aarq_has_mechanism = aarq->has_mechanism,
		    .aarq_has_calling_auth = aarq->has_calling_auth,
		    .aarq_ireq_ok = ireq_ok,
		    .aarq_dlms_version = ireq_ok ? ireq.proposed_dlms_version : 0,
		    .aarq_conformance = ireq_ok ? ireq.proposed_conformance : 0,
		    .aarq_client_pdu = ireq_ok ? ireq.client_max_receive_pdu_size : 0,
		    .aare_result = aare.result,
		    .aare_diagnostic = aare.result_source_diagnostic,
		    .aare_diag_is_provider = aare.result_source_is_provider,
		    .aare_initiate_error = reject.initiate_err,
		    .aare_hls_pending = s->hls_pending,
		    .aare_application_context = aare.application_context,
		    .aare_user_info = ui_kind,
		};
		server_emit_acse_log(s, &log_ev);
	}

	return server_send(s, buf.buf, buf.wr);
}

/* ── Handle GET ──────────────────────────────────────────────────────────── */

static osp_err_t dispatch_get_attr(osp_server_t *s, const osp_attribute_descriptor_t *attr, osp_get_result_item_t *out) {
	osp_value_t result;
	osp_err_t r = osp_dispatcher_get(&s->dispatcher, attr->class_id, &attr->instance_id, attr->attribute_id, &result);
	if (r == OSP_OK) {
		out->is_data = 1;
		out->data = result;
		return OSP_OK;
	}
	out->is_data = 0;
	out->access_result = (r == OSP_ERR_SECURITY) ? OSP_DAR_READ_DENIED : OSP_DAR_OBJECT_UNDEFINED;
	return OSP_OK;
}

static osp_err_t send_get_response(osp_server_t *s, const osp_get_response_t *resp) {
	osp_buf_t buf;
	osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
	if (osp_get_response_encode(&buf, resp) != 0) {
		return OSP_ERR_INVALID;
	}
	return server_send(s, buf.buf, buf.wr);
}

static osp_err_t start_get_blocks(osp_server_t *s, uint8_t invoke_id_priority, const uint8_t *data, uint32_t len) {
	s->pending_get.active = true;
	s->pending_get.data_len = len;
	s->pending_get.next_block = 1;
	if (data != s->pending_get.data) {
		memcpy(s->pending_get.data, data, len);
	}
	return handle_get_next(s, invoke_id_priority, 0);
}

static uint32_t server_send_payload_max(const osp_server_t *s) {
	uint32_t max_pdu = s->max_pdu > 0 ? s->max_pdu : OSP_SERVER_MAX_PDU;
	if (s->hdlc_active) {
		uint32_t max_info = s->hdlc.xid.max_info_tx > 0 ? s->hdlc.xid.max_info_tx : OSP_HDLC_MAX_FRAME_SIZE;
		if (max_info > OSP_HDLC_MAX_FRAME_SIZE) {
			max_info = OSP_HDLC_MAX_FRAME_SIZE;
		}
		/* LLC(3) + GET-response-block header (~12) */
		uint32_t hdlc_payload = max_info > 16 ? max_info - 16 : 1;
		if (hdlc_payload < max_pdu) {
			max_pdu = hdlc_payload;
		}
	}
	/* osp_data_block_t.raw_data is OSP_MAX_OCTET_LEN */
	if (max_pdu > OSP_MAX_OCTET_LEN) {
		max_pdu = OSP_MAX_OCTET_LEN;
	}
	return max_pdu;
}

static osp_err_t handle_get_next(osp_server_t *s, uint8_t invoke_id_priority, uint32_t acked_block) {
	if (!s->pending_get.active) {
		osp_get_response_t resp = {0};
		resp.invoke_id_priority = invoke_id_priority;
		resp.type = OSP_GET_RESP_DATA_ERROR;
		resp.data_access_result = OSP_DAR_NO_LONG_GET_IN_PROGRESS;
		return send_get_response(s, &resp);
	}
	uint32_t block_number = s->pending_get.next_block;
	if (acked_block != 0 && acked_block + 1 != block_number) {
		osp_get_response_t resp = {0};
		resp.invoke_id_priority = invoke_id_priority;
		resp.type = OSP_GET_RESP_DATA_ERROR;
		resp.data_access_result = OSP_DAR_LONG_GET_ABORTED;
		return send_get_response(s, &resp);
	}
	uint32_t max_pdu = server_send_payload_max(s);
	uint32_t start = (block_number - 1) * max_pdu;
	uint32_t end = start + max_pdu;
	if (end > s->pending_get.data_len) {
		end = s->pending_get.data_len;
	}
	bool last_block = end >= s->pending_get.data_len;
	osp_get_response_t resp = {0};
	resp.invoke_id_priority = invoke_id_priority;
	resp.type = last_block ? OSP_GET_RESP_BLOCK_LAST : OSP_GET_RESP_BLOCK;
	resp.data_block.block_number = block_number;
	resp.data_block.last_block = last_block;
	resp.data_block.raw_data_len = end - start;
	memcpy(resp.data_block.raw_data, &s->pending_get.data[start], resp.data_block.raw_data_len);
	s->pending_get.next_block++;
	if (last_block) {
		s->pending_get.active = false;
	}
	return send_get_response(s, &resp);
}

static osp_err_t handle_get(osp_server_t *s, const osp_get_request_t *req) {
	if (req->type == OSP_GET_WITH_BLOCK) {
		return handle_get_next(s, req->invoke_id_priority, req->as.next.block_number);
	}
	if (req->type == OSP_GET_WITH_LIST) {
		osp_get_response_t resp = {0};
		resp.invoke_id_priority = req->invoke_id_priority;
		resp.type = OSP_GET_RESP_WITH_LIST;
		resp.with_list.count = req->as.with_list.count;
		for (uint8_t i = 0; i < req->as.with_list.count; i++) {
			dispatch_get_attr(s, &req->as.with_list.items[i].attr, &resp.with_list.items[i]);
		}
		return send_get_response(s, &resp);
	}

	osp_get_response_t resp = {0};
	resp.invoke_id_priority = req->invoke_id_priority;
	osp_get_result_item_t item;
	dispatch_get_attr(s, &req->as.normal.attr, &item);

	/* Apply selective access filter for ProfileGeneric buffer (attr 2) */
	if (item.is_data && req->as.normal.access_selection.type != OSP_SEL_ACCESS_NONE &&
	    req->as.normal.attr.attribute_id == 2 && item.data.tag == OSP_TAG_ARRAY) {
		osp_value_list_t *arr = &item.data.as.array.elements;
		if (req->as.normal.access_selection.type == OSP_SEL_ACCESS_BY_ENTRY ||
		    req->as.normal.access_selection.type == OSP_SEL_ACCESS_BY_RANGE) {
			uint32_t from = req->as.normal.access_selection.param.entry.from;
			uint32_t to = req->as.normal.access_selection.param.entry.to;
			uint8_t filtered = 0;
			for (uint8_t i = 0; i < arr->count; i++) {
				uint32_t entry = i + 1;
				if (entry >= from && (to == 0 || entry <= to)) {
					if (filtered != i) {
						arr->items[filtered] = arr->items[i];
					}
					filtered++;
				}
			}
			arr->count = filtered;
		}
	}

	if (item.is_data) {
		resp.type = OSP_GET_RESP_DATA;
		resp.data = item.data;
		uint8_t enc_mem[OSP_SERVER_MAX_PDU];
		osp_buf_t enc;
		osp_buf_init(&enc, enc_mem, sizeof(enc_mem));
		int enc_rc = osp_get_response_encode(&enc, &resp);
		uint32_t send_limit = server_send_payload_max(s);
		/* Oversized attribute (e.g. Association LN object_list): fall back to
		 * data-block transfer instead of aborting the association. */
		if (enc_rc != 0 || enc.wr > send_limit) {
			if (s->gbt_enabled && enc_rc == 0 && enc.wr > server_gbt_payload_max(s)) {
				return server_send(s, enc.buf, enc.wr);
			}
			/* Serialize into the server's pending buffer (not a large stack temp). */
			osp_buf_t w;
			osp_buf_init(&w, s->pending_get.data, sizeof(s->pending_get.data));
			osp_err_t wr = osp_value_write(&w, &item.data);
			if (wr != OSP_OK) {
				resp.type = OSP_GET_RESP_DATA_ERROR;
				resp.data_access_result = OSP_DAR_TEMPORARY_FAILURE;
				return send_get_response(s, &resp);
			}
			return start_get_blocks(s, req->invoke_id_priority, s->pending_get.data, w.wr);
		}
	} else {
		resp.type = OSP_GET_RESP_DATA_ERROR;
		resp.data_access_result = item.access_result;
	}
	return send_get_response(s, &resp);
}

/* ── Handle SET ──────────────────────────────────────────────────────────── */

static osp_err_t send_set_response(osp_server_t *s, const osp_set_response_t *resp) {
	osp_buf_t buf;
	osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
	if (osp_set_response_encode(&buf, resp) != 0) {
		return OSP_ERR_INVALID;
	}
	return server_send(s, buf.buf, buf.wr);
}

static osp_err_t accumulate_set_block(osp_server_t *s, uint8_t invoke_id_priority, const osp_data_block_t *block) {
	if (!s->pending_set.active) {
		osp_set_response_t resp = {0};
		resp.invoke_id_priority = invoke_id_priority;
		resp.type = OSP_SET_RESP_NORMAL;
		resp.as.normal.result = OSP_DAR_TYPE_MISMATCH;
		return send_set_response(s, &resp);
	}
	if (block->raw_data_len + s->pending_set.set_buffer_len > sizeof(s->pending_set.set_buffer)) {
		s->pending_set.active = false;
		s->pending_set.set_buffer_len = 0;
		osp_set_response_t resp = {0};
		resp.invoke_id_priority = invoke_id_priority;
		resp.type = OSP_SET_RESP_NORMAL;
		resp.as.normal.result = OSP_DAR_LONG_BLOCK_TRANSFER;
		return send_set_response(s, &resp);
	}
	memcpy(&s->pending_set.set_buffer[s->pending_set.set_buffer_len], block->raw_data, block->raw_data_len);
	s->pending_set.set_buffer_len += block->raw_data_len;

	if (!block->last_block) {
		osp_set_response_t resp = {0};
		resp.invoke_id_priority = invoke_id_priority;
		resp.type = OSP_SET_RESP_DATABLOCK;
		resp.as.datablock.block_number = block->block_number;
		return send_set_response(s, &resp);
	}

	osp_attribute_descriptor_t attr = s->pending_set.set_attr;
	uint32_t buflen = s->pending_set.set_buffer_len;
	s->pending_set.active = false;
	s->pending_set.set_buffer_len = 0;

	osp_buf_t vbuf;
	osp_buf_init(&vbuf, s->pending_set.set_buffer, buflen);
	vbuf.wr = buflen;
	osp_value_t value;
	osp_set_response_t resp = {0};
	resp.invoke_id_priority = invoke_id_priority;
	resp.type = OSP_SET_RESP_LAST_DATABLOCK;
	resp.as.last_datablock.block_number = block->block_number;
	if (osp_value_read(&vbuf, &value) != OSP_OK) {
		resp.as.last_datablock.result = OSP_DAR_TYPE_MISMATCH;
	} else {
		osp_err_t wr = osp_dispatcher_set(&s->dispatcher, attr.class_id, &attr.instance_id, attr.attribute_id, &value);
		resp.as.last_datablock.result =
		    (wr == OSP_OK) ? OSP_DAR_SUCCESS : ((wr == OSP_ERR_SECURITY) ? OSP_DAR_SCOPE_OF_ACCESS : OSP_DAR_OBJECT_UNDEFINED);
	}
	return send_set_response(s, &resp);
}

static osp_err_t handle_set(osp_server_t *s, const osp_set_request_t *req) {
	if (req->type == OSP_SET_WITH_FIRST_DATABLOCK) {
		s->pending_set.active = true;
		s->pending_set.set_attr = req->as.first_datablock.attr;
		s->pending_set.set_buffer_len = 0;
		return accumulate_set_block(s, req->invoke_id_priority, &req->as.first_datablock.datablock);
	}
	if (req->type == OSP_SET_WITH_DATABLOCK) {
		return accumulate_set_block(s, req->invoke_id_priority, &req->as.datablock.datablock);
	}

	osp_set_response_t resp;
	memset(&resp, 0, sizeof(resp));
	resp.invoke_id_priority = req->invoke_id_priority;

	if (req->type == OSP_SET_WITH_LIST) {
		resp.type = OSP_SET_RESP_WITH_LIST;
		resp.as.with_list.count = req->as.with_list.count;
		for (uint8_t i = 0; i < req->as.with_list.count; i++) {
			osp_err_t r = osp_dispatcher_set(&s->dispatcher, req->as.with_list.items[i].attr.class_id,
			                                 &req->as.with_list.items[i].attr.instance_id,
			                                 req->as.with_list.items[i].attr.attribute_id,
			                                 &req->as.with_list.items[i].data);
			resp.as.with_list.results[i] =
			    (r == OSP_OK) ? OSP_DAR_SUCCESS : ((r == OSP_ERR_SECURITY) ? OSP_DAR_SCOPE_OF_ACCESS : OSP_DAR_OBJECT_UNDEFINED);
		}
	} else {
		resp.type = OSP_SET_RESP_NORMAL;
		osp_err_t r = osp_dispatcher_set(
		    &s->dispatcher, req->as.normal.items[0].attr.class_id, &req->as.normal.items[0].attr.instance_id,
		    req->as.normal.items[0].attr.attribute_id, &req->as.normal.items[0].data);
		resp.as.normal.result =
		    (r == OSP_OK) ? OSP_DAR_SUCCESS : ((r == OSP_ERR_SECURITY) ? OSP_DAR_SCOPE_OF_ACCESS : OSP_DAR_OBJECT_UNDEFINED);
	}

	return send_set_response(s, &resp);
}

/* ── Handle ACTION ───────────────────────────────────────────────────────── */

static osp_err_t send_action_response(osp_server_t *s, const osp_action_response_t *resp) {
	osp_buf_t buf;
	osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
	if (osp_action_response_encode(&buf, resp) != 0) {
		return OSP_ERR_INVALID;
	}
	return server_send(s, buf.buf, buf.wr);
}

static osp_err_t handle_action_resp_next(osp_server_t *s, uint8_t invoke_id_priority, uint32_t acked_block) {
	if (!s->pending_action_out.active) {
		osp_action_response_t resp = {0};
		resp.invoke_id_priority = invoke_id_priority;
		resp.type = OSP_ACTION_RESP_NORMAL;
		resp.as.normal.item_count = 1;
		resp.as.normal.items[0].result = OSP_DAR_TYPE_MISMATCH;
		return send_action_response(s, &resp);
	}
	uint32_t block_number = s->pending_action_out.next_block;
	if (acked_block != 0 && acked_block + 1 != block_number) {
		s->pending_action_out.active = false;
		osp_action_response_t resp = {0};
		resp.invoke_id_priority = invoke_id_priority;
		resp.type = OSP_ACTION_RESP_NORMAL;
		resp.as.normal.item_count = 1;
		resp.as.normal.items[0].result = OSP_DAR_LONG_BLOCK_TRANSFER;
		return send_action_response(s, &resp);
	}
	uint32_t max_pdu = s->max_pdu > 0 ? s->max_pdu : OSP_SERVER_MAX_PDU;
	uint32_t start = (block_number - 1) * max_pdu;
	uint32_t end = start + max_pdu;
	if (end > s->pending_action_out.data_len) {
		end = s->pending_action_out.data_len;
	}
	bool last_block = end >= s->pending_action_out.data_len;
	osp_action_response_t resp = {0};
	resp.invoke_id_priority = invoke_id_priority;
	resp.type = OSP_ACTION_RESP_WITH_PARAM_BLOCK;
	resp.as.with_param_block.param_block.block_number = block_number;
	resp.as.with_param_block.param_block.last_block = last_block;
	resp.as.with_param_block.param_block.raw_data_len = end - start;
	memcpy(resp.as.with_param_block.param_block.raw_data, &s->pending_action_out.data[start], resp.as.with_param_block.param_block.raw_data_len);
	s->pending_action_out.next_block++;
	if (last_block) {
		s->pending_action_out.active = false;
	}
	return send_action_response(s, &resp);
}

static osp_err_t send_action_invoke_result(osp_server_t *s, uint8_t invoke_id_priority, osp_dar_t dar, const osp_value_t *result) {
	if (dar != OSP_DAR_SUCCESS || !result || result->tag == OSP_TAG_NULL) {
		osp_action_response_t resp = {0};
		resp.invoke_id_priority = invoke_id_priority;
		resp.type = OSP_ACTION_RESP_NORMAL;
		resp.as.normal.item_count = 1;
		resp.as.normal.items[0].result = dar;
		resp.as.normal.items[0].return_data = result ? *result : osp_val_null();
		return send_action_response(s, &resp);
	}

	uint8_t mem[OSP_SERVER_PENDING_MAX];
	osp_buf_t w;
	osp_buf_init(&w, mem, sizeof(mem));
	if (osp_value_write(&w, result) != OSP_OK) {
		return OSP_ERR_INVALID;
	}
	if (w.wr > s->max_pdu) {
		s->pending_action_out.active = true;
		s->pending_action_out.invoke_id_priority = invoke_id_priority;
		s->pending_action_out.result = dar;
		s->pending_action_out.data_len = w.wr;
		s->pending_action_out.next_block = 1;
		memcpy(s->pending_action_out.data, mem, w.wr);
		return handle_action_resp_next(s, invoke_id_priority, 0);
	}

	osp_action_response_t resp = {0};
	resp.invoke_id_priority = invoke_id_priority;
	resp.type = OSP_ACTION_RESP_NORMAL;
	resp.as.normal.item_count = 1;
	resp.as.normal.items[0].result = dar;
	resp.as.normal.items[0].return_data = *result;
	return send_action_response(s, &resp);
}

static osp_err_t accumulate_action_param_block(osp_server_t *s, uint8_t invoke_id_priority, const osp_data_block_t *block) {
	if (!s->pending_action_in.active) {
		osp_action_response_t resp = {0};
		resp.invoke_id_priority = invoke_id_priority;
		resp.type = OSP_ACTION_RESP_NORMAL;
		resp.as.normal.item_count = 1;
		resp.as.normal.items[0].result = OSP_DAR_TYPE_MISMATCH;
		return send_action_response(s, &resp);
	}
	if (block->raw_data_len + s->pending_action_in.buffer_len > sizeof(s->pending_action_in.buffer)) {
		s->pending_action_in.active = false;
		s->pending_action_in.buffer_len = 0;
		osp_action_response_t resp = {0};
		resp.invoke_id_priority = invoke_id_priority;
		resp.type = OSP_ACTION_RESP_NORMAL;
		resp.as.normal.item_count = 1;
		resp.as.normal.items[0].result = OSP_DAR_LONG_BLOCK_TRANSFER;
		return send_action_response(s, &resp);
	}
	memcpy(&s->pending_action_in.buffer[s->pending_action_in.buffer_len], block->raw_data, block->raw_data_len);
	s->pending_action_in.buffer_len += block->raw_data_len;

	if (!block->last_block) {
		osp_action_response_t resp = {0};
		resp.invoke_id_priority = invoke_id_priority;
		resp.type = OSP_ACTION_RESP_NEXT_PARAM_BLOCK;
		resp.as.next_param_block.block_number = block->block_number;
		return send_action_response(s, &resp);
	}

	osp_method_descriptor_t method = s->pending_action_in.method;
	uint32_t buflen = s->pending_action_in.buffer_len;
	s->pending_action_in.active = false;
	s->pending_action_in.buffer_len = 0;

	osp_buf_t vbuf;
	osp_buf_init(&vbuf, s->pending_action_in.buffer, buflen);
	vbuf.wr = buflen;
	osp_value_t param;
	if (osp_value_read(&vbuf, &param) != OSP_OK) {
		return send_action_invoke_result(s, invoke_id_priority, OSP_DAR_TYPE_MISMATCH, NULL);
	}

	osp_value_t result = osp_val_null();
	osp_err_t r = osp_dispatcher_action(&s->dispatcher, method.class_id, &method.instance_id, method.method_id, &param,
	                                    &result);
	osp_dar_t dar =
	    (r == OSP_OK) ? OSP_DAR_SUCCESS : ((r == OSP_ERR_SECURITY) ? OSP_DAR_SCOPE_OF_ACCESS : OSP_DAR_OBJECT_UNDEFINED);
	return send_action_invoke_result(s, invoke_id_priority, dar, &result);
}

static osp_err_t handle_action(osp_server_t *s, const osp_action_request_t *req) {
	if (req->type == OSP_ACTION_NEXT_PARAM_BLOCK) {
		return handle_action_resp_next(s, req->invoke_id_priority, req->as.next_param_block.block_number);
	}
	if (req->type == OSP_ACTION_WITH_FIRST_PARAM_BLOCK) {
		s->pending_action_in.active = true;
		s->pending_action_in.method = req->as.first_param_block.method;
		s->pending_action_in.buffer_len = 0;
		return accumulate_action_param_block(s, req->invoke_id_priority, &req->as.first_param_block.param_block);
	}
	if (req->type == OSP_ACTION_WITH_PARAM_BLOCK) {
		return accumulate_action_param_block(s, req->invoke_id_priority, &req->as.with_param_block.param_block);
	}

	osp_action_response_t resp;
	memset(&resp, 0, sizeof(resp));
	resp.invoke_id_priority = req->invoke_id_priority;

	if (req->type == OSP_ACTION_WITH_LIST) {
		resp.type = OSP_ACTION_RESP_WITH_LIST;
		resp.as.with_list.count = req->as.with_list.count;
		for (uint8_t i = 0; i < req->as.with_list.count; i++) {
			osp_value_t result;
			osp_err_t r = osp_dispatcher_action(&s->dispatcher, req->as.with_list.items[i].method.class_id,
			                                    &req->as.with_list.items[i].method.instance_id,
			                                    req->as.with_list.items[i].method.method_id,
			                                    &req->as.with_list.items[i].data, &result);
			resp.as.with_list.items[i].result =
			    (r == OSP_OK) ? OSP_DAR_SUCCESS : ((r == OSP_ERR_SECURITY) ? OSP_DAR_SCOPE_OF_ACCESS : OSP_DAR_OBJECT_UNDEFINED);
			resp.as.with_list.items[i].return_data = result;
		}
	} else {
		resp.type = OSP_ACTION_RESP_NORMAL;
		osp_value_t result;
		osp_err_t r = osp_dispatcher_action(
		    &s->dispatcher, req->as.normal.items[0].method.class_id, &req->as.normal.items[0].method.instance_id,
		    req->as.normal.items[0].method.method_id, &req->as.normal.items[0].data, &result);
		resp.as.normal.items[0].result =
		    (r == OSP_OK) ? OSP_DAR_SUCCESS : ((r == OSP_ERR_SECURITY) ? OSP_DAR_SCOPE_OF_ACCESS : OSP_DAR_OBJECT_UNDEFINED);
		resp.as.normal.items[0].return_data = result;
		resp.as.normal.item_count = 1;
	}

	return server_send_action_response(s, &resp);
}

/* ── Handle HLS pass 3 (from client) ────────────────────────────────────── */

static osp_err_t handle_hls_pass3(osp_server_t *s, const osp_action_request_t *req) {
	if (req->as.normal.items[0].data.tag != OSP_TAG_OCTETSTRING || req->as.normal.items[0].data.as.octetstring.len == 0) {
		return OSP_ERR_INVALID;
	}

	const uint8_t *f_stoc = req->as.normal.items[0].data.as.octetstring.data;
	uint32_t f_len = req->as.normal.items[0].data.as.octetstring.len;

	/* Verify client's f(StoC) */
	if (osp_hls_pass3_verify(&s->security, f_stoc, f_len) != 0) {
		/* Auth failed — send error response; AA not established */
		osp_action_response_t resp;
		memset(&resp, 0, sizeof(resp));
		resp.invoke_id_priority = req->invoke_id_priority;
		resp.type = OSP_ACTION_RESP_NORMAL;
		resp.as.normal.items[0].result = OSP_DAR_AUTHORIZATION_FAILURE;
		resp.as.normal.item_count = 1;

		osp_buf_t buf;
		osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
		osp_action_response_encode(&buf, &resp);
		osp_acse_log_event_t log_ev = {
		    .kind = OSP_ACSE_LOG_HLS_PASS3,
		    .client_sap = osp_server_get_client_sap(s),
		    .hls_pass3_ok = false,
		};
		server_emit_acse_log(s, &log_ev);
		return server_send(s, buf.buf, buf.wr);
	}

	/* Pass 3 OK — establish AA and mirror current association */
	s->associated = true;
	s->hls_pending = false;
	if (s->dispatcher.association) {
		server_mirror_current(s, s->dispatcher.association);
	}

	/* Build pass 4: f(CtoS) */
	uint8_t f_ctos[OSP_SEC_HLS_AUTH_MAX];
	uint32_t f_ctos_len = 0;
	if (osp_hls_pass4_build(&s->security, f_ctos, sizeof(f_ctos), &f_ctos_len) != 0) {
		return OSP_ERR_INVALID;
	}

	/* Send ACTION response with f(CtoS) */
	osp_action_response_t resp;
	memset(&resp, 0, sizeof(resp));
	resp.invoke_id_priority = req->invoke_id_priority;
	resp.type = OSP_ACTION_RESP_NORMAL;
	resp.as.normal.items[0].result = OSP_DAR_SUCCESS;
	resp.as.normal.items[0].return_data.tag = OSP_TAG_OCTETSTRING;
	memcpy(resp.as.normal.items[0].return_data.as.octetstring.data, f_ctos, f_ctos_len);
	resp.as.normal.items[0].return_data.as.octetstring.len = f_ctos_len;
	resp.as.normal.item_count = 1;

	osp_buf_t buf;
	osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
	osp_action_response_encode(&buf, &resp);
	osp_acse_log_event_t log_ev = {
	    .kind = OSP_ACSE_LOG_HLS_PASS3,
	    .client_sap = osp_server_get_client_sap(s),
	    .hls_pass3_ok = true,
	};
	server_emit_acse_log(s, &log_ev);
	return server_send(s, buf.buf, buf.wr);
}

/* ── Accept one request ──────────────────────────────────────────────────── */

osp_err_t osp_server_accept(osp_server_t *s, uint32_t timeout_ms) {
	if (!s || !s->transport) {
		return OSP_ERR_INVALID;
	}

	/* Receive raw APDU */
	uint32_t rx_len = 0;
	osp_err_t r = OSP_OK;

	/* HDLC: NDM (IDLE) — SNRM/DISC; NRM (CONNECTED) — I-frame/APDU */
	if (s->framing == OSP_FRAMING_HDLC) {
		uint32_t frame_wait_ms = osp_hdlc_inactivity_wait_ms(s->hdlc.inactivity_timeout_s);
		if (osp_hdlc_session_state(&s->hdlc) != OSP_HDLC_STATE_CONNECTED) {
			r = osp_hdlc_session_connect(&s->hdlc, frame_wait_ms);
			if (r != OSP_OK) {
				return r;
			}
			s->hdlc_active = true;
			return OSP_OK; /* SNRM/UA: now in NRM, caller calls again for APDU */
		}
		r = osp_hdlc_session_recv_apdu(&s->hdlc, s->rx_buf, sizeof(s->rx_buf), &rx_len, frame_wait_ms);
	} else {
		r = osp_transport_recv_apdu(s->transport, s->framing, s->rx_buf, sizeof(s->rx_buf), &rx_len, timeout_ms);
	}
	if (r != OSP_OK) {
		if (r == OSP_ERR_DISCONNECTED) {
			/* HDLC DISC → NDM: clear AA, keep TCP, wait for next SNRM. */
			osp_server_clear_current_association(s);
			s->associated = false;
			s->hls_pending = false;
			s->ciphering_enabled = false;
			if (s->framing == OSP_FRAMING_HDLC) {
				s->hdlc_active = false;
				return OSP_ERR_TIMEOUT;
			}
			return r;
		}
		if (r == OSP_ERR_IO) {
			osp_server_clear_current_association(s);
		}
		return r;
	}

	if (rx_len == 0) {
		return OSP_ERR_INVALID;
	}

	uint8_t tag = s->rx_buf[0];

	/* ACSE messages (BER-encoded) */
	if (tag == 0x60 || tag == 0x61) {
		osp_aarq_t aarq;
		osp_buf_t buf;
		osp_buf_init(&buf, s->rx_buf, rx_len);
		buf.wr = rx_len; /* data already received */

		if (osp_aarq_decode(&buf, &aarq) != 0) {
			return OSP_ERR_INVALID;
		}
		return handle_aarq(s, &aarq);
	}

	if (tag == OSP_ACSE_RLRQ_TAG) {
		/* Client release request — decode RLRQ, send RLRE */
		osp_rlrq_t rlrq;
		osp_buf_t buf;
		osp_buf_init(&buf, s->rx_buf, rx_len);
		buf.wr = rx_len;

		if (osp_rlrq_decode(&buf, &rlrq) != 0) {
			return OSP_ERR_INVALID;
		}

		osp_rlrq_t rlre;
		rlre.reason = 0;
		osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
		if (osp_rlre_encode(&rlre, &buf) != 0) {
			return OSP_ERR_INVALID;
		}
		s->associated = false;
		s->hls_pending = false;
		s->ciphering_enabled = false;
		server_clear_current(s);
		return server_send(s, buf.buf, buf.wr);
	}

	/* Decipher before association gate: HLS pass 3 may arrive glo-/ded-ciphered. */
	if (s->ciphering_enabled && osp_svc_is_ciphered_tag(tag)) {
		static uint8_t plain[OSP_GBT_MAX_APDU];
		uint32_t plain_len = 0;
		int ur = osp_glo_unprotect(&s->cipher_rx, s->rx_buf, rx_len, plain, &plain_len);
		if (ur == -2) {
			return server_send_exception(s, OSP_EXC_STATE_SERVICE_NOT_ALLOWED, OSP_EXC_SVC_IC_ERROR);
		}
		if (ur != 0) {
			return server_send_exception(s, OSP_EXC_STATE_SERVICE_NOT_ALLOWED, OSP_EXC_SVC_DECIPHERING_ERROR);
		}
		if (plain_len > sizeof(s->rx_buf)) {
			return OSP_ERR_NOMEM;
		}
		memcpy(s->rx_buf, plain, plain_len);
		rx_len = plain_len;
		tag = s->rx_buf[0];
		/* Keep outbound cipher IC/policy aligned with the peer frame we just accepted. */
		if (s->cipher_rx.ic_valid) {
			if (s->cipher_tx.invocation_counter <= s->cipher_rx.last_peer_ic) {
				s->cipher_tx.invocation_counter = s->cipher_rx.last_peer_ic + 1;
			}
			s->cipher_tx.policy = s->cipher_rx.policy;
		}
	}

	/* HLS pass 3 is allowed before associated (after AARE with StoC) */
	if (!s->associated) {
		if (s->hls_pending && tag == OSP_TAG_ACTION_REQUEST) {
			osp_buf_t abuf;
			osp_buf_init(&abuf, s->rx_buf, rx_len);
			abuf.wr = rx_len;
			osp_action_request_t areq;
			if (osp_action_request_decode(&abuf, &areq) == 0 && areq.type == OSP_ACTION_NORMAL &&
			    areq.as.normal.items[0].method.class_id == 15 && areq.as.normal.items[0].method.method_id == 1) {
				return handle_hls_pass3(s, &areq);
			}
		}
		return server_send_exception(s, OSP_EXC_STATE_SERVICE_NOT_ALLOWED, OSP_EXC_SVC_OPERATION_NOT_POSSIBLE);
	}

	/* General block transfer wrapper */
	if (tag == OSP_TAG_GENERAL_BLOCK_TRANSFER) {
		if (!s->gbt_enabled) {
			return OSP_ERR_UNSUPPORTED;
		}
		static uint8_t apdu[OSP_GBT_MAX_APDU];
		uint32_t apdu_len = 0;
		osp_err_t gr = osp_gbt_transport_recv(s->transport, s->framing, s->rx_buf, sizeof(s->rx_buf), apdu, sizeof(apdu), &apdu_len,
		                                        s->tx_buf, sizeof(s->tx_buf), timeout_ms, s->rx_buf, rx_len);
		if (gr != OSP_OK || apdu_len == 0) {
			return gr != OSP_OK ? gr : OSP_ERR_INVALID;
		}
		if (apdu_len > sizeof(s->rx_buf)) {
			return OSP_ERR_NOMEM;
		}
		memcpy(s->rx_buf, apdu, apdu_len);
		rx_len = apdu_len;
		tag = s->rx_buf[0];
	}

	if (s->ciphering_enabled && osp_svc_is_ciphered_tag(tag)) {
		static uint8_t plain_gbt[OSP_GBT_MAX_APDU];
		uint32_t plain_len = 0;
		int ur = osp_glo_unprotect(&s->cipher_rx, s->rx_buf, rx_len, plain_gbt, &plain_len);
		if (ur == -2) {
			return server_send_exception(s, OSP_EXC_STATE_SERVICE_NOT_ALLOWED, OSP_EXC_SVC_IC_ERROR);
		}
		if (ur != 0) {
			return server_send_exception(s, OSP_EXC_STATE_SERVICE_NOT_ALLOWED, OSP_EXC_SVC_DECIPHERING_ERROR);
		}
		if (plain_len > sizeof(s->rx_buf)) {
			return OSP_ERR_NOMEM;
		}
		memcpy(s->rx_buf, plain_gbt, plain_len);
		rx_len = plain_len;
		tag = s->rx_buf[0];
	}

	/* xDLMS service messages (A-XDR) */
	osp_buf_t rbuf;
	osp_buf_init(&rbuf, s->rx_buf, rx_len);
	rbuf.wr = rx_len; /* data already received */

	switch (tag) {
		case OSP_TAG_GET_REQUEST: {
			osp_get_request_t req;
			if (osp_get_request_decode(&rbuf, &req) != 0) {
				/* Malformed GET must not kill the session.
				 * Conformance clients expect a Get-Response with DATA-ERROR, not a raw ExceptionResponse. */
				if (s->associated) {
					osp_get_response_t resp = {0};
					/* APDU layout: [C0 tag][type][invoke-id-priority]... */
					resp.type = OSP_GET_RESP_DATA_ERROR;
					resp.invoke_id_priority = (rx_len >= 3) ? s->rx_buf[2] : 0;
					/* Etalon returns data-access-result == 1 for malformed GET in tests #20/#21. */
					resp.data_access_result = OSP_DAR_HARDWARE_FAULT;

					osp_buf_t buf;
					osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
					if (osp_get_response_encode(&buf, &resp) != 0) {
						return OSP_ERR_INVALID;
					}
					return server_send(s, buf.buf, buf.wr);
				}
				return OSP_ERR_INVALID;
			}
			return handle_get(s, &req);
		}

		case OSP_TAG_SET_REQUEST: {
			osp_set_request_t req;
			if (osp_set_request_decode(&rbuf, &req) != 0) {
				/* Malformed SET must not kill the session.
				 * Conformance clients expect a Set-Response with normal DAR result. */
				if (s->associated) {
					osp_set_response_t resp = {0};
					/* APDU layout: [C1 tag][type][invoke-id-priority]... */
					resp.type = OSP_SET_RESP_NORMAL;
					resp.invoke_id_priority = (rx_len >= 3) ? s->rx_buf[2] : 0;
					/* Etalon returns result == 1 for malformed SET in tests #20/#21. */
					resp.as.normal.result = OSP_DAR_HARDWARE_FAULT;

					osp_buf_t buf;
					osp_buf_init(&buf, s->tx_buf, sizeof(s->tx_buf));
					if (osp_set_response_encode(&buf, &resp) != 0) {
						return OSP_ERR_INVALID;
					}
					return server_send(s, buf.buf, buf.wr);
				}
				return OSP_ERR_INVALID;
			}
			return handle_set(s, &req);
		}

		case OSP_TAG_ACTION_REQUEST: {
			osp_action_request_t req;
			if (osp_action_request_decode(&rbuf, &req) != 0) {
				return OSP_ERR_INVALID;
			}

			/* Check if this is HLS pass 3 (ACTION on class 15, method 1) */
			if (req.type == OSP_ACTION_NORMAL && req.as.normal.items[0].method.class_id == 15 &&
			    req.as.normal.items[0].method.method_id == 1) {
				return handle_hls_pass3(s, &req);
			}
			return handle_action(s, &req);
		}

		default:
			/* Unknown APDU tag (e.g. SN-referencing ReadRequest/WriteRequest):
			 * send ExceptionResponse per IEC 62056-6-2 §9.1.2 */
			if (s->associated) {
				return server_send_exception(s, OSP_EXC_STATE_SERVICE_NOT_ALLOWED, OSP_EXC_SVC_OPERATION_NOT_POSSIBLE);
			}
			return OSP_ERR_UNSUPPORTED;
	}
}

/* ── Run server loop ─────────────────────────────────────────────────────── */

osp_err_t osp_server_run(osp_server_t *s, uint32_t timeout_ms) {
	osp_err_t r;
	while (true) {
		r = osp_server_accept(s, timeout_ms);
		if (r == OSP_ERR_TIMEOUT) {
			continue; /* no data, try again */
		}
		if (r == OSP_ERR_INVALID && !s->associated) {
			continue; /* not associated yet */
		}
		if (r != OSP_OK) {
			break; /* transport error or release */
		}
	}
	return r;
}
