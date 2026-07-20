/**
 * data_hal.h — Data HAL interface for reading current data, writing setpoints,
 *              and executing commands on hardware.
 *
 * Plug in before osp_server_accept(). If osp_hal_data == NULL,
 * IC classes fall back to their cached value fields (existing behavior).
 */

#ifndef OSP_DATA_HAL_H
#define OSP_DATA_HAL_H

#include "openspodes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	/** Read current value from hardware.
	 *  @param ctx       User context
	 *  @param obis      Object OBIS address
	 *  @param attr_id   Attribute ID (1-based)
	 *  @param result    Output value
	 *  @return OSP_OK on success, OSP_ERR_NOT_FOUND if unknown, OSP_ERR_IO on hardware error */
	osp_err_t (*read)(void *ctx, const osp_obis_t *obis, uint8_t attr_id, osp_value_t *result);

	/** Write setpoint to hardware.
	 *  @param ctx       User context
	 *  @param obis      Object OBIS address
	 *  @param attr_id   Attribute ID (1-based)
	 *  @param value     Value to write
	 *  @return OSP_OK on success, OSP_ERR_NOT_FOUND if unknown, OSP_ERR_IO on hardware error */
	osp_err_t (*write)(void *ctx, const osp_obis_t *obis, uint8_t attr_id, const osp_value_t *value);

	/** Execute command on hardware.
	 *  @param ctx       User context
	 *  @param obis      Object OBIS address
	 *  @param method_id Method ID (1-based)
	 *  @param param     Input parameter (may be NULL)
	 *  @param result    Output result (may be NULL)
	 *  @return OSP_OK on success, OSP_ERR_NOT_FOUND if unknown, OSP_ERR_IO on hardware error */
	osp_err_t (*execute)(void *ctx, const osp_obis_t *obis, uint8_t method_id,
	                     const osp_value_t *param, osp_value_t *result);

	void *ctx;
} osp_hal_data_t;

/** Global data HAL pointer. Set before osp_server_accept() / osp_client_connect().
 *  NULL = no hardware access (default, existing behavior). */
extern osp_hal_data_t *osp_hal_data;

#ifdef __cplusplus
}
#endif
#endif
