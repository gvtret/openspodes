# HANDOFF — OpenSPODES

## 2026-07-06 22:10 — IC classes complete, all 38 implemented

**Done:**
- Created 38 IC classes in `src/ic/` (38 .c + 38 .h files), all following vtable pattern from spodes-rs
- All 38 classes have init + vtable (get_attr/set_attr/invoke where applicable)
- Association LN (15) has full object_list management (add/remove/find/can_read/can_write/can_invoke)
- Committed: 13 commits on `main` branch (6210f3d is HEAD)
- 22/22 CMocka tests pass, clang-tidy clean
- Total: 7509 lines C11, 92 files

**State:**
- Branch: `main`, clean working tree
- All modules build and pass: codec, transport, service, security, server, ic
- 38 IC classes: Data(1), Register(3), ExtRegister(4), DemandRegister(5), RegisterActivation(6), ProfileGeneric(7), Clock(8), ScriptTable(9), Schedule(10), SpecialDays(11), AssociationLN(15), SAPAssignment(17), ImageTransfer(18), IEC_HDLCSetup(23), UtilityTables(26), DataProtection(30), ProfileFilter(31), PushSetup(40), IPv4Setup(42), MACAddress(43), IPv6Setup(48), RegisterTable(61), CompactData(62), StatusMapping(63), SecuritySetup(64), ParameterMonitor(65), Arbitrator(68), DisconnectControl(70), Limiter(71), ActivityCalendar(20), RegisterMonitor(21), SingleActionSchedule(22), IEC_LocalPortSetup(19), GPRSModemSetup(45), GSMDiagnostic(47), MBusSlaveSetup(76), TableManager(8200)

**Next:**
1. Client/Server drivers — `osp_client_connect()` (AARQ→HLS→associate) and server main loop (accept→dispatch)
2. More IC class tests — currently only smoke tests for Data/Register/Clock/ExtRegister
3. Integration tests — AARQ→HLS→GET end-to-end through mock HAL
4. Replace security stubs with actual AES-GCM via HAL

**Notes:**
- Reference repo: `/home/trgv/spodes-rs` (Rust implementation, local)
- C11, clang-format (LLVM tabs from opendlms), clang-tidy clean
- HAL: function pointers, global crypto for GCM, MCU-safe no-malloc core
- IC vtable pattern: first field must be `osp_obis_t logical_name` (dispatcher matches on this)
- Commit convention: no `Co-Authored-By: Claude` trailers (guard hook)
- Security: HLS GMAC handshake implemented, glo-ciphering is stub only (needs real GCM HAL)
- `osp_hal_gcm_init/update/finish` are global function pointers — application sets them at startup
