/**
 * xdms_selective.c — Selective access encode/decode for xDLMS APDUs
 *
 * Implements selective access parameters for ProfileGeneric buffer attribute
 * per IEC 62056-6-2 §4.3.6.2.9 and IEC 62056-5-3 Table 42.
 *
 * Access selectors:
 *   1 = range_descriptor (restriction by date/entry)
 *   2 = entry_descriptor (restriction by entry count)
 */

#include "xdms_selective.h"
#include "../codec/serialize.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  Encode
 * ═══════════════════════════════════════════════════════════════════════════ */

int osp_selective_access_encode(osp_buf_t *buf, const osp_selective_access_t *sa) {
	if (!buf || !sa) {
		return -1;
	}

	/* has_access_selection: 0 = none, 1 = has selection */
	if (sa->type == OSP_SEL_ACCESS_NONE) {
		osp_axdr_write_u8(buf, 0);
		return 0;
	}

	osp_axdr_write_u8(buf, 1);

	/* access_selector: 1 = range, 2 = entry */
	osp_axdr_write_u8(buf, 1);

	/* restriction_element ::= structure { restriction_type, restriction_value } */
	osp_axdr_write_u8(buf, OSP_TAG_STRUCTURE);
	osp_axdr_write_u8(buf, 2); /* element count */

	/* restriction_type: enum */
	osp_axdr_write_u8(buf, OSP_TAG_ENUM);
	switch (sa->type) {
		case OSP_SEL_ACCESS_BY_DATE:
			osp_axdr_write_u8(buf, 1);
			break;
		case OSP_SEL_ACCESS_BY_ENTRY:
		case OSP_SEL_ACCESS_BY_RANGE:
			osp_axdr_write_u8(buf, 2);
			break;
		default:
			osp_axdr_write_u8(buf, 0);
			break;
	}

	/* restriction_value: CHOICE */
	switch (sa->type) {
		case OSP_SEL_ACCESS_BY_DATE: {
			/* restriction_by_date ::= structure { from_date, to_date } */
			osp_axdr_write_u8(buf, OSP_TAG_STRUCTURE);
			osp_axdr_write_u8(buf, 2);
			osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
			osp_axdr_write_u8(buf, 12);
			osp_date_write(buf, &sa->param.date.from);
			osp_axdr_write_u8(buf, OSP_TAG_OCTETSTRING);
			osp_axdr_write_u8(buf, 12);
			osp_date_write(buf, &sa->param.date.to);
			break;
		}
		case OSP_SEL_ACCESS_BY_ENTRY:
		case OSP_SEL_ACCESS_BY_RANGE: {
			/* restriction_by_entry ::= structure { from_entry, to_entry } */
			osp_axdr_write_u8(buf, OSP_TAG_STRUCTURE);
			osp_axdr_write_u8(buf, 2);
			osp_axdr_write_u8(buf, OSP_TAG_DOUBLE_LONG_UNS);
			osp_axdr_write_u32(buf, sa->param.entry.from);
			osp_axdr_write_u8(buf, OSP_TAG_DOUBLE_LONG_UNS);
			osp_axdr_write_u32(buf, sa->param.entry.to);
			break;
		}
		default:
			osp_axdr_write_u8(buf, OSP_TAG_NULL);
			break;
	}

	return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Decode
 * ═══════════════════════════════════════════════════════════════════════════ */

static int decode_restriction_element(osp_buf_t *buf, osp_selective_access_t *sa) {
	uint8_t tag;
	if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_STRUCTURE) {
		return -1;
	}
	uint8_t count;
	if (osp_axdr_read_u8(buf, &count) != OSP_OK || count < 2) {
		return -1;
	}

	/* restriction_type: enum */
	if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_ENUM) {
		return -1;
	}
	uint8_t restriction_type;
	if (osp_axdr_read_u8(buf, &restriction_type) != OSP_OK) {
		return -1;
	}

	/* restriction_value: CHOICE */
	switch (restriction_type) {
		case 1: /* restriction by date */
			sa->type = OSP_SEL_ACCESS_BY_DATE;
			if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_STRUCTURE) {
				return -1;
			}
			if (osp_axdr_read_u8(buf, &count) != OSP_OK || count < 2) {
				return -1;
			}
			/* from_date */
			if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_OCTETSTRING) {
				return -1;
			}
			{
				uint8_t len;
				if (osp_axdr_read_u8(buf, &len) != OSP_OK || len != 12) {
					return -1;
				}
				osp_date_read(buf, &sa->param.date.from);
			}
			/* to_date */
			if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_OCTETSTRING) {
				return -1;
			}
			{
				uint8_t len;
				if (osp_axdr_read_u8(buf, &len) != OSP_OK || len != 12) {
					return -1;
				}
				osp_date_read(buf, &sa->param.date.to);
			}
			return 0;

		case 2: /* restriction by entry */
			sa->type = OSP_SEL_ACCESS_BY_ENTRY;
			if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_STRUCTURE) {
				return -1;
			}
			if (osp_axdr_read_u8(buf, &count) != OSP_OK || count < 2) {
				return -1;
			}
			/* from_entry */
			if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_DOUBLE_LONG_UNS) {
				return -1;
			}
			osp_axdr_read_u32(buf, &sa->param.entry.from);
			/* to_entry */
			if (osp_axdr_read_u8(buf, &tag) != OSP_OK || tag != OSP_TAG_DOUBLE_LONG_UNS) {
				return -1;
			}
			osp_axdr_read_u32(buf, &sa->param.entry.to);
			return 0;

		default: /* none or unknown — skip restriction_value */
			sa->type = OSP_SEL_ACCESS_NONE;
			osp_value_skip(buf);
			return 0;
	}
}

int osp_selective_access_decode(osp_buf_t *buf, osp_selective_access_t *sa) {
	if (!buf || !sa) {
		return -1;
	}

	memset(sa, 0, sizeof(*sa));

	/* has_access_selection */
	uint8_t has_selection;
	if (osp_axdr_read_u8(buf, &has_selection) != OSP_OK) {
		return -1;
	}
	if (has_selection == 0) {
		sa->type = OSP_SEL_ACCESS_NONE;
		return 0;
	}

	/* access_selector */
	uint8_t access_selector;
	if (osp_axdr_read_u8(buf, &access_selector) != OSP_OK) {
		return -1;
	}

	/* For now, only support selector 1 (range_descriptor) */
	if (access_selector != 1) {
		/* Unknown selector — skip the rest */
		osp_value_skip(buf);
		return 0;
	}

	/* restriction_element */
	return decode_restriction_element(buf, sa);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Skip (for server-side decode when selective access not needed)
 * ═══════════════════════════════════════════════════════════════════════════ */

int osp_selective_access_skip(osp_buf_t *buf) {
	if (!buf) {
		return -1;
	}

	uint8_t has_selection;
	if (osp_axdr_read_u8(buf, &has_selection) != OSP_OK) {
		return -1;
	}
	if (has_selection == 0) {
		return 0;
	}

	/* access_selector */
	osp_axdr_read_u8(buf, NULL);

	/* restriction_element — skip the structure */
	return osp_value_skip(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Apply selective access filter to ProfileGeneric buffer rows
 * ═══════════════════════════════════════════════════════════════════════════ */

int osp_selective_access_apply_to_buffer(const osp_selective_access_t *sa,
                                          osp_profile_row_t *rows, uint8_t *row_count) {
	if (!sa || !rows || !row_count) {
		return -1;
	}

	/* No filter — return all rows */
	if (sa->type == OSP_SEL_ACCESS_NONE || *row_count == 0) {
		return *row_count;
	}

	uint8_t original_count = *row_count;
	uint8_t filtered = 0;

	/* For now, support entry-based filtering only.
	 * The "from" and "to" are 1-based entry indices (row 0 = entry 1). */
	uint32_t from = 0;
	uint32_t to = 0xFFFFFFFF;

	switch (sa->type) {
		case OSP_SEL_ACCESS_BY_ENTRY:
		case OSP_SEL_ACCESS_BY_RANGE:
			from = sa->param.entry.from;
			to = sa->param.entry.to;
			break;
		case OSP_SEL_ACCESS_BY_DATE:
			/* Date-based filtering requires timestamp comparison —
			 * for now, fall through and return all rows.
			 * Full implementation requires a timestamp column index. */
			return original_count;
		default:
			return original_count;
	}

	/* Filter by 1-based entry index.
	 * entry_from=1 means first row (index 0).
	 * entry_to=0 means all remaining rows. */
	for (uint8_t i = 0; i < original_count; i++) {
		uint32_t entry_num = i + 1; /* 1-based */
		if (entry_num >= from && (to == 0 || entry_num <= to)) {
			if (filtered != i) {
				rows[filtered] = rows[i];
			}
			filtered++;
		}
	}

	*row_count = filtered;
	return filtered;
}
