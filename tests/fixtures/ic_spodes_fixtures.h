/**
 * ic_spodes_fixtures.h — IC test instances with normative + parity sources
 *
 * Primary normative refs (doc-rag-remote):
 *   - IEC 62056-6-2 / Blue Book §4.3.x — IC attribute tables, scaler_unit, Clock
 *   - STO 34.01-5.1-006 — date-time 12-octet layout (deviation, clock_status)
 *   - GOST R 58940-2020 — Register IC (class 3)
 * Secondary parity ref:
 *   - spodes-rs/tests/integration.rs — Rust roundtrip fixtures
 */

#ifndef OSP_IC_SPODES_FIXTURES_H
#define OSP_IC_SPODES_FIXTURES_H

#include "openspodes.h"
#include "ic/data.h"
#include "ic/register.h"
#include "ic/extended_register.h"
#include "ic/demand_register.h"
#include "ic/clock.h"
#include "ic/security_setup.h"
#include "ic/disconnect_control.h"
#include "ic/mbus_slave_port_setup.h"
#include "ic/gprs_modem.h"
#include "ic/gsm_diagnostic.h"
#include "ic/activity_calendar.h"
#include "ic/ipv4_setup.h"
#include "ic/ipv6_setup.h"
#include "ic/mac_address.h"
#include "ic/profile_generic.h"
#include "ic/limiter.h"
#include "ic/register_monitor.h"
#include "ic/sap_assignment.h"
#include "ic/single_action_schedule.h"
#include "ic/iec_hdlc_setup.h"
#include "ic/iec_local_port_setup.h"
#include "ic/tcp_udp_setup.h"
#include "ic/push_setup.h"
#include "ic/image_transfer.h"
#include "ic/arbitrator.h"
#include "ic/data_protection.h"
#include "ic/association_ln.h"
#include "ic/schedule.h"
#include "ic/script_table.h"
#include "ic/special_days.h"
#include "ic/register_activation.h"
#include "ic/compact_data.h"
#include "ic/profile_filter.h"
#include "ic/profile_data_filter.h"
#include "ic/table_manager.h"
#include "ic/register_table.h"
#include "ic/parameter_monitor.h"
#include "ic/mbus_slave.h"
#include "ic/utility_tables.h"
#include "ic/status_mapping.h"

#ifdef __cplusplus
extern "C" {
#endif

/* integration.rs: ObisCode::new(0, 0, 96, 1, 0, 255) */
#define OSP_FIXTURE_LN_DATA ((osp_obis_t){0, 0, 96, 1, 0, 255})
/* Blue Book Table 5: 593 kWh → value=593, scaler=3, unit=Wh(30); OBIS 1.0.1.8.0.255 */
#define OSP_FIXTURE_LN_ACTIVE_ENERGY_IMPORT ((osp_obis_t){1, 0, 1, 8, 0, 255})
#define OSP_FIXTURE_LN_REGISTER_ENERGY OSP_FIXTURE_LN_ACTIVE_ENERGY_IMPORT
/* integration.rs: Clock 0.0.1.0.0.255 */
#define OSP_FIXTURE_LN_CLOCK ((osp_obis_t){0, 0, 1, 0, 0, 255})
/* integration.rs: ExtendedRegister 1.0.1.8.1.255 */
#define OSP_FIXTURE_LN_EXT_REGISTER ((osp_obis_t){1, 0, 1, 8, 1, 255})
/* integration.rs: DemandRegister 1.0.1.8.2.255 */
#define OSP_FIXTURE_LN_DEMAND_REGISTER ((osp_obis_t){1, 0, 1, 8, 2, 255})
/* integration.rs: ProfileGeneric 1.0.99.1.0.255 */
#define OSP_FIXTURE_LN_PROFILE ((osp_obis_t){1, 0, 99, 1, 0, 255})
/* integration.rs: SecuritySetup 0.0.43.0.0.255 */
#define OSP_FIXTURE_LN_SECURITY ((osp_obis_t){0, 0, 43, 0, 0, 255})
/* Generic fallback */
#define OSP_FIXTURE_LN_DEFAULT ((osp_obis_t){0, 0, 1, 0, 0, 255})

/** 12-octet COSEM date-time (STO 34.01-5.1-006 §date-time, Blue Book §4.1.6.1). */
osp_cosem_datetime_t osp_fixture_cosem_datetime_bytes(const uint8_t dt[12]);

/** Legacy helper: first 9 bytes → AXDR datetime (capture_time in registers). */
osp_datetime_t osp_fixture_datetime_bytes(const uint8_t dt[12]);

/** Pack first six datetime octets as capture-time octet string (extended/demand register). */
void osp_fixture_capture_time_obis(const uint8_t dt[12], osp_obis_t *out);

void osp_fixture_data(void *inst);
void osp_fixture_register(void *inst);
/** Blue Book §4.3.2 Table 5: 593 × 10³ Wh = 593 kWh */
void osp_fixture_register_bluebook_kwh(void *inst);
void osp_fixture_extended_register(void *inst);
void osp_fixture_demand_register(void *inst);
void osp_fixture_clock(void *inst);
void osp_fixture_security_setup(void *inst);
void osp_fixture_disconnect_control(void *inst);
void osp_fixture_mbus_slave_port_setup(void *inst);
void osp_fixture_gprs_modem(void *inst);
void osp_fixture_gsm_diagnostic(void *inst);
void osp_fixture_activity_calendar(void *inst);
void osp_fixture_ipv4_setup(void *inst);
void osp_fixture_ipv6_setup(void *inst);
void osp_fixture_mac_address(void *inst);
void osp_fixture_profile_generic(void *inst);
/** spodes-rs integration.rs: buffer row {1000, 2025-05-01 00:00} */
void osp_fixture_profile_generic_spodes_buffer(void *inst);
void osp_fixture_limiter(void *inst);
void osp_fixture_register_monitor(void *inst);
void osp_fixture_sap_assignment(void *inst);
void osp_fixture_single_action_schedule(void *inst);
void osp_fixture_iec_hdlc_setup(void *inst);
void osp_fixture_iec_local_port_setup(void *inst);
void osp_fixture_tcp_udp_setup(void *inst);
void osp_fixture_push_setup(void *inst);
void osp_fixture_image_transfer(void *inst);
void osp_fixture_arbitrator(void *inst);
void osp_fixture_data_protection(void *inst);
void osp_fixture_association_ln(void *inst);
void osp_fixture_schedule(void *inst);
void osp_fixture_script_table(void *inst);
void osp_fixture_special_days(void *inst);
void osp_fixture_register_activation(void *inst);
void osp_fixture_compact_data(void *inst);
void osp_fixture_profile_filter(void *inst);
void osp_fixture_profile_data_filter(void *inst);
void osp_fixture_table_manager(void *inst);
void osp_fixture_register_table(void *inst);
void osp_fixture_parameter_monitor(void *inst);
void osp_fixture_mbus_slave(void *inst);
void osp_fixture_utility_tables(void *inst);
void osp_fixture_status_mapping(void *inst);

#ifdef __cplusplus
}
#endif

#endif /* OSP_IC_SPODES_FIXTURES_H */
