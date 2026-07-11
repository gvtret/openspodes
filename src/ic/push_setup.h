/**
 * @file push_setup.h
 * @brief Push Setup interface class (class_id = 40, IEC 62056-6-2)
 *
 * @par Attributes
 * | #  | Name                        | Type                          | Access |
 * |----|-----------------------------|-------------------------------|--------|
 * | 1  | logical_name                | octet-string                  | RO     |
 * | 2  | push_object_list            | array                         | RW     |
 * | 3  | send_destination_and_method | structure                     | RW     |
 * | 4  | communication_window        | array                         | RW     |
 * | 5  | randomisation_start_interval| long-unsigned                 | RW     |
 * | 6  | number_of_retries           | unsigned                      | RW     |
 * | 7  | repetition_delay            | structure                     | RW     |
 * | 8  | port_reference              | octet-string                  | RW     |
 * | 9  | push_client_SAP             | integer                       | RW     |
 * | 10 | push_protection_parameters  | array                         | RW     |
 * | 11 | push_operation_method       | enum                          | RW     |
 * | 12 | confirmation_parameters     | structure                     | RW     |
 * | 13 | last_confirmation_date_time | datetime                      | RO     |
 *
 * @par Methods
 * | # | Name | Argument | Description                                |
 * |---|------|----------|--------------------------------------------|
 * | 1 | push | data     | Initiates a push operation to the destination |
 * | 2 | reset| data     | Resets push configuration and counters       |
 */
#ifndef OSP_IC_PUSH_SETUP_H
#define OSP_IC_PUSH_SETUP_H
#include "../openspodes.h"
#include "../codec/structures.h"
#include "../server/server.h"

/**
 * @brief Push Setup object structure
 *
 * Configures and manages push operations for sending data to
 * remote clients on a scheduled or triggered basis.
 */
typedef struct {
	osp_obis_t logical_name;                              /**< OBIS logical name */
	osp_push_object_t push_object_list[OSP_MAX_PUSH_OBJECTS]; /**< List of objects to push */
	uint8_t push_object_count;                            /**< Number of push objects */
	osp_send_destination_t send_destination;              /**< Destination address and method */
	osp_comm_window_t communication_window[OSP_MAX_COMM_WINDOW]; /**< Allowed communication windows */
	uint8_t comm_window_count;                            /**< Number of communication windows */
	uint16_t randomisation_start_interval;                /**< Randomisation start interval in seconds */
	uint8_t number_of_retries;                            /**< Number of retry attempts */
	uint16_t repetition_delay;                            /**< Delay between retries in seconds */
	osp_server_t *server;                                 /**< Reference to the server instance */
} osp_ic_push_setup_t;

/**
 * @brief Initialize a Push Setup IC object
 * @param p  Pointer to push setup object to initialize
 * @param ln OBIS logical name
 */
void osp_ic_push_setup_init(osp_ic_push_setup_t *p, osp_obis_t ln);

/**
 * @brief Bind a server instance to the push setup
 * @param p      Pointer to push setup object
 * @param server Pointer to the server instance
 */
void osp_ic_push_setup_bind_server(osp_ic_push_setup_t *p, osp_server_t *server);

/**
 * @brief Get the class descriptor for the Push Setup IC
 * @return Pointer to the class descriptor structure
 */
const osp_ic_class_t *osp_ic_push_setup_class(void);
#endif