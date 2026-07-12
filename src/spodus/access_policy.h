/**
 * access_policy.h — Meter access policies (STO §10.6)
 */

#ifndef OSP_SPODUS_ACCESS_POLICY_H
#define OSP_SPODUS_ACCESS_POLICY_H

#include "meter_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSP_SPODUS_MAX_ACCESS_POLICIES 16
#define OSP_SPODUS_MAX_SECURITY_ITEMS  6
#define OSP_SPODUS_MAX_SECURITY_KEY_LEN 32

enum {
	OSP_SPODUS_SEC_LLS_PASSWORD = 0,
	OSP_SPODUS_SEC_LLS_AUTHENTICATION_KEY = 1,
	OSP_SPODUS_SEC_LLS_ENCRYPTION_KEY = 2,
	OSP_SPODUS_SEC_HLS_AUTHENTICATION_KEY = 3,
	OSP_SPODUS_SEC_HLS_ENCRYPTION_KEY = 4,
	OSP_SPODUS_SEC_KEK = 5,
};

typedef struct {
	uint8_t type;
	uint8_t key_len;
	uint8_t key[OSP_SPODUS_MAX_SECURITY_KEY_LEN];
} osp_spodus_security_item_t;

typedef struct {
	uint8_t meter_id_len;
	uint8_t meter_id[OSP_SPODUS_MAX_METER_ID_LEN];
	uint8_t policy_id;
	uint8_t suite_id;
	uint8_t security_count;
	osp_spodus_security_item_t security[OSP_SPODUS_MAX_SECURITY_ITEMS];
} osp_spodus_access_policy_t;

typedef struct {
	osp_spodus_access_policy_t entries[OSP_SPODUS_MAX_ACCESS_POLICIES];
	uint8_t count;
	osp_value_t value, rows[OSP_SPODUS_MAX_ACCESS_POLICIES], fields[OSP_SPODUS_MAX_ACCESS_POLICIES][4];
	osp_value_t security_arrays[OSP_SPODUS_MAX_ACCESS_POLICIES];
	osp_value_t security_rows[OSP_SPODUS_MAX_ACCESS_POLICIES][OSP_SPODUS_MAX_SECURITY_ITEMS];
	osp_value_t security_fields[OSP_SPODUS_MAX_ACCESS_POLICIES][OSP_SPODUS_MAX_SECURITY_ITEMS][2];
} osp_spodus_access_policies_t;

void osp_spodus_access_policies_init(osp_spodus_access_policies_t *policies);
osp_err_t osp_spodus_access_policies_add(osp_spodus_access_policies_t *policies, const osp_spodus_access_policy_t *policy);
osp_err_t osp_spodus_access_policies_build_value(const osp_spodus_access_policies_t *policies, osp_value_t *out);

#ifdef __cplusplus
}
#endif

#endif
