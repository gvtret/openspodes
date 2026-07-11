/**
 * xdms_selective.h — Selective access encode/decode
 */

#ifndef OSP_XDMS_SELECTIVE_H
#define OSP_XDMS_SELECTIVE_H

#include "../openspodes.h"
#include "../codec/structures.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Encode selective access parameters (has_access_selection + restriction_element) */
int osp_selective_access_encode(osp_buf_t *buf, const osp_selective_access_t *sa);

/* Decode selective access parameters */
int osp_selective_access_decode(osp_buf_t *buf, osp_selective_access_t *sa);

/* Skip selective access bytes (server-side when not needed) */
int osp_selective_access_skip(osp_buf_t *buf);

/* Apply selective access filter to ProfileGeneric buffer rows.
 * Modifies `out_rows` in-place: sets row_count to filtered count.
 * Returns number of matching rows, or -1 on error. */
int osp_selective_access_apply_to_buffer(const osp_selective_access_t *sa,
                                          osp_profile_row_t *rows, uint8_t *row_count);

#ifdef __cplusplus
}
#endif

#endif /* OSP_XDMS_SELECTIVE_H */
