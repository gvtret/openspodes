/**
 * structures.h — Complete COSEM composite structures (IEC 62056-6-2)
 *
 * Structures for ALL 31 IC classes implemented in spodes-rs.
 * Organized by IC class. No malloc. MCU-safe with configurable max sizes.
 *
 * Reference: IEC 62056-6-2 ED4, Blue Book 2015.
 */

#ifndef OSP_STRUCTURES_H
#define OSP_STRUCTURES_H

#include "../openspodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 *  CONFIGURABLE MAX SIZES
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifndef OSP_MAX_NAME_LEN
#define OSP_MAX_NAME_LEN 64
#endif
#ifndef OSP_MAX_OBJECT_LIST
/* Must fit in osp_value array count/capacity (uint8_t); Category A needs ~205. */
#define OSP_MAX_OBJECT_LIST 255
#endif
#ifndef OSP_MAX_ACCESS_ITEMS
#define OSP_MAX_ACCESS_ITEMS 16
#endif
#ifndef OSP_MAX_METHOD_ITEMS
#define OSP_MAX_METHOD_ITEMS 16
#endif
#ifndef OSP_MAX_CAPTURE_OBJECTS
/* Monthly billing profile (1.0.98.1) needs ~30 capture columns (SPODES / test #40). */
#define OSP_MAX_CAPTURE_OBJECTS 32
#endif
#ifndef OSP_MAX_BUFFER_ROWS
#define OSP_MAX_BUFFER_ROWS 32
#endif
#ifndef OSP_MAX_SEASON_PROFILE
#define OSP_MAX_SEASON_PROFILE 8
#endif
#ifndef OSP_MAX_WEEK_PROFILE
#define OSP_MAX_WEEK_PROFILE 8
#endif
#ifndef OSP_MAX_DAY_PROFILE
#define OSP_MAX_DAY_PROFILE 8
#endif
#ifndef OSP_MAX_DAY_ACTION
#define OSP_MAX_DAY_ACTION 12
#endif
#ifndef OSP_MAX_SCRIPT_PER_ACTION
#define OSP_MAX_SCRIPT_PER_ACTION 4
#endif
#ifndef OSP_MAX_SCHEDULE_ENTRY
#define OSP_MAX_SCHEDULE_ENTRY 16
#endif
#ifndef OSP_MAX_PUSH_OBJECTS
#define OSP_MAX_PUSH_OBJECTS 16
#endif
#ifndef OSP_MAX_COMM_WINDOW
#define OSP_MAX_COMM_WINDOW 4
#endif
#ifndef OSP_MAX_EMERGENCY_GROUP
#define OSP_MAX_EMERGENCY_GROUP 8
#endif
#ifndef OSP_MAX_LIMITER_CHANNELS
#define OSP_MAX_LIMITER_CHANNELS 8
#endif
#ifndef OSP_MAX_REGISTER_TABLE
#define OSP_MAX_REGISTER_TABLE 32
#endif
#ifndef OSP_MAX_FILTER_ENTRIES
#define OSP_MAX_FILTER_ENTRIES 16
#endif
#ifndef OSP_MAX_MASK_LIST
#define OSP_MAX_MASK_LIST 16
#endif
#ifndef OSP_MAX_MASK_REGISTERS
#define OSP_MAX_MASK_REGISTERS 16
#endif
#ifndef OSP_MAX_SCRIPT_ACTIONS
#define OSP_MAX_SCRIPT_ACTIONS 16
#endif
#ifndef OSP_MAX_SCRIPTS
#define OSP_MAX_SCRIPTS 32
#endif
#ifndef OSP_MAX_IP_MULTICAST
#define OSP_MAX_IP_MULTICAST 8
#endif
#ifndef OSP_MAX_IP_OPTIONS
#define OSP_MAX_IP_OPTIONS 4
#endif
#ifndef OSP_MAX_MAC_LIST
#define OSP_MAX_MAC_LIST 8
#endif
#ifndef OSP_MAX_CERTIFICATES
#define OSP_MAX_CERTIFICATES 8
#endif
#ifndef OSP_MAX_ARBITRATOR_ACTIONS
#define OSP_MAX_ARBITRATOR_ACTIONS 8
#endif
#ifndef OSP_MAX_IMAGE_TO_ACTIVATE
#define OSP_MAX_IMAGE_TO_ACTIVATE 4
#endif
#ifndef OSP_MAX_IPv6_ADDRESSES
#define OSP_MAX_IPv6_ADDRESSES 4
#endif
#ifndef OSP_MAX_IPv6_NEIGHBORS
#define OSP_MAX_IPv6_NEIGHBORS 4
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 *  ENUMERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
	OSP_ACCESS_NONE = 0,
	OSP_ACCESS_READ_ONLY = 1,
	OSP_ACCESS_WRITE_ONLY = 2,
	OSP_ACCESS_READ_WRITE = 3,
	OSP_ACCESS_AUTH_READ_ONLY = 4,
	OSP_ACCESS_AUTH_WRITE_ONLY = 5,
	OSP_ACCESS_AUTH_READ_WRITE = 6,
} osp_attr_access_t;

typedef enum {
	OSP_METHOD_NO_ACCESS = 0,
	OSP_METHOD_ACCESS = 1,
	OSP_METHOD_AUTHENTICATED_ACCESS = 2,
} osp_method_access_t;

typedef enum {
	OSP_SORT_FIFO = 0,
	OSP_SORT_LIFO = 1,
	OSP_SORT_LARGEST = 2,
	OSP_SORT_SMALLEST = 3,
	OSP_SORT_TIME_LATEST = 4,
	OSP_SORT_TIME_EARLIEST = 5,
} osp_sort_method_t;

typedef enum {
	OSP_RESTRICTION_NONE = 0,
	OSP_RESTRICTION_BY_DATE = 1,
	OSP_RESTRICTION_BY_ENTRY = 2,
} osp_restriction_type;

typedef enum {
	OSP_SELECTOR_NONE = 0,
	OSP_SELECTOR_RANGE = 1,
} osp_access_selector_type;

/* ═══════════════════════════════════════════════════════════════════════════
 *  COMMON TYPES (used by multiple IC classes)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── Access descriptors ──────────────────────────────────────────────────── */

typedef struct {
	osp_access_selector_type type;
	int32_t values[8];
	uint8_t count;
} osp_access_selectors_t;

typedef struct {
	int8_t attribute_id;
	osp_attr_access_t access_mode;
	osp_access_selectors_t access_selectors;
} osp_attribute_access_item_t;

typedef struct {
	int8_t method_id;
	osp_method_access_t access_mode;
} osp_method_access_item_t;

typedef struct {
	osp_attribute_access_item_t attr_items[OSP_MAX_ACCESS_ITEMS];
	uint8_t attr_count;
	osp_method_access_item_t method_items[OSP_MAX_METHOD_ITEMS];
	uint8_t method_count;
} osp_access_right_t;

/* ── Scaler unit ─────────────────────────────────────────────────────────── */
/* scal_unit_type ::= structure { scaler: integer, unit: enum } */

typedef struct {
	int8_t scaler;
	uint8_t unit;
} osp_scaler_unit_t;

/* ── Value definition (Limiter, Register Monitor) ────────────────────────── */
/* value_definition ::= structure { class_id, logical_name, attribute_index } */

typedef struct {
	uint16_t class_id;
	osp_obis_t logical_name;
	int8_t attribute_index;
} osp_value_definition_t;

/* ── Data access result ──────────────────────────────────────────────────── */

typedef enum {
	OSP_DATA_ACCESS_SUCCESS = 0,
	OSP_DATA_ACCESS_HARDWARE_FAULT = 1,
	OSP_DATA_ACCESS_TEMPORARY_FAILURE = 2,
	OSP_DATA_ACCESS_READ_DENIED = 3,
	OSP_DATA_ACCESS_OBJECT_UNDEFINED = 4,
	OSP_DATA_ACCESS_OBJECT_CLASS_UNKNOWN = 5,
	OSP_DATA_ACCESS_OBJECT_UNAVAILABLE = 6,
	OSP_DATA_ACCESS_TYPE_MISMATCH = 7,
	OSP_DATA_ACCESS_SCOPE_OF_ACCESS = 8,
	OSP_DATA_ACCESS_DATA_BLOCK_UNAVAILABLE = 9,
	OSP_DATA_ACCESS_LONG_GET_ABORTED = 10,
	OSP_DATA_ACCESS_NO_LONG_GET_IN_PROGRESS = 11,
	OSP_DATA_ACCESS_LONG_BLOCK_TRANSFER = 12,
	OSP_DATA_ACCESS_UNEXPECTED_ATTRIBUTE = 13,
	OSP_DATA_ACCESS_AUTHORIZATION_FAILURE = 14,
	OSP_DATA_ACCESS_HARDWARE_FAULT2 = 15,
} osp_data_access_result_t;

/* ── Selective access ────────────────────────────────────────────────────── */

typedef enum {
	OSP_SEL_ACCESS_NONE = 0,
	OSP_SEL_ACCESS_BY_ENTRY = 1,
	OSP_SEL_ACCESS_BY_DATE = 2,
	OSP_SEL_ACCESS_BY_RANGE = 3,
	OSP_SEL_ACCESS_BY_POINTER = 4,
} osp_sel_access_type;

typedef struct {
	osp_sel_access_type type;

	union {
		struct {
			uint32_t from;
			uint32_t to;
		} entry;

		struct {
			osp_date_t from;
			osp_date_t to;
		} date;

		struct {
			int32_t value;
		} single;
	} param;
} osp_selective_access_t;

/* ── Security policy bits ────────────────────────────────────────────────── */

#define OSP_SEC_POLICY_AUTH_REQUEST  (1U << 2)
#define OSP_SEC_POLICY_AUTH_RESPONSE (1U << 3)
#define OSP_SEC_POLICY_ENCR_REQUEST  (1U << 4)
#define OSP_SEC_POLICY_ENCR_RESPONSE (1U << 5)
#define OSP_SEC_POLICY_SIGN_REQUEST  (1U << 6)
#define OSP_SEC_POLICY_SIGN_RESPONSE (1U << 7)

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 1: Data — no composite types (value is osp_value_t)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 3: Register — value is CHOICE (use osp_value_t)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 4: Extended Register
 * ═══════════════════════════════════════════════════════════════════════════ */

/* capture_time is octet-string(12) datetime, value/status are CHOICE (osp_value_t) */

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 5: Demand Register
 * ═══════════════════════════════════════════════════════════════════════════ */

/* current/last_average_value: CHOICE; status: CHOICE; period: uint32 */

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 6: Register Activation
 * ═══════════════════════════════════════════════════════════════════════════ */

/* register_assignment ::= array of long-unsigned (indices into object list) */
typedef struct {
	uint32_t indices[OSP_MAX_MASK_REGISTERS];
	uint8_t count;
} osp_register_list_t;

/* activation_mask ::= structure { mask_name, mask, register_list } */
typedef struct {
	char mask_name[OSP_MAX_NAME_LEN];
	uint8_t mask_name_len;
	uint8_t mask[32]; /* bitstring: which registers are active */
	uint32_t mask_bits;
	osp_register_list_t register_list;
} osp_activation_mask_t;

typedef struct {
	osp_activation_mask_t items[OSP_MAX_MASK_LIST];
	uint8_t count;
} osp_mask_list_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 7: Profile Generic
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── Restriction element ─────────────────────────────────────────────────── */

typedef struct {
	osp_obis_t from_date;
	osp_obis_t to_date;
} osp_restriction_by_date_t;

typedef struct {
	uint32_t from_entry;
	uint32_t to_entry;
} osp_restriction_by_entry_t;

typedef struct {
	osp_restriction_type type;

	union {
		osp_restriction_by_date_t by_date;
		osp_restriction_by_entry_t by_entry;
	} value;
} osp_restriction_element_t;

/* ── Capture object definition ───────────────────────────────────────────── */

typedef struct {
	uint16_t class_id;
	osp_obis_t logical_name;
	int8_t attribute_index;
	uint32_t data_index;
	osp_restriction_element_t restriction;
} osp_capture_object_t;

typedef struct {
	osp_capture_object_t items[OSP_MAX_CAPTURE_OBJECTS];
	uint8_t count;
} osp_capture_object_list_t;

/* ── Profile buffer rows ─────────────────────────────────────────────────── */

typedef struct {
	osp_value_t cells[OSP_MAX_CAPTURE_OBJECTS];
	uint8_t cell_count;
} osp_profile_row_t;

typedef struct {
	osp_profile_row_t rows[OSP_MAX_BUFFER_ROWS];
	uint8_t row_count;
	uint32_t entries_in_use;
} osp_profile_buffer_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 8: Clock — uses octet-string for date/time (covered by osp_*_t)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 9: Script Table
 * ═══════════════════════════════════════════════════════════════════════════ */

/* script_action_item ::= structure { class_id, logical_name, method_id,
 *                                    method_param } */
typedef struct {
	uint16_t class_id;
	osp_obis_t logical_name;
	int8_t method_id;
	osp_value_t method_param; /* CHOICE: null or data */
} osp_script_action_item_t;

/* script ::= structure { script_id, actions: array of action_item } */
typedef struct {
	uint32_t script_id;
	osp_script_action_item_t actions[OSP_MAX_SCRIPT_ACTIONS];
	uint8_t action_count;
} osp_script_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 10: Schedule
 * ═══════════════════════════════════════════════════════════════════════════ */

/* schedule_entry_script ::= structure { script_id, script_selector } */
typedef struct {
	uint32_t script_id;
	int32_t script_selector;
} osp_schedule_entry_script_t;

/* schedule_entry ::= structure { start_time, end_time, scripts } */
typedef struct {
	uint8_t start_time[4]; /* HH:MM:SS:ms */
	uint8_t end_time[4];
	osp_schedule_entry_script_t scripts[OSP_MAX_SCRIPT_PER_ACTION];
	uint8_t script_count;
} osp_schedule_entry_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 11: Special Days Table
 * ═══════════════════════════════════════════════════════════════════════════ */

/* special_day ::= structure { day_id: uint32, date: octet-string(5) } */
typedef struct {
	uint32_t day_id;
	uint8_t date[5]; /* date format: year_h, year_l, month, day, dow */
} osp_special_day_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 15: Association LN
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── Object list element ─────────────────────────────────────────────────── */

typedef struct {
	uint16_t class_id;
	uint8_t version;
	osp_obis_t logical_name;
	osp_access_right_t access_rights;
} osp_object_list_element_t;

typedef struct {
	osp_object_list_element_t elements[OSP_MAX_OBJECT_LIST];
	uint16_t count;
} osp_object_list_t;

/* ── Associated partners ─────────────────────────────────────────────────── */

typedef struct {
	int8_t client_sap;
	uint16_t server_sap;
} osp_associated_partners_t;

/* ── Application context name (CHOICE) ───────────────────────────────────── */

typedef struct {
	uint8_t joint_iso_ctt;
	uint8_t country;
	uint16_t country_name;
	uint8_t identified_organization;
	uint8_t dlms_ua;
	uint8_t application_context;
	uint8_t context_id;
} osp_context_name_structure_t;

typedef struct {
	bool is_structure; /* true = structure, false = octet-string OID */

	union {
		osp_context_name_structure_t structure;

		struct {
			uint8_t data[10];
			uint8_t len;
		} oid;
	} as;
} osp_context_name_t;

/* ── xDLMS context info ─────────────────────────────────────────────────── */

typedef struct {
	uint32_t conformance;
	uint16_t max_receive_pdu_size;
	uint16_t max_send_pdu_size;
	uint8_t dlms_version_number;
	int8_t quality_of_service;
	uint8_t cyphering_info[64];
	uint32_t cyphering_info_len;
} osp_xdms_context_t;

/* ── User list (Association LN v2) ───────────────────────────────────────── */

typedef struct {
	int8_t id;
	char name[OSP_MAX_NAME_LEN];
	uint8_t name_len;
} osp_user_list_item_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 17: SAP Assignment
 * ═══════════════════════════════════════════════════════════════════════════ */

/* channel_list ::= array of structure { sap_id: integer, channel_id: integer } */
typedef struct {
	int32_t sap_id;
	int32_t channel_id;
} osp_sap_channel_item_t;

typedef struct {
	osp_sap_channel_item_t items[32];
	uint8_t count;
} osp_channel_list_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 18: Image Transfer
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
	OSP_IMAGE_IDLE = 0,
	OSP_IMAGE_TRANSFER_INITIATED = 1,
	OSP_IMAGE_TRANSFER_RUNNING = 2,
	OSP_IMAGE_VERIFICATION_OK = 3,
	OSP_IMAGE_VERIFICATION_ERR = 4,
	OSP_IMAGE_ACTIVATION_OK = 5,
	OSP_IMAGE_ACTIVATION_ERR = 6,
} osp_image_transfer_status_t;

/* image_to_activate_info element */
typedef struct {
	uint8_t signature[64];
	uint32_t signature_len;
	uint32_t size;
	uint8_t date[5]; /* octet-string(5) */
	uint32_t actual;
} osp_image_info_t;

/* image_transfer_init param */
typedef struct {
	uint8_t image_identifier[64];
	uint32_t image_identifier_len;
	uint32_t image_size;
} osp_image_init_t;

/* image_block_transfer param */
typedef struct {
	uint32_t block_number;
	uint8_t block_value[OSP_MAX_OCTET_LEN];
	uint32_t block_value_len;
} osp_image_block_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 20: Activity Calendar
 * ═══════════════════════════════════════════════════════════════════════════ */

/* day_profile_action_script ::= structure { script_id, script_selector } */
typedef struct {
	uint32_t script_id;
	int32_t script_selector;
} osp_day_profile_action_script_t;

/* day_profile_action ::= structure { time, scripts } */
typedef struct {
	osp_day_profile_action_script_t scripts[OSP_MAX_SCRIPT_PER_ACTION];
	uint8_t script_count;
	uint8_t time[4];
} osp_day_profile_action_t;

/* day_profile ::= structure { name, day_profile_table } */
typedef struct {
	char name[OSP_MAX_NAME_LEN];
	uint8_t name_len;
	osp_day_profile_action_t actions[OSP_MAX_DAY_ACTION];
	uint8_t action_count;
} osp_day_profile_t;

/* week_profile ::= structure { name, monday..sunday } */
typedef struct {
	char name[OSP_MAX_NAME_LEN];
	uint8_t name_len;
	uint8_t day_names[7][OSP_MAX_NAME_LEN];
	uint8_t day_name_lens[7];
} osp_week_profile_t;

/* season ::= structure { name, start, week_name } */
typedef struct {
	char name[OSP_MAX_NAME_LEN];
	uint8_t name_len;
	osp_obis_t start;
	char week_name[OSP_MAX_NAME_LEN];
	uint8_t week_name_len;
} osp_season_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 21: Register Monitor
 * ═══════════════════════════════════════════════════════════════════════════ */

/* register_monitor_threshold ::= structure { value: CHOICE, severity: enum } */
typedef enum {
	OSP_SEVERITY_NORMAL = 0,
	OSP_SEVERITY_WARNING = 1,
	OSP_SEVERITY_DISTURBANCE = 2,
	OSP_SEVERITY_DANGER = 3,
	OSP_SEVERITY_ELECTRICITY_OFF = 4,
	OSP_SEVERITY_MONITOR_ACTUAL = 5,
	OSP_SEVERITY_MONITOR_TREND = 6,
} osp_severity_t;

typedef struct {
	osp_value_t value;
	osp_severity_t severity;
} osp_register_monitor_threshold_t;

typedef struct {
	osp_register_monitor_threshold_t thresholds[16];
	uint8_t count;
} osp_threshold_list_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 22: Single Action Schedule — simple types only
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 23: IEC HDLC Setup — mostly simple types + speed enum
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
	OSP_HDLC_SPEED_300 = 0,
	OSP_HDLC_SPEED_600 = 1,
	OSP_HDLC_SPEED_1200 = 2,
	OSP_HDLC_SPEED_2400 = 3,
	OSP_HDLC_SPEED_4800 = 4,
	OSP_HDLC_SPEED_9600 = 5,
	OSP_HDLC_SPEED_19200 = 6,
	OSP_HDLC_SPEED_38400 = 7,
	OSP_HDLC_SPEED_57600 = 8,
	OSP_HDLC_SPEED_115200 = 9,
} osp_hdlc_speed_t;

typedef enum {
	OSP_HDLC_MODE_NRM = 0,
	OSP_HDLC_MODE_ARM = 1,
	OSP_HDLC_MODE_ABM = 2,
} osp_hdlc_mode_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 26: Utility Tables — simple types only
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 30: Data Protection
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
	OSP_DATA_PROTECT_NONE = 0,
	OSP_DATA_PROTECT_ENCRYPT = 1,
	OSP_DATA_PROTECT_AUTHENTICATE = 2,
	OSP_DATA_PROTECT_ENCRYPT_AUTH = 3,
} osp_data_protection_method_t;

typedef struct {
	osp_data_protection_method_t method;
	uint8_t global_key_list_id;
} osp_data_protection_entry_t;

typedef struct {
	osp_data_protection_entry_t entries[8];
	uint8_t count;
} osp_data_protection_list_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 31: Profile Filter
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
	OSP_FILTER_BY_CLASS_ID = 0,
	OSP_FILTER_BY_OBIS = 1,
	OSP_FILTER_BY_PROPERTY = 2,
	OSP_FILTER_BY_ATTRIBUTE = 3,
} osp_filter_type_t;

typedef struct {
	osp_filter_type_t filter_type;

	union {
		uint16_t class_id;
		osp_obis_t obis;
		int32_t property_index;
		int32_t attribute_index;
	} filter_value;

	osp_value_t condition_value; /* value to compare against */
	osp_value_t selected_values[8];
	uint8_t selected_count;
} osp_profile_filter_entry_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 40: Push Setup
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Same shape as capture_object_definition (class_id, LN, attr, data_index). */
typedef struct {
	uint16_t class_id;
	osp_obis_t logical_name;
	int8_t attribute_index;
	uint16_t data_index;
} osp_push_object_t;

typedef enum {
	OSP_PUSH_TRANSPORT_TCP = 0,
	OSP_PUSH_TRANSPORT_UDP = 1,
	OSP_PUSH_TRANSPORT_SMS = 2,
	OSP_PUSH_TRANSPORT_HDLC = 3,
	OSP_PUSH_TRANSPORT_M_BUS = 4,
	OSP_PUSH_TRANSPORT_ZIGBEE = 5,
} osp_push_transport_t;

typedef enum {
	OSP_PUSH_MSG_DATA_NOTIFICATION = 0,
	OSP_PUSH_MSG_UNCONFIRMED_WRITE = 1,
	OSP_PUSH_MSG_GENERAL_GLO_CIPHERING = 2,
} osp_push_message_t;

typedef struct {
	osp_push_transport_t transport_service;
	uint8_t destination[128];
	uint32_t destination_len;
	osp_push_message_t message;
} osp_send_destination_t;

/* window_element ::= structure { start_time, end_time } as date-time octet-strings */
typedef struct {
	uint8_t start[OSP_COSEM_DATETIME_LEN];
	uint8_t end[OSP_COSEM_DATETIME_LEN];
} osp_comm_window_t;

/* Push Setup v2 repetition_delay */
typedef struct {
	uint16_t repetition_delay_min;
	uint16_t repetition_delay_exponent;
	uint16_t repetition_delay_max;
} osp_repetition_delay_t;

/* confirmation_parameters ::= structure { confirmation_start_date: date-time, confirmation_interval } */
typedef struct {
	osp_datetime_t confirmation_start_date;
	uint32_t confirmation_interval;
} osp_confirmation_parameters_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 41: TCP/UDP Setup
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
	uint8_t ip_address[4];
} osp_ip4_addr_t;

/* ip_option ::= structure { length, value } */
typedef struct {
	uint8_t length;
	uint8_t value[40];
} osp_ip_option_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 42: IPv4 Setup
 * ═══════════════════════════════════════════════════════════════════════════ */

/* All attributes are simple types (uint32 IP, boolean, arrays of uint32) */

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 43: MAC Address Setup
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 45: GPRS Modem Setup — mostly simple types
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 47: GSM Diagnostic
 * ═══════════════════════════════════════════════════════════════════════════ */

/* neighbour_cell ::= structure { cell_id, location_area_code, rx_level } */
typedef struct {
	int32_t cell_id;
	int32_t location_area_code;
	int8_t rx_level;
} osp_neighbour_cell_t;

typedef struct {
	osp_neighbour_cell_t cells[16];
	uint8_t count;
} osp_neighbour_cell_list_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 48: IPv6 Setup
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ipv6_address ::= structure { address: octet-string(16), prefix_length: unsigned,
 *                               life_time: double-long-unsigned } */
typedef struct {
	uint8_t address[16];
	uint8_t prefix_length;
	uint32_t life_time;
} osp_ipv6_address_t;

typedef struct {
	osp_ipv6_address_t items[OSP_MAX_IPv6_ADDRESSES];
	uint8_t count;
} osp_ipv6_address_list_t;

/* neighbour_discovery ::= structure { ip_address, physical_address, lifetime } */
typedef struct {
	osp_ipv6_address_t ip_address;
	uint8_t physical_address[6];
	uint32_t lifetime;
} osp_ipv6_neighbor_t;

typedef struct {
	osp_ipv6_neighbor_t items[OSP_MAX_IPv6_NEIGHBORS];
	uint8_t count;
} osp_ipv6_neighbor_list_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 61: Register Table
 * ═══════════════════════════════════════════════════════════════════════════ */

/* table_cell_definition ::= structure { class_id, logical_name,
 *   group_E_values, attribute_index } */
typedef struct {
	uint16_t class_id;
	osp_obis_t logical_name;
	uint8_t group_e_values[8];
	uint8_t group_e_count;
	int8_t attribute_index;
} osp_table_cell_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 62: Compact Data — uses template_description (octet-string)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 63: Status Mapping — array of status octet-strings
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 64: Security Setup
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
	osp_user_list_item_t items[16];
	uint8_t count;
} osp_security_user_list_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 68: Arbitrator
 * ═══════════════════════════════════════════════════════════════════════════ */

/* actions ::= array of bitstring (each bitstring is an action group) */
typedef struct {
	uint8_t groups[OSP_MAX_ARBITRATOR_ACTIONS][32]; /* bitstrings */
	uint8_t group_count;
	uint8_t group_bits[OSP_MAX_ARBITRATOR_ACTIONS]; /* actual bits per group */
} osp_arbitrator_actions_t;

/* permissions_table, weightings_table, most_recent_requests_table:
   all are arrays of bitstrings (same shape as actions) */

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 70: Disconnect Control
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
	OSP_DISCO_OUTPUT_STATE_DISCONNECT = 0,
	OSP_DISCO_OUTPUT_STATE_RECONNECT = 1,
	OSP_DISCO_OUTPUT_STATE_RECONNECT_IF_AUTOCLOSE = 2,
	OSP_DISCO_OUTPUT_STATE_RECONNECT_IF_CLOSE = 3,
} osp_output_state_t;

typedef enum {
	OSP_DISCO_CONTROL_MANUAL = 0,
	OSP_DISCO_CONTROL_REMOTE = 1,
	OSP_DISCO_CONTROL_AUTOMATIC = 2,
} osp_control_model_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 71: Limiter
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Legacy compound threshold (unused by Limiter IC — threshold is CHOICE). */
typedef struct {
	osp_value_t normal_value;
	osp_value_t threshold_value;
	uint32_t min_duration;
	uint32_t max_duration;
	uint32_t emergency_duration;
} osp_threshold_t;

/* emergency_profile ::= structure {
 *   emergency_profile_id: long-unsigned,
 *   emergency_activation_time: octet-string (date-time),
 *   emergency_duration: double-long-unsigned } */
typedef struct {
	uint16_t emergency_profile_id;
	uint8_t emergency_activation_time[OSP_COSEM_DATETIME_LEN];
	uint32_t emergency_duration;
} osp_emergency_profile_t;

/* action_item ::= structure { script_logical_name, script_selector } */
typedef struct {
	osp_obis_t script_logical_name;
	uint16_t script_selector;
} osp_action_item_t;

/* action ::= structure { action_over_threshold, action_under_threshold } */
typedef struct {
	osp_action_item_t action_over_threshold;
	osp_action_item_t action_under_threshold;
} osp_limiter_action_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  CLASS 76: MBus Slave Port Setup — mostly simple types
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════════════
 *  SERVICE DESCRIPTORS (IEC 62056-5-3)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
	uint16_t class_id;
	osp_obis_t instance_id;
	int8_t attribute_id;
} osp_attribute_descriptor_t;

typedef struct {
	uint16_t class_id;
	osp_obis_t instance_id;
	int8_t method_id;
} osp_method_descriptor_t;

typedef struct {
	bool last_block;
	uint32_t block_number;
	uint8_t raw_data[OSP_MAX_OCTET_LEN];
	uint32_t raw_data_len;
} osp_data_block_t;

#ifdef __cplusplus
}
#endif

#endif /* OSP_STRUCTURES_H */
