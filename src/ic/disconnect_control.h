/**
 * @file disconnect_control.h
 * @brief Disconnect Control interface class (class_id = 70, IEC 62056-6-2)
 *
 * @par Attributes
 * | # | Name                        | Type         | Access |
 * |---|-----------------------------|--------------|--------|
 * | 1 | logical_name                | octet-string | RO     |
 * | 2 | output_state                | unsigned     | RW     |
 * | 3 | control_mode                | unsigned     | RW     |
 * | 4 | control_leads_state         | unsigned     | RO     |
 * | 5 | control_leads_configuration | structure    | RW     |
 *
 * @par Methods
 * | # | Name               | Argument | Description                                |
 * |---|--------------------|----------|--------------------------------------------|
 * | 1 | remote_disconnect  | data     | Remotely disconnects the load              |
 * | 2 | remote_reconnect   | data     | Remotely reconnects the load               |
 */
#ifndef OSP_IC_DISCONNECT_CONTROL_H
#define OSP_IC_DISCONNECT_CONTROL_H
#include "../openspodes.h"

/**
 * @brief Disconnect Control object structure
 *
 * Manages remote disconnect/reconnect operations for load control,
 * with configurable control modes and power failure tracking.
 */
typedef struct {
	osp_obis_t logical_name;                  /**< OBIS logical name */
	uint8_t output_state;                     /**< Current output state */
	uint8_t control_state;                    /**< Current control state */
	uint8_t control_model;                    /**< Control model type */
	osp_obis_t control_mode_script;           /**< Script for control mode changes */
	osp_obis_t normal_position_script;        /**< Script for normal position */
	osp_obis_t disconnect_position_script;    /**< Script for disconnect position */
	uint32_t power_failure_counter;           /**< Power failure event counter */
} osp_ic_disconnect_control_t;

/**
 * @brief Initialize a Disconnect Control IC object
 * @param d  Pointer to disconnect control object to initialize
 * @param ln OBIS logical name
 */
void osp_ic_disconnect_control_init(osp_ic_disconnect_control_t *d, osp_obis_t ln);

/**
 * @brief Get the class descriptor for the Disconnect Control IC
 * @return Pointer to the class descriptor structure
 */
const osp_ic_class_t *osp_ic_disconnect_control_class(void);
#endif