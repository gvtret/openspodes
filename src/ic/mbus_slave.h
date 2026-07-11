#ifndef OSP_IC_MBUS_SLAVE_H
#define OSP_IC_MBUS_SLAVE_H
#include "../openspodes.h"
#include "../codec/structures.h"

/**
 * @file mbus_slave.h
 * @brief MBus Slave Port Setup interface class (class_id = 76, IEC 62056-6-2)
 *
 * @par Attributes
 * | # | Name             | Type              | Access |
 * |---|------------------|-------------------|--------|
 * | 1 | logical_name     | octet-string      | RO     |
 * | 2 | physical_address | long-unsigned     | RW     |
 * | 3 | logical_address  | long-unsigned     | RW     |
 * | 4 | id_number        | octet-string      | RO     |
 * | 5 | manufacturer     | octet-string      | RO     |
 * | 6 | version          | unsigned          | RO     |
 * | 7 | medium           | unsigned          | RO     |
 *
 * @par Methods
 * None.
 */

#define OSP_MAX_MBUS_ID_LEN 16
#define OSP_MAX_MBUS_MFR_LEN 16

typedef struct {
	osp_obis_t logical_name;
	uint16_t physical_address;
	uint16_t logical_address;
	uint8_t id_number[OSP_MAX_MBUS_ID_LEN];
	uint8_t id_number_len;
	uint8_t manufacturer[OSP_MAX_MBUS_MFR_LEN];
	uint8_t manufacturer_len;
	uint8_t version;
	uint8_t medium;
} osp_ic_mbus_slave_t;

void osp_ic_mbus_slave_init(osp_ic_mbus_slave_t *i, osp_obis_t ln);
const osp_ic_class_t *osp_ic_mbus_slave_class(void);
#endif
