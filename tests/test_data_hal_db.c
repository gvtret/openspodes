/**
 * test_data_hal_db.c — Simple Dataset/Datapoint database implementation for tests.
 */

#include "test_data_hal.h"
#include <string.h>

void test_db_init(test_db_t *db) {
	memset(db, 0, sizeof(*db));
}

static test_ds_t *find_ds(test_db_t *db, const char *name) {
	for (uint8_t i = 0; i < db->count; i++) {
		if (strcmp(db->datasets[i].name, name) == 0)
			return &db->datasets[i];
	}
	return NULL;
}

static test_ds_t *add_ds(test_db_t *db, const char *name) {
	if (db->count >= TEST_DB_MAX_DATASETS)
		return NULL;
	test_ds_t *ds = &db->datasets[db->count++];
	strncpy(ds->name, name, TEST_DB_MAX_NAME - 1);
	ds->name[TEST_DB_MAX_NAME - 1] = '\0';
	ds->count = 0;
	return ds;
}

void test_db_add(test_db_t *db, const char *ds_name, const osp_obis_t *obis,
                 uint8_t attr_id, osp_value_t value) {
	test_ds_t *ds = find_ds(db, ds_name);
	if (!ds) {
		ds = add_ds(db, ds_name);
		if (!ds)
			return;
	}
	if (ds->count >= TEST_DB_MAX_POINTS)
		return;
	test_dp_t *dp = &ds->points[ds->count++];
	dp->obis = *obis;
	dp->attr_id = attr_id;
	dp->value = value;
}

osp_err_t test_db_read(test_db_t *db, const osp_obis_t *obis, uint8_t attr_id,
                       osp_value_t *result) {
	if (!db || !obis || !result)
		return OSP_ERR_INVALID;
	for (uint8_t i = 0; i < db->count; i++) {
		test_ds_t *ds = &db->datasets[i];
		for (uint8_t j = 0; j < ds->count; j++) {
			test_dp_t *dp = &ds->points[j];
			if (osp_obis_eq(&dp->obis, obis) && dp->attr_id == attr_id) {
				*result = dp->value;
				return OSP_OK;
			}
		}
	}
	return OSP_ERR_NOT_FOUND;
}

osp_err_t test_db_write(test_db_t *db, const osp_obis_t *obis, uint8_t attr_id,
                        const osp_value_t *value) {
	if (!db || !obis || !value)
		return OSP_ERR_INVALID;
	for (uint8_t i = 0; i < db->count; i++) {
		test_ds_t *ds = &db->datasets[i];
		for (uint8_t j = 0; j < ds->count; j++) {
			test_dp_t *dp = &ds->points[j];
			if (osp_obis_eq(&dp->obis, obis) && dp->attr_id == attr_id) {
				dp->value = *value;
				return OSP_OK;
			}
		}
	}
	return OSP_ERR_NOT_FOUND;
}

osp_err_t test_db_execute(test_db_t *db, const osp_obis_t *obis, uint8_t method_id,
                          const osp_value_t *param, osp_value_t *result) {
	(void)db;
	(void)obis;
	(void)method_id;
	(void)param;
	(void)result;
	return OSP_ERR_NOT_FOUND;
}

/* Static callbacks for the HAL */
static osp_err_t hal_read(void *ctx, const osp_obis_t *obis, uint8_t attr_id,
                          osp_value_t *result) {
	return test_db_read((test_db_t *)ctx, obis, attr_id, result);
}

static osp_err_t hal_write(void *ctx, const osp_obis_t *obis, uint8_t attr_id,
                           const osp_value_t *value) {
	return test_db_write((test_db_t *)ctx, obis, attr_id, value);
}

static osp_err_t hal_execute(void *ctx, const osp_obis_t *obis, uint8_t method_id,
                             const osp_value_t *param, osp_value_t *result) {
	return test_db_execute((test_db_t *)ctx, obis, method_id, param, result);
}

osp_hal_data_t test_db_make_hal(test_db_t *db) {
	osp_hal_data_t hal;
	hal.read = hal_read;
	hal.write = hal_write;
	hal.execute = hal_execute;
	hal.ctx = db;
	return hal;
}
