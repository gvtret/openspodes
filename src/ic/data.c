/**
 * data.c — Data interface class (class_id = 1)
 *
 * Minimal IC proving the vtable architecture works.
 */

#include "data.h"
#include "../codec/codec.h"
#include <string.h>

/* ── Instance operations ─────────────────────────────────────────────────── */

static osp_err_t data_get_attr(const void *inst, uint8_t attr_id, osp_buf_t *buf)
{
    const osp_ic_data_t *d = (const osp_ic_data_t *)inst;
    if (attr_id != 1) return OSP_ERR_NOT_FOUND;

    osp_err_t r = osp_axdr_write_u8(buf, (uint8_t)d->value_type);
    if (r != OSP_OK) return r;

    switch (d->value_type) {
    case OSP_TYPE_UINT32:
        return osp_axdr_write_u32(buf, *(const uint32_t *)d->value_buf);
    case OSP_TYPE_OCTETSTRING:
        return osp_axdr_write_octet_string(buf, d->value_buf, d->value_len);
    case OSP_TYPE_BOOLEAN:
        return osp_axdr_write_bool(buf, d->value_buf[0] != 0);
    default:
        return OSP_ERR_UNSUPPORTED;
    }
}

static osp_err_t data_set_attr(void *inst, uint8_t attr_id, const osp_buf_t *buf)
{
    osp_ic_data_t *d = (osp_ic_data_t *)inst;
    if (attr_id != 1) return OSP_ERR_NOT_FOUND;
    if (!buf || osp_buf_unread(buf) < 1) return OSP_ERR_INVALID;

    uint8_t tag;
    osp_err_t r = osp_axdr_read_tag((osp_buf_t *)buf, &tag);
    if (r != OSP_OK) return r;

    d->value_type = (osp_type_t)tag;

    switch (tag) {
    case OSP_TYPE_UINT32:
        if (osp_buf_unread(buf) < 4) return OSP_ERR_INVALID;
        /* Read big-endian AXDR bytes into native uint32_t */
        {
            uint32_t v = ((uint32_t)buf->buf[buf->rd] << 24) |
                         ((uint32_t)buf->buf[buf->rd+1] << 16) |
                         ((uint32_t)buf->buf[buf->rd+2] << 8) |
                         (uint32_t)buf->buf[buf->rd+3];
            memcpy(d->value_buf, &v, sizeof(v));
        }
        d->value_len = 4;
        break;
    case OSP_TYPE_BOOLEAN:
        if (osp_buf_unread(buf) < 1) return OSP_ERR_INVALID;
        d->value_buf[0] = buf->buf[buf->rd] != 0 ? 1 : 0;
        d->value_len = 1;
        break;
    default:
        return OSP_ERR_UNSUPPORTED;
    }
    return OSP_OK;
}

/* ── Attribute metadata ──────────────────────────────────────────────────── */

static const osp_ic_attr_t data_attrs[] = {
    { 1, 0x01, OSP_TYPE_OCTETSTRING },
};

/* ── Vtable ──────────────────────────────────────────────────────────────── */

static const osp_ic_class_t data_class = {
    .name          = "Data",
    .class_id      = 1,
    .version       = 0,
    .attrs         = data_attrs,
    .attr_count    = 1,
    .methods       = NULL,
    .method_count  = 0,
    .get_attr      = data_get_attr,
    .set_attr      = data_set_attr,
    .invoke        = NULL,
    .instance_size = sizeof(osp_ic_data_t),
};

const osp_ic_class_t *osp_ic_data_class(void)
{
    return &data_class;
}

void osp_ic_data_init(osp_ic_data_t *data, osp_obis_t ln)
{
    if (!data) return;
    memset(data, 0, sizeof(*data));
    data->logical_name = ln;
}
