/**
 * test_data_hal.h — Simple Dataset/Datapoint database for testing Data HAL.
 *
 * Inspired by /home/trgv/comms/comms Dataset/Datapoint pattern.
 */

#ifndef TEST_DATA_HAL_H
#define TEST_DATA_HAL_H

#include "../src/openspodes.h"
#include "../src/data_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_DB_MAX_DATASETS 32
#define TEST_DB_MAX_POINTS   8
#define TEST_DB_MAX_NAME     32

/** Datapoint: maps (obis, attr_id) -> value */
typedef struct {
	osp_obis_t obis;
	uint8_t attr_id;
	osp_value_t value;
} test_dp_t;

/** Dataset: named group of datapoints */
typedef struct {
	char name[TEST_DB_MAX_NAME];
	test_dp_t points[TEST_DB_MAX_POINTS];
	uint8_t count;
} test_ds_t;

/** Simple Dataset/Datapoint database */
typedef struct {
	test_ds_t datasets[TEST_DB_MAX_DATASETS];
	uint8_t count;
} test_db_t;

/** Initialize the database */
void test_db_init(test_db_t *db);

/** Add a datapoint to the database */
void test_db_add(test_db_t *db, const char *ds_name, const osp_obis_t *obis,
                 uint8_t attr_id, osp_value_t value);

/** Read a datapoint from the database */
osp_err_t test_db_read(test_db_t *db, const osp_obis_t *obis, uint8_t attr_id,
                       osp_value_t *result);

/** Write a datapoint to the database */
osp_err_t test_db_write(test_db_t *db, const osp_obis_t *obis, uint8_t attr_id,
                        const osp_value_t *value);

/** Execute a command (stub: always returns OSP_ERR_NOT_FOUND) */
osp_err_t test_db_execute(test_db_t *db, const osp_obis_t *obis, uint8_t method_id,
                          const osp_value_t *param, osp_value_t *result);

/** Create a HAL struct from this database */
osp_hal_data_t test_db_make_hal(test_db_t *db);

#ifdef __cplusplus
}
#endif
#endif
