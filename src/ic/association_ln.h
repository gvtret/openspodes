/**
 * @file association_ln.h
 * @brief Association LN interface class (class_id = 15, IEC 62056-6-2)
 *
 * @par Attributes
 * | # | Name                    | Type                    | Access |
 * |---|-------------------------|-------------------------|--------|
 * | 1 | logical_name            | octet-string            | RO     |
 * | 2 | object_list             | array                   | RW     |
 * | 3 | associated_partners_id  | structure               | RO     |
 * | 4 | application_context_name| OID                     | RW     |
 * | 5 | xDLMS_context_info      | structure               | RW     |
 * | 6 | authentication_mechanism_name | OID              | RW     |
 * | 7 | closing_info            | structure               | RO     |
 * | 8 | client_SAP              | unsigned                | RO     |
 * | 9 | server_SAP              | unsigned                | RO     |
 * | 10| security_options        | bitstring               | RW     |
 * | 11| user_information        | octet-string            | RW     |
 *
 * @par Methods
 * | # | Name    | Argument | Description                                      |
 * |---|---------|----------|--------------------------------------------------|
 * | 1 | reset   | data     | Resets the association and clears all context     |
 */
#ifndef OSP_IC_ASSOCIATION_LN_H
#define OSP_IC_ASSOCIATION_LN_H
#include "../openspodes.h"
#include "../codec/structures.h"
#ifndef OSP_MAX_OBJECT_LIST_ENTRIES
#define OSP_MAX_OBJECT_LIST_ENTRIES OSP_MAX_OBJECT_LIST
#endif

/**
 * @brief Association LN object structure
 *
 * Manages DLMS/COSEM associations, including object lists,
 * authentication, and access rights for connected clients.
 */
typedef struct {
	osp_obis_t logical_name;                /**< OBIS logical name */
	osp_object_list_t object_list;          /**< List of accessible objects */
	osp_associated_partners_t associated_partners; /**< Associated partner IDs */
	osp_context_name_t app_context_name;    /**< Application context name (OID) */
	osp_xdms_context_t xdms_context;        /**< xDLMS context information */
	uint8_t authentication_mechanism;       /**< Authentication mechanism type */
	uint8_t secret[64];                     /**< Authentication secret */
	uint8_t secret_len;                     /**< Length of authentication secret */
	uint8_t association_status;             /**< Association status: 0=idle, 1=associated, 2=pending */
	osp_obis_t security_setup_reference;    /**< Reference to security setup object */
	osp_user_list_item_t user_list[16];     /**< List of authorized users */
	uint8_t user_count;                     /**< Number of users in the list */
	osp_user_list_item_t current_user;      /**< Currently authenticated user */
} osp_ic_association_ln_t;

/**
 * @brief Initialize an Association LN IC object
 * @param a  Pointer to association object to initialize
 * @param ln OBIS logical name
 */
void osp_ic_association_ln_init(osp_ic_association_ln_t *a, osp_obis_t ln);

/**
 * @brief Get the class descriptor for the Association LN IC
 * @return Pointer to the class descriptor structure
 */
const osp_ic_class_t *osp_ic_association_ln_class(void);

/** @name Object list management
 * @{
 */

/**
 * @brief Add an object to the association's object list
 * @param a    Pointer to association object
 * @param elem Object list element to add
 * @return OSP_ERR_OK on success, error code otherwise
 */
osp_err_t osp_ic_association_ln_add_object(osp_ic_association_ln_t *a, const osp_object_list_element_t *elem);

/**
 * @brief Remove an object from the association's object list
 * @param a       Pointer to association object
 * @param class_id Class ID of the object to remove
 * @param ln      OBIS logical name of the object to remove
 * @return OSP_ERR_OK on success, error code otherwise
 */
osp_err_t osp_ic_association_ln_remove_object(osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln);

/**
 * @brief Find an object in the association's object list
 * @param a       Pointer to association object
 * @param class_id Class ID to search for
 * @param ln      OBIS logical name to search for
 * @return Pointer to the found element, or NULL if not found
 */
const osp_object_list_element_t *osp_ic_association_ln_find_object(const osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln);

/**
 * @brief Check if read access is allowed for an attribute
 * @param a       Pointer to association object
 * @param class_id Class ID of the target object
 * @param ln      OBIS logical name of the target object
 * @param attr_id Attribute ID to check
 * @return true if read access is allowed, false otherwise
 */
bool osp_ic_association_ln_can_read(const osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln, int8_t attr_id);

/**
 * @brief Check if write access is allowed for an attribute
 * @param a       Pointer to association object
 * @param class_id Class ID of the target object
 * @param ln      OBIS logical name of the target object
 * @param attr_id Attribute ID to check
 * @return true if write access is allowed, false otherwise
 */
bool osp_ic_association_ln_can_write(const osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln, int8_t attr_id);

/**
 * @brief Check if method invocation is allowed
 * @param a       Pointer to association object
 * @param class_id Class ID of the target object
 * @param ln      OBIS logical name of the target object
 * @param method_id Method ID to check
 * @return true if method invocation is allowed, false otherwise
 */
bool osp_ic_association_ln_can_invoke(const osp_ic_association_ln_t *a, uint16_t class_id, const osp_obis_t *ln, int8_t method_id);

/**
 * @brief Mirror all association state from @p src into @p dst except logical_name.
 * Sets association_status to associated (1).
 */
void osp_ic_association_ln_mirror(osp_ic_association_ln_t *dst, const osp_ic_association_ln_t *src);

/**
 * @brief Reset to idle: clear object_list and other fields, keep logical_name.
 */
void osp_ic_association_ln_set_idle(osp_ic_association_ln_t *a);

/** @} */
#endif