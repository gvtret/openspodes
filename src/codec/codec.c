/**
 * codec.c — BER/AXDR codec implementation
 *
 * Zero-copy operations on osp_buf_t.
 * Based on spodes-rs serialization patterns.
 */

#include "codec.h"
#include <string.h>

/* ── BER read ────────────────────────────────────────────────────────────── */

osp_err_t osp_ber_read_tag(osp_buf_t *buf, osp_ber_tag_t *tag) {
	if (!buf || !tag || osp_buf_unread(buf) == 0) {
		return OSP_ERR_INVALID;
	}

	uint8_t first = buf->buf[buf->rd++];
	tag->tag_class = (first >> 6) & 0x03;
	tag->tag_constructed = (first >> 5) & 0x01;
	tag->tag_number = first & 0x1F;

	/* Long tag number */
	if (tag->tag_number == 0x1F) {
		tag->tag_number = 0;
		uint8_t byte;
		do {
			if (osp_buf_unread(buf) == 0) {
				return OSP_ERR_INVALID;
			}
			byte = buf->buf[buf->rd++];
			tag->tag_number = (tag->tag_number << 7) | (byte & 0x7F);
		} while (byte & 0x80);
	}

	return OSP_OK;
}

osp_err_t osp_ber_read_length(osp_buf_t *buf, uint32_t *len) {
	if (!buf || !len || osp_buf_unread(buf) == 0) {
		return OSP_ERR_INVALID;
	}

	uint8_t first = buf->buf[buf->rd++];

	if (first < 0x80) {
		*len = first;
	} else if (first == 0x81) {
		if (osp_buf_unread(buf) < 1) {
			return OSP_ERR_INVALID;
		}
		*len = buf->buf[buf->rd++];
	} else if (first == 0x82) {
		if (osp_buf_unread(buf) < 2) {
			return OSP_ERR_INVALID;
		}
		*len = ((uint32_t)buf->buf[buf->rd++] << 8) | buf->buf[buf->rd++];
	} else {
		return OSP_ERR_UNSUPPORTED;
	}

	return OSP_OK;
}

osp_err_t osp_ber_read_uint(osp_buf_t *buf, uint32_t *val) {
	if (!buf || !val || osp_buf_unread(buf) == 0) {
		return OSP_ERR_INVALID;
	}

	*val = 0;
	while (osp_buf_unread(buf) > 0) {
		*val = (*val << 8) | buf->buf[buf->rd++];
	}
	return OSP_OK;
}

osp_err_t osp_ber_read_octet_string(osp_buf_t *buf, uint8_t *out, uint32_t max_len, uint32_t *out_len) {
	osp_ber_tag_t tag;
	osp_err_t r = osp_ber_read_tag(buf, &tag);
	if (r != OSP_OK) {
		return r;
	}
	if (tag.tag_number != 4) {
		return OSP_ERR_INVALID; /* not OCTET STRING */
	}

	uint32_t len;
	r = osp_ber_read_length(buf, &len);
	if (r != OSP_OK) {
		return r;
	}
	if (len > max_len) {
		return OSP_ERR_NOMEM;
	}
	if (osp_buf_unread(buf) < len) {
		return OSP_ERR_INVALID;
	}

	memcpy(out, &buf->buf[buf->rd], len);
	buf->rd += len;
	if (out_len) {
		*out_len = len;
	}
	return OSP_OK;
}

/* ── BER write ───────────────────────────────────────────────────────────── */

osp_err_t osp_ber_write_tag(osp_buf_t *buf, uint8_t tag_class, bool constructed, uint8_t tag_number) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}

	uint8_t first = ((tag_class & 0x03) << 6) | ((constructed ? 1 : 0) << 5);

	if (tag_number < 0x1F) {
		if (osp_buf_free(buf) < 1) {
			return OSP_ERR_NOMEM;
		}
		buf->buf[buf->wr++] = first | tag_number;
	} else {
		/* Long tag: need 2+ bytes */
		if (osp_buf_free(buf) < 2) {
			return OSP_ERR_NOMEM;
		}
		buf->buf[buf->wr++] = first | 0x1F;
		buf->buf[buf->wr++] = tag_number & 0x7F;
	}
	return OSP_OK;
}

osp_err_t osp_ber_write_length(osp_buf_t *buf, uint32_t len) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}

	if (len < 0x80) {
		if (osp_buf_free(buf) < 1) {
			return OSP_ERR_NOMEM;
		}
		buf->buf[buf->wr++] = (uint8_t)len;
	} else if (len <= 0xFF) {
		if (osp_buf_free(buf) < 2) {
			return OSP_ERR_NOMEM;
		}
		buf->buf[buf->wr++] = 0x81;
		buf->buf[buf->wr++] = (uint8_t)len;
	} else if (len <= 0xFFFF) {
		if (osp_buf_free(buf) < 3) {
			return OSP_ERR_NOMEM;
		}
		buf->buf[buf->wr++] = 0x82;
		buf->buf[buf->wr++] = (uint8_t)(len >> 8);
		buf->buf[buf->wr++] = (uint8_t)(len & 0xFF);
	} else {
		return OSP_ERR_UNSUPPORTED;
	}
	return OSP_OK;
}

osp_err_t osp_ber_write_uint(osp_buf_t *buf, uint32_t val) {
	if (!buf) {
		return OSP_ERR_INVALID;
	}

	if (val < 0x80) {
		if (osp_buf_free(buf) < 1) {
			return OSP_ERR_NOMEM;
		}
		buf->buf[buf->wr++] = (uint8_t)val;
	} else if (val <= 0xFF) {
		if (osp_buf_free(buf) < 2) {
			return OSP_ERR_NOMEM;
		}
		buf->buf[buf->wr++] = (uint8_t)val;
	} else if (val <= 0xFFFF) {
		if (osp_buf_free(buf) < 3) {
			return OSP_ERR_NOMEM;
		}
		buf->buf[buf->wr++] = (uint8_t)(val >> 8);
		buf->buf[buf->wr++] = (uint8_t)(val & 0xFF);
	} else {
		if (osp_buf_free(buf) < 5) {
			return OSP_ERR_NOMEM;
		}
		buf->buf[buf->wr++] = (uint8_t)(val >> 24);
		buf->buf[buf->wr++] = (uint8_t)(val >> 16);
		buf->buf[buf->wr++] = (uint8_t)(val >> 8);
		buf->buf[buf->wr++] = (uint8_t)(val & 0xFF);
	}
	return OSP_OK;
}

osp_err_t osp_ber_write_octet_string(osp_buf_t *buf, const uint8_t *data, uint32_t len) {
	osp_err_t r = osp_ber_write_tag(buf, 0, false, 4);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_ber_write_length(buf, len);
	if (r != OSP_OK) {
		return r;
	}
	if (osp_buf_free(buf) < len) {
		return OSP_ERR_NOMEM;
	}
	memcpy(&buf->buf[buf->wr], data, len);
	buf->wr += len;
	return OSP_OK;
}

/* ── AXDR read ───────────────────────────────────────────────────────────── */

osp_err_t osp_axdr_read_tag(osp_buf_t *buf, uint8_t *tag) {
	if (!buf || !tag || osp_buf_unread(buf) < 1) {
		return OSP_ERR_INVALID;
	}
	*tag = buf->buf[buf->rd++];
	return OSP_OK;
}

osp_err_t osp_axdr_read_u8(osp_buf_t *buf, uint8_t *val) {
	if (!buf || !val || osp_buf_unread(buf) < 1) {
		return OSP_ERR_INVALID;
	}
	*val = buf->buf[buf->rd++];
	return OSP_OK;
}

osp_err_t osp_axdr_read_u16(osp_buf_t *buf, uint16_t *val) {
	if (!buf || !val || osp_buf_unread(buf) < 2) {
		return OSP_ERR_INVALID;
	}
	*val = ((uint16_t)buf->buf[buf->rd] << 8) | buf->buf[buf->rd + 1];
	buf->rd += 2;
	return OSP_OK;
}

osp_err_t osp_axdr_read_u32(osp_buf_t *buf, uint32_t *val) {
	if (!buf || !val || osp_buf_unread(buf) < 4) {
		return OSP_ERR_INVALID;
	}
	*val = ((uint32_t)buf->buf[buf->rd] << 24) | ((uint32_t)buf->buf[buf->rd + 1] << 16) | ((uint32_t)buf->buf[buf->rd + 2] << 8) |
	    (uint32_t)buf->buf[buf->rd + 3];
	buf->rd += 4;
	return OSP_OK;
}

osp_err_t osp_axdr_read_bool(osp_buf_t *buf, bool *val) {
	if (!buf || !val || osp_buf_unread(buf) < 1) {
		return OSP_ERR_INVALID;
	}
	*val = (buf->buf[buf->rd++] != 0);
	return OSP_OK;
}

osp_err_t osp_axdr_read_octet_string(osp_buf_t *buf, uint8_t *out, uint32_t max_len, uint32_t *out_len) {
	uint8_t tag;
	osp_err_t r = osp_axdr_read_tag(buf, &tag);
	if (r != OSP_OK) {
		return r;
	}
	if (tag != OSP_AXDR_OCTETSTRING) {
		return OSP_ERR_INVALID;
	}

	uint32_t len;
	r = osp_axdr_read_u32(buf, &len);
	if (r != OSP_OK) {
		return r;
	}
	if (len > max_len) {
		return OSP_ERR_NOMEM;
	}
	if (osp_buf_unread(buf) < len) {
		return OSP_ERR_INVALID;
	}

	memcpy(out, &buf->buf[buf->rd], len);
	buf->rd += len;
	if (out_len) {
		*out_len = len;
	}
	return OSP_OK;
}

/* ── AXDR write ──────────────────────────────────────────────────────────── */

osp_err_t osp_axdr_write_tag(osp_buf_t *buf, uint8_t tag) {
	if (!buf || osp_buf_free(buf) < 1) {
		return OSP_ERR_INVALID;
	}
	buf->buf[buf->wr++] = tag;
	return OSP_OK;
}

osp_err_t osp_axdr_write_u8(osp_buf_t *buf, uint8_t val) {
	if (!buf || osp_buf_free(buf) < 1) {
		return OSP_ERR_INVALID;
	}
	buf->buf[buf->wr++] = val;
	return OSP_OK;
}

osp_err_t osp_axdr_write_u16(osp_buf_t *buf, uint16_t val) {
	if (!buf || osp_buf_free(buf) < 2) {
		return OSP_ERR_INVALID;
	}
	buf->buf[buf->wr++] = (uint8_t)(val >> 8);
	buf->buf[buf->wr++] = (uint8_t)(val & 0xFF);
	return OSP_OK;
}

osp_err_t osp_axdr_write_u32(osp_buf_t *buf, uint32_t val) {
	if (!buf || osp_buf_free(buf) < 4) {
		return OSP_ERR_INVALID;
	}
	buf->buf[buf->wr++] = (uint8_t)(val >> 24);
	buf->buf[buf->wr++] = (uint8_t)(val >> 16);
	buf->buf[buf->wr++] = (uint8_t)(val >> 8);
	buf->buf[buf->wr++] = (uint8_t)(val & 0xFF);
	return OSP_OK;
}

osp_err_t osp_axdr_write_bool(osp_buf_t *buf, bool val) {
	if (!buf || osp_buf_free(buf) < 1) {
		return OSP_ERR_INVALID;
	}
	buf->buf[buf->wr++] = val ? 1 : 0;
	return OSP_OK;
}

osp_err_t osp_axdr_write_octet_string(osp_buf_t *buf, const uint8_t *data, uint32_t len) {
	osp_err_t r = osp_axdr_write_tag(buf, OSP_AXDR_OCTETSTRING);
	if (r != OSP_OK) {
		return r;
	}
	r = osp_axdr_write_u32(buf, len);
	if (r != OSP_OK) {
		return r;
	}
	if (osp_buf_free(buf) < len) {
		return OSP_ERR_NOMEM;
	}
	memcpy(&buf->buf[buf->wr], data, len);
	buf->wr += len;
	return OSP_OK;
}
