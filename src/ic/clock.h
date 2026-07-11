/**
 * @file clock.h
 * @brief Clock interface class (class_id = 8, IEC 62056-6-2)
 *
 * @par Attributes
 * | # | Name                      | Type            | Access |
 * |---|---------------------------|-----------------|--------|
 * | 1 | logical_name              | octet-string    | RO     |
 * | 2 | time                      | octet-string    | RW     |
 * | 3 | time_zone                 | long            | RW     |
 * | 4 | status                    | unsigned        | RW     |
 * | 5 | daylight_savings_begin    | octet-string    | RW     |
 * | 6 | daylight_savings_end      | octet-string    | RW     |
 * | 7 | daylight_savings_deviation| integer         | RW     |
 * | 8 | daylight_savings_enabled  | boolean         | RW     |
 * | 9 | clock_base                | enum            | RO     |
 *
 * @par Methods
 * | # | Name                    | Argument | Description                                       |
 * |---|-------------------------|----------|---------------------------------------------------|
 * | 1 | adjust_to_meter_time    | data     | Adjusts clock to meter's local time               |
 * | 2 | adjust_to_gps_time      | data     | Adjusts clock to GPS reference time               |
 * | 3 | adjust_to_abstract_time | data     | Adjusts clock to abstract time value              |
 * | 4 | preset_time             | data     | Presets the clock to a specified time value        |
 * | 5 | shift_time              | data     | Shifts the clock by a relative time offset         |
 * | 6 | shift_time_positive     | data     | Shifts the clock forward by a relative time offset |
 */
#ifndef OSP_IC_CLOCK_H
#define OSP_IC_CLOCK_H
#include "../openspodes.h"

/**
 * @brief Clock object structure
 *
 * Manages the device clock with time zone, daylight savings,
 * and various time adjustment methods.
 */
typedef struct {
	osp_obis_t logical_name;                    /**< OBIS logical name */
	osp_cosem_datetime_t time;                  /**< Current time */
	uint32_t timezone_offset;                   /**< Time zone offset in minutes from UTC */
	uint8_t clock_status;                       /**< Clock status: 0=ok, 1=error */
	osp_cosem_datetime_t daylight_savings_begin; /**< Daylight savings start time */
	osp_cosem_datetime_t daylight_savings_end;  /**< Daylight savings end time */
	uint8_t daylight_savings_deviation;         /**< Daylight savings deviation in minutes */
	uint8_t daylight_savings_enabled;           /**< Daylight savings enabled flag */
	uint8_t clock_base;                         /**< Clock base reference type */
} osp_ic_clock_t;

/**
 * @brief Initialize a Clock IC object
 * @param c  Pointer to clock object to initialize
 * @param ln OBIS logical name
 */
void osp_ic_clock_init(osp_ic_clock_t *c, osp_obis_t ln);

/**
 * @brief Get the class descriptor for the Clock IC
 * @return Pointer to the class descriptor structure
 */
const osp_ic_class_t *osp_ic_clock_class(void);
#endif