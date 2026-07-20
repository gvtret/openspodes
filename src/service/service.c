/**
 * service.c — xDLMS service layer implementation
 *
 * ACSE, GET, SET, ACTION encode/decode.
 * BER for ACSE, A-XDR for xDLMS services.
 */

#include "service.h"
#include "initiate.h"
#include "../security/security.h"
#include "../codec/codec.h"
#include "../codec/serialize.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  BER helpers for ACSE
 * ═══════════════════════════════════════════════════════════════════════════ */

static void ber_write_oid(osp_buf_t *buf, const uint8_t *prefix, uint8_t prefix_len, uint8_t last_arc) {
	osp_ber_write_tag(buf, 0, false, 6);
	osp_ber_write_length(buf, prefix_len + 1);
	for (uint8_t i = 0; i < prefix_len; i++) {
		osp_axdr_write_u8(buf, prefix[i]);
	}
	osp_axdr_write_u8(buf, last_arc);
}

static int ber_read_oid(osp_buf_t *buf, uint8_t *last_arc) {
	osp_ber_tag_t tag;
	if (osp_ber_read_tag(buf, &tag) != OSP_OK || tag.tag_number != 6) {
		return -1;
	}
	uint32_t len;
	if (osp_ber_read_length(buf, &len) != OSP_OK || len < 7) {
		return -1;
	}
	/* Skip 6 prefix bytes, read last arc */
	buf->rd += 6;
	*last_arc = buf->buf[buf->rd++];
	return 0;
}

static const uint8_t OID_PREFIX_APP[] = {0x60, 0x85, 0x74, 0x05, 0x08, 0x01};
static const uint8_t OID_PREFIX_MECH[] = {0x60, 0x85, 0x74, 0x05, 0x08, 0x02};

/* BER length backpatch: reserve 1 byte, then fix up after content is written.
 * Uses short form for < 128, shifts content and uses long form otherwise. */
static int ber_backpatch_length(osp_buf_t *buf, uint32_t len_pos) {
	uint32_t content_len = buf->wr - len_pos - 1;
	if (content_len < 0x80) {
		buf->buf[len_pos] = (uint8_t)content_len;
	} else {
		/* Shift content right by 1 byte to make room for 0x81 prefix */
		if (buf->wr + 1 > buf->size)
			return -1;
		memmove(&buf->buf[len_pos + 2], &buf->buf[len_pos + 1], content_len);
		buf->buf[len_pos] = 0x81;
		buf->buf[len_pos + 1] = (uint8_t)content_len;
		buf->wr++;
	}
	return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  ACSE name helpers (logging)
 * ═══════════════════════════════════════════════════════════════════════════ */

const char *osp_acse_result_name(uint8_t result) {
	switch (result) {
	case OSP_RESULT_ACCEPTED:
		return "accepted";
	case OSP_RESULT_REJECTED_PERMANENT:
		return "rejected-permanent";
	case OSP_RESULT_REJECTED_TRANSIENT:
		return "rejected-transient";
	default:
		return "unknown";
	}
}

const char *osp_acse_diag_name(uint8_t diag, int is_provider) {
	if (is_provider) {
		switch (diag) {
		case OSP_ACSE_DIAG_NULL:
			return "null";
		case OSP_ACSE_PROVIDER_NO_COMMON_ACSE_VERSION:
			return "no-common-acse-version";
		default:
			return "provider-unknown";
		}
	}
	switch (diag) {
	case OSP_ACSE_DIAG_NULL:
		return "null";
	case OSP_ACSE_DIAG_NO_REASON:
		return "no-reason";
	case OSP_ACSE_DIAG_APP_CONTEXT_NOT_SUPPORTED:
		return "application-context-not-supported";
	case OSP_ACSE_DIAG_CALLING_AP_TITLE_NOT_RECOGNIZED:
		return "calling-AP-title-not-recognized";
	case OSP_ACSE_DIAG_AUTH_MECH_NOT_RECOGNISED:
		return "authentication-mechanism-not-recognised";
	case OSP_ACSE_DIAG_AUTH_MECH_REQUIRED:
		return "authentication-mechanism-required";
	case OSP_ACSE_DIAG_AUTH_FAILURE:
		return "authentication-failure";
	case OSP_ACSE_DIAG_AUTH_REQUIRED:
		return "authentication-required";
	default:
		return "user-unknown";
	}
}

const char *osp_app_context_name(uint8_t ctx) {
	switch (ctx) {
	case OSP_CTX_LN:
		return "LN";
	case OSP_CTX_SN:
		return "SN";
	case OSP_CTX_LN_CIPHERING:
		return "LN-ciphering";
	case OSP_CTX_SN_CIPHERING:
		return "SN-ciphering";
	default:
		return "unknown";
	}
}

const char *osp_auth_mechanism_name(uint8_t mech) {
	switch (mech) {
	case OSP_MECH_LOWEST:
		return "lowest";
	case OSP_MECH_LLS:
		return "LLS";
	case OSP_MECH_HLS:
		return "HLS";
	case OSP_MECH_HLS_MD5:
		return "HLS-MD5";
	case OSP_MECH_HLS_SHA1:
		return "HLS-SHA1";
	case OSP_MECH_HLS_GMAC:
		return "HLS-GMAC";
	case OSP_MECH_HLS_SHA256:
		return "HLS-SHA256";
	case OSP_MECH_HLS_ECDSA:
		return "HLS-ECDSA";
	case OSP_MECH_HLS_GOST_CMAC:
		return "HLS-GOST-CMAC";
	case OSP_MECH_HLS_GOST_STREEBOG:
		return "HLS-GOST-Streebog";
	case OSP_MECH_HLS_GOST_SIG:
		return "HLS-GOST-SIG";
	default:
		return "unknown";
	}
}

const char *osp_initiate_error_name(uint8_t err) {
	switch (err) {
	case OSP_INITIATE_ERR_DLMS_VERSION_TOO_LOW:
		return "dlms-version-too-low";
	case OSP_INITIATE_ERR_INCOMPATIBLE_CONFORMANCE:
		return "incompatible-conformance";
	case OSP_INITIATE_ERR_PDU_SIZE_TOO_SHORT:
		return "pdu-size-too-short";
	default:
		return "unknown";
	}
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  AARQ
 * ═══════════════════════════════════════════════════════════════════════════ */

int osp_aarq_encode(osp_aarq_t *aarq, osp_buf_t *buf) {
	if (!aarq || !buf)
		return -1;

	osp_ber_write_tag(buf, 1, true, 0); /* [APPLICATION 0] constructed */

	/* Reserve 1-byte length placeholder, backpatch later */
	uint32_t len_pos = buf->wr;
	if (osp_buf_free(buf) < 1)
		return -1;
	buf->buf[buf->wr++] = 0x00;

	/* [1] application-context-name: EXPLICIT, wraps OID */
	osp_ber_write_tag(buf, 2, true, 1);
	uint32_t oid_len_pos = buf->wr;
	if (osp_buf_free(buf) < 1)
		return -1;
	buf->buf[buf->wr++] = 0x00; /* placeholder */
	ber_write_oid(buf, OID_PREFIX_APP, 6, aarq->application_context);
	if (ber_backpatch_length(buf, oid_len_pos) != 0)
		return -1;

	/* [10] sender-acse-requirements: IMPLICIT BIT STRING */
	osp_ber_write_tag(buf, 2, false, 10);
	osp_ber_write_length(buf, 2);
	osp_axdr_write_u8(buf, 7);    /* unused bits */
	osp_axdr_write_u8(buf, 0x80); /* auth required */

	/* [11] mechanism-name: IMPLICIT OID (no universal 06 tag) */
	osp_ber_write_tag(buf, 2, false, 11);
	osp_ber_write_length(buf, sizeof(OID_PREFIX_MECH) + 1);
	for (uint8_t i = 0; i < sizeof(OID_PREFIX_MECH); i++) {
		osp_axdr_write_u8(buf, OID_PREFIX_MECH[i]);
	}
	osp_axdr_write_u8(buf, aarq->mechanism);

	/* [6] calling-AP-title: EXPLICIT wraps OCTET STRING (system title)
	 * Per Rust spodes-rs: 0xA6 <len> 04 <datalen> <data> */
	if (aarq->calling_ap_title_len > 0) {
		osp_ber_write_tag(buf, 2, true, 6); /* A6: context constructed 6 */
		osp_ber_write_length(buf, aarq->calling_ap_title_len + 2);
		osp_axdr_write_u8(buf, 4); /* OCTET STRING tag */
		osp_axdr_write_u8(buf, aarq->calling_ap_title_len);
		for (uint8_t i = 0; i < aarq->calling_ap_title_len; i++) {
			osp_axdr_write_u8(buf, aarq->calling_ap_title[i]);
		}
	}

	/* [12] calling-authentication-value: EXPLICIT Authentication-value
	 * Per Green Book Table D.4: AC <len> 80 <datalen> <data>
	 * 80 = charstring [0] IMPLICIT GraphicString */
	osp_ber_write_tag(buf, 2, true, 12);
	osp_ber_write_length(buf, aarq->calling_auth_value_len + 2);
	osp_axdr_write_u8(buf, 0x80); /* charstring [0] */
	osp_axdr_write_u8(buf, aarq->calling_auth_value_len);
	for (uint8_t i = 0; i < aarq->calling_auth_value_len; i++) {
		osp_axdr_write_u8(buf, aarq->calling_auth_value[i]);
	}

	/* [30] user-information: EXPLICIT wraps OCTET STRING */
	osp_ber_write_tag(buf, 2, true, 30); /* BE: context constructed 30 */
	osp_ber_write_length(buf, aarq->user_info_len + 2);
	osp_axdr_write_u8(buf, 4); /* OCTET STRING tag */
	osp_axdr_write_u8(buf, aarq->user_info_len);
	for (uint32_t i = 0; i < aarq->user_info_len; i++) {
		osp_axdr_write_u8(buf, aarq->user_info[i]);
	}

	/* Backpatch outer AARQ length */
	if (ber_backpatch_length(buf, len_pos) != 0)
		return -1;

	return 0;
}

int osp_aarq_decode(osp_buf_t *buf, osp_aarq_t *aarq) {
	if (!buf || !aarq)
		return -1;

	memset(aarq, 0, sizeof(*aarq));

	osp_ber_tag_t tag;
	if (osp_ber_read_tag(buf, &tag) != OSP_OK || tag.tag_number != 0 || !tag.tag_constructed) {
		return -1;
	}

	uint32_t len;
	if (osp_ber_read_length(buf, &len) != OSP_OK) {
		return -1;
	}
	uint32_t end = buf->rd + len;

	while (buf->rd < end) {
		osp_ber_tag_t ftag;
		if (osp_ber_read_tag(buf, &ftag) != OSP_OK)
			break;

		uint32_t field_len;
		if (osp_ber_read_length(buf, &field_len) != OSP_OK)
			break;

		uint32_t field_start = buf->rd;

		if (ftag.tag_class == 2) {
			/* Context-specific: dispatch by tag number */
			switch (ftag.tag_number) {
				case 0: /* [0] protocol-version */
					if (field_len >= 2) {
						aarq->has_protocol_version = 1;
						aarq->protocol_version[0] = buf->buf[buf->rd++];
						aarq->protocol_version[1] = buf->buf[buf->rd++];
					}
					break;

				case 1: /* [1] application-context-name: EXPLICIT wraps OID */
					if (ber_read_oid(buf, &aarq->application_context) != 0)
						return -1;
					break;

				case 6: /* [6] calling-AP-title: EXPLICIT wraps OCTET STRING */
				{
					osp_ber_tag_t otag;
					if (osp_ber_read_tag(buf, &otag) != OSP_OK)
						return -1;
					uint32_t olen;
					if (osp_ber_read_length(buf, &olen) != OSP_OK)
						return -1;
					aarq->calling_ap_title_len = (uint8_t)(olen > 255u ? 255u : olen);
					for (uint32_t i = 0; i < olen; i++) {
						uint8_t b;
						if (osp_axdr_read_u8(buf, &b) != OSP_OK)
							return -1;
						if (i < sizeof(aarq->calling_ap_title)) {
							aarq->calling_ap_title[i] = b;
						}
					}
				}
					break;

				case 10: /* [10] sender-acse-requirements */
					if (field_len >= 2) {
						aarq->has_sender_acse_requirement = 1;
						buf->rd++; /* unused bits */
						aarq->sender_acse_requirement = buf->buf[buf->rd++];
					}
					break;

				case 11: /* [11] mechanism-name: IMPLICIT OID (no 06 tag prefix) */
				{
					/* IMPLICIT OID: field_len bytes of raw OID content */
					if (field_len < 7)
						return -1;
					buf->rd += 6;
					if (osp_axdr_read_u8(buf, &aarq->mechanism) != OSP_OK)
						return -1;
					aarq->has_mechanism = 1;
				}
					break;

				case 12: /* [12] calling-authentication-value: EXPLICIT wraps Authentication-value */
				{
					osp_ber_tag_t atag;
					if (osp_ber_read_tag(buf, &atag) == OSP_OK && atag.tag_number == 0 && !atag.tag_constructed) {
						uint32_t alen;
						if (osp_ber_read_length(buf, &alen) != OSP_OK)
							return -1;
						if (alen > sizeof(aarq->calling_auth_value))
							return -1;
						aarq->calling_auth_value_len = (uint8_t)alen;
						for (uint32_t i = 0; i < alen; i++) {
							if (osp_axdr_read_u8(buf, &aarq->calling_auth_value[i]) != OSP_OK)
								return -1;
						}
						aarq->has_calling_auth = 1;
					}
					break;
				}

				case 30: /* [30] user-information: EXPLICIT wraps OCTET STRING */
				{
					osp_ber_tag_t utag;
					if (osp_ber_read_tag(buf, &utag) != OSP_OK)
						return -1;
					uint32_t utlen;
					if (osp_ber_read_length(buf, &utlen) != OSP_OK)
						return -1;
					if (utlen > sizeof(aarq->user_info))
						return -1;
					aarq->user_info_len = utlen;
					for (uint32_t i = 0; i < utlen; i++) {
						if (osp_axdr_read_u8(buf, &aarq->user_info[i]) != OSP_OK)
							return -1;
					}
					break;
				}

				default:
					break;
			}
		}

		/* Ensure we consume exactly field_len bytes */
		uint32_t field_end = field_start + field_len;
		if (field_end < field_start || field_end > buf->wr) {
			return -1; /* overflow or read past buffer */
		}
		if (buf->rd < field_end) {
			buf->rd = field_end;
		}
	}

	return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  AARE
 * ═══════════════════════════════════════════════════════════════════════════ */

int osp_aare_encode(osp_aare_t *aare, osp_buf_t *buf) {
	if (!aare || !buf)
		return -1;

	osp_ber_write_tag(buf, 1, true, 1); /* [APPLICATION 1] constructed */

	/* Reserve 1-byte length placeholder, backpatch later */
	uint32_t len_pos = buf->wr;
	if (osp_buf_free(buf) < 1)
		return -1;
	buf->buf[buf->wr++] = 0x00;

	/* [0] protocol-version: IMPLICIT BIT STRING (optional) */
	if (aare->has_protocol_version) {
		osp_ber_write_tag(buf, 2, false, 0);
		osp_ber_write_length(buf, 2);
		osp_axdr_write_u8(buf, aare->protocol_version[0]);
		osp_axdr_write_u8(buf, aare->protocol_version[1]);
	}

	/* [1] application-context-name: EXPLICIT OID (optional) */
	if (aare->application_context != 0) {
		osp_ber_write_tag(buf, 2, true, 1);
		uint32_t oid_len_pos = buf->wr;
		if (osp_buf_free(buf) < 1)
			return -1;
		buf->buf[buf->wr++] = 0x00;
		ber_write_oid(buf, OID_PREFIX_APP, 6, aare->application_context);
		if (ber_backpatch_length(buf, oid_len_pos) != 0)
			return -1;
	}

	/* [2] result: EXPLICIT wraps INTEGER
	 * Per Green Book Table D.6: A2 03 02 01 <value> */
	osp_ber_write_tag(buf, 2, true, 2);
	osp_ber_write_length(buf, 3);
	osp_axdr_write_u8(buf, 2); /* INTEGER tag */
	osp_axdr_write_u8(buf, 1); /* length */
	osp_axdr_write_u8(buf, aare->result);

	/* [3] result-source-diagnostic: always present (null = 0 on accept) */
	osp_ber_write_tag(buf, 2, true, 3);
	osp_ber_write_length(buf, 5);
	osp_ber_write_tag(buf, 2, true, aare->result_source_is_provider ? 2 : 1);
	osp_ber_write_length(buf, 3);
	osp_axdr_write_u8(buf, 2); /* INTEGER tag */
	osp_axdr_write_u8(buf, 1); /* length */
	osp_axdr_write_u8(buf, aare->result_source_diagnostic);

	/* [4] responding-AP-title: EXPLICIT wraps OCTET STRING */
	if (aare->responding_ap_title_len > 0) {
		osp_ber_write_tag(buf, 2, true, 4); /* A4: context constructed 4 */
		osp_ber_write_length(buf, aare->responding_ap_title_len + 2);
		osp_axdr_write_u8(buf, 4); /* OCTET STRING tag */
		osp_axdr_write_u8(buf, aare->responding_ap_title_len);
		for (uint8_t i = 0; i < aare->responding_ap_title_len; i++) {
			osp_axdr_write_u8(buf, aare->responding_ap_title[i]);
		}
	}

	/* Auth fields: SPODES etalon LLS AARE omits these; HLS needs StoC in AA. */
	if (aare->include_authentication) {
		/* [8] responder-acse-requirements: IMPLICIT BIT STRING */
		osp_ber_write_tag(buf, 2, false, 8);
		osp_ber_write_length(buf, 2);
		osp_axdr_write_u8(buf, 7);
		osp_axdr_write_u8(buf, 0x80);

		/* [9] mechanism-name: IMPLICIT OID (no universal 06 tag) */
		osp_ber_write_tag(buf, 2, false, 9);
		osp_ber_write_length(buf, sizeof(OID_PREFIX_MECH) + 1);
		for (uint8_t i = 0; i < sizeof(OID_PREFIX_MECH); i++) {
			osp_axdr_write_u8(buf, OID_PREFIX_MECH[i]);
		}
		osp_axdr_write_u8(buf, aare->mechanism);

		/* [10] responding-authentication-value */
		osp_ber_write_tag(buf, 2, true, 10);
		osp_ber_write_length(buf, aare->responding_auth_value_len + 2);
		osp_axdr_write_u8(buf, 0x80); /* charstring [0] */
		osp_axdr_write_u8(buf, aare->responding_auth_value_len);
		for (uint8_t i = 0; i < aare->responding_auth_value_len; i++) {
			osp_axdr_write_u8(buf, aare->responding_auth_value[i]);
		}
	}

	/* [30] user-information: EXPLICIT wraps OCTET STRING */
	osp_ber_write_tag(buf, 2, true, 30);
	osp_ber_write_length(buf, aare->user_info_len + 2);
	osp_axdr_write_u8(buf, 4); /* OCTET STRING tag */
	osp_axdr_write_u8(buf, aare->user_info_len);
	for (uint32_t i = 0; i < aare->user_info_len; i++) {
		osp_axdr_write_u8(buf, aare->user_info[i]);
	}

	/* Backpatch outer AARE length */
	if (ber_backpatch_length(buf, len_pos) != 0)
		return -1;

	return 0;
}

int osp_aare_decode(osp_buf_t *buf, osp_aare_t *aare) {
	if (!buf || !aare)
		return -1;

	memset(aare, 0, sizeof(*aare));

	osp_ber_tag_t tag;
	if (osp_ber_read_tag(buf, &tag) != OSP_OK || tag.tag_number != 1 || !tag.tag_constructed) {
		return -1;
	}

	uint32_t len;
	if (osp_ber_read_length(buf, &len) != OSP_OK) {
		return -1;
	}
	uint32_t end = buf->rd + len;

	while (buf->rd < end) {
		osp_ber_tag_t ftag;
		if (osp_ber_read_tag(buf, &ftag) != OSP_OK)
			break;

		uint32_t field_len;
		if (osp_ber_read_length(buf, &field_len) != OSP_OK)
			break;

		uint32_t field_start = buf->rd;

		if (ftag.tag_class == 2) {
			/* Context-specific: dispatch by tag number */
			switch (ftag.tag_number) {
				case 0: /* [0] protocol-version: skip */
					break;

				case 1: /* [1] application-context-name: EXPLICIT wraps OID, skip */
					break;

				case 2: /* [2] result: EXPLICIT wraps INTEGER */
				{
					osp_ber_tag_t rtag;
					osp_ber_read_tag(buf, &rtag);
					uint32_t rlen;
					osp_ber_read_length(buf, &rlen);
					osp_axdr_read_u8(buf, &aare->result);
					break;
				}

				case 3: /* [3] result-source-diagnostic: EXPLICIT wraps CHOICE */
				{
					osp_ber_tag_t dtag;
					osp_ber_read_tag(buf, &dtag);
					uint32_t dlen;
					osp_ber_read_length(buf, &dlen);
					osp_ber_tag_t itag;
					osp_ber_read_tag(buf, &itag);
					uint32_t ilen;
					osp_ber_read_length(buf, &ilen);
					aare->result_source_is_provider = (itag.tag_number == 2) ? 1 : 0;
					osp_axdr_read_u8(buf, &aare->result_source_diagnostic);
					break;
				}

				case 4: /* [4] responding-AP-title: EXPLICIT wraps OCTET STRING */
				{
					osp_ber_tag_t otag;
					osp_ber_read_tag(buf, &otag);
					uint32_t olen;
					osp_ber_read_length(buf, &olen);
					aare->responding_ap_title_len = (uint8_t)olen;
					for (uint32_t i = 0; i < olen && i < 8; i++) {
						osp_axdr_read_u8(buf, &aare->responding_ap_title[i]);
					}
				}
					break;

				case 8: /* [8] responder-acse-requirements: skip */
					break;

				case 9: /* [9] mechanism-name: IMPLICIT OID (no 06 tag prefix) */
				{
					/* IMPLICIT OID: field_len bytes of raw OID content */
					if (field_len < 7)
						return -1;
					buf->rd += 6;
					if (osp_axdr_read_u8(buf, &aare->mechanism) != OSP_OK)
						return -1;
				}
					break;

				case 10: /* [10] responding-authentication-value: EXPLICIT wraps Authentication-value */
				{
					osp_ber_tag_t atag;
					if (osp_ber_read_tag(buf, &atag) == OSP_OK && atag.tag_number == 0 && !atag.tag_constructed) {
						uint32_t alen;
						osp_ber_read_length(buf, &alen);
						aare->responding_auth_value_len = (uint8_t)alen;
						for (uint32_t i = 0; i < alen && i < 64; i++) {
							osp_axdr_read_u8(buf, &aare->responding_auth_value[i]);
						}
					}
					break;
				}

				case 30: /* [30] user-information: EXPLICIT wraps OCTET STRING */
				{
					osp_ber_tag_t utag;
					osp_ber_read_tag(buf, &utag);
					uint32_t utlen;
					osp_ber_read_length(buf, &utlen);
					aare->user_info_len = utlen;
					for (uint32_t i = 0; i < utlen && i < 128; i++) {
						osp_axdr_read_u8(buf, &aare->user_info[i]);
					}
					break;
				}

				default:
					break;
			}
		}

		if (buf->rd < field_start + field_len) {
			buf->rd = field_start + field_len;
		}
	}

	return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RLRQ / RLRE
 * ═══════════════════════════════════════════════════════════════════════════ */

static int decode_release(osp_buf_t *buf, osp_rlrq_t *rl, uint8_t expected_tag_num) {
	if (!buf || !rl) {
		return -1;
	}
	osp_ber_tag_t tag;
	if (osp_ber_read_tag(buf, &tag) != OSP_OK) {
		return -1;
	}
	if (tag.tag_class != 1 || !tag.tag_constructed || tag.tag_number != expected_tag_num) {
		return -1;
	}
	uint32_t len;
	if (osp_ber_read_length(buf, &len) != OSP_OK) {
		return -1;
	}
	if (len < 1 || osp_buf_unread(buf) < 1) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &rl->reason) != OSP_OK) {
		return -1;
	}
	return 0;
}

static int encode_release(osp_buf_t *buf, uint8_t apdu_tag_num, uint8_t reason) {
	if (!buf) {
		return -1;
	}
	/* RLRQ/RLRE: [APPLICATION n] IMPLICIT SEQUENCE → wire tag 0x62 / 0x63 */
	if (osp_ber_write_tag(buf, 1, true, apdu_tag_num) != OSP_OK) {
		return -1;
	}
	if (osp_ber_write_length(buf, 1) != OSP_OK) {
		return -1;
	}
	if (osp_axdr_write_u8(buf, reason) != OSP_OK) {
		return -1;
	}
	return 0;
}

int osp_rlrq_encode(osp_rlrq_t *rlrq, osp_buf_t *buf) {
	return encode_release(buf, 2, rlrq ? rlrq->reason : 0);
}

int osp_rlre_encode(osp_rlrq_t *rlre, osp_buf_t *buf) {
	return encode_release(buf, 3, rlre ? rlre->reason : 0);
}

int osp_rlrq_decode(osp_buf_t *buf, osp_rlrq_t *rlrq) {
	return decode_release(buf, rlrq, 2);
}

int osp_rlre_decode(osp_buf_t *buf, osp_rlrq_t *rlre) {
	return decode_release(buf, rlre, 3);
}


/* GET/SET/ACTION: see xdms.c */

/* ═══════════════════════════════════════════════════════════════════════════
 *  EXCEPTION RESPONSE
 * ═══════════════════════════════════════════════════════════════════════════ */

int osp_exception_response_encode(osp_buf_t *buf, const osp_exception_response_t *resp) {
	if (!buf || !resp)
		return -1;
	osp_axdr_write_u8(buf, OSP_TAG_EXCEPTION_RESPONSE);
	osp_axdr_write_u8(buf, resp->invoke_id_priority);
	osp_axdr_write_u8(buf, resp->error_code);
	osp_axdr_write_u8(buf, resp->service_error);
	return 0;
}

int osp_exception_response_decode(osp_buf_t *buf, osp_exception_response_t *resp) {
	if (!buf || !resp)
		return -1;
	uint8_t tag;
	osp_axdr_read_u8(buf, &tag);
	if (tag != OSP_TAG_EXCEPTION_RESPONSE)
		return -1;
	osp_axdr_read_u8(buf, &resp->invoke_id_priority);
	osp_axdr_read_u8(buf, &resp->error_code);
	osp_axdr_read_u8(buf, &resp->service_error);
	return 0;
}

int osp_exception_response_encode_simple(osp_buf_t *buf, uint8_t state_error, uint8_t service_error) {
	osp_exception_response_t resp = {0};
	resp.error_code = state_error;
	resp.service_error = service_error;
	return osp_exception_response_encode(buf, &resp);
}

int osp_confirmed_service_error_encode(osp_buf_t *buf, const osp_confirmed_service_error_t *err) {
	if (!buf || !err) {
		return -1;
	}
	osp_axdr_write_u8(buf, OSP_TAG_CONFIRMED_SERVICE_ERROR);
	osp_axdr_write_u8(buf, err->service);
	osp_axdr_write_u8(buf, err->category);
	osp_axdr_write_u8(buf, err->value);
	return 0;
}

int osp_confirmed_service_error_decode(osp_buf_t *buf, osp_confirmed_service_error_t *err) {
	if (!buf || !err) {
		return -1;
	}
	uint8_t tag;
	if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_CONFIRMED_SERVICE_ERROR) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &err->service) != OSP_OK) {
		return -1;
	}
	if (osp_axdr_read_u8(buf, &err->category) != OSP_OK) {
		return -1;
	}
	return osp_axdr_read_u8(buf, &err->value) == OSP_OK ? 0 : -1;
}
