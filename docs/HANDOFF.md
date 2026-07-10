# HANDOFF — OpenSPODES (full context for new session)

## Repository
- **Path**: `E:/work/opendlms/openspodes`
- **Branch**: main
- **Reference**: spodes-rs (Rust implementation)
- **Language**: C11, clang-format (LLVM tabs)
- **Stats**: ~100 files, 40 IC classes, 10 CTest suites, ~210 unit tests

## Project: OpenSPODES — DLMS/COSEM protocol stack in C11
Portable C11 implementation of IEC 62056 DLMS/COSEM, modeled after spodes-rs.
No malloc in core. HAL via function pointers. MCU-pluggable.

## Architecture
```
src/codec/     BER/AXDR read/write, serialize/deserialize, compact-array
src/transport/ HDLC (IEC 62056-46) + COSEM wrapper (IEC 62056-47)
src/service/   ACSE + xDLMS + notifications + GBT codec
src/security/  HLS GMAC/MD5/SHA1/SHA256 (pass 3/4) + glo-ciphering + replay (GMAC IC)
src/server/    RequestDispatcher (class_id+OBIS), osp_server_accept/run
src/client/    connect/get/set/action + with-list + block transfer + recv notification
src/ic/        40 IC classes, vtable pattern
tests/         CMocka unit/golden/error/ic/integration/phase0-2 + mock transport/crypto
scripts/       coverage_report.py (gcov summary)
```

## Build & Test

```bash
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
```

### Test suites — all PASS (10/10)

| Target | File | Tests | Purpose |
|--------|------|-------|---------|
| `openspodes_test` | `tests/test_core.c` | ~27 | Core codec, transport, IC, dispatcher |
| `openspodes_test_golden` | `tests/test_codec_golden.c` | ~104 | Golden vectors, BER/ACSE/xDLMS, thirdparty cross-check |
| `openspodes_test_errors` | `tests/test_errors.c` | 34 | Error paths |
| `openspodes_test_ic` | `tests/test_ic_smoke.c` | 3 | All IC classes smoke |
| `openspodes_test_integration` | `tests/test_integration.c` | 21 | Client↔server E2E loopback (+ HLS/GBT) |
| `openspodes_test_phase0` | `tests/test_phase0.c` | 7 | SPODUS helpers |
| `openspodes_test_phase1` | `tests/test_phase1.c` | 8 | Table manager / profile filter |
| `openspodes_test_phase2` | `tests/test_phase2.c` | 9 | WithList codec, blocks, GBT confirmed |
| `openspodes_test_security` | `tests/test_security_glo.c` | 2 | glo-ciphering E.5 + roundtrip (OpenSSL) |
| `openspodes_loopback_cli` | `examples/loopback_cli.c` | demo | In-process GET/SET demo (CTest) |

## Recent accomplishments (Phase 2)

- Unified BER length codec (`osp_dlms_*`)
- Client SET/ACTION block transfer; ACTION return param_block reassembly
- `param_block` rename (was `pblock`)
- Compact-array encode/decode + IC 62 Compact Data
- Dispatcher capture() via `osp_ic_compact_data_bind_dispatcher`
- Push Setup → data-notification E2E (`osp_client_recv_data_notification`)
- Golden vectors cross-check vs `thirdparty/dlms-codec`
- **Client with-list API**: `osp_client_get/set/action_with_list`
- **GBT runtime E2E**: `osp_server_enable_gbt` / `osp_client_enable_gbt`, transport send/recv
- **glo-ciphering**: `osp_glo_protect/unprotect`, `osp_client_set_ciphering`, E.5 vector test
- **HLS MD5/SHA1/SHA256**: `osp_hls_pass3/4_*`, client/server handshake, OpenSSL hash HAL
- **GBT confirmed mode**: `osp_*_set_gbt_window`, ack between windows, loopback E2E
- **Example CLI**: `openspodes_loopback_cli` — in-process GET/SET demo

## Client API

| Function | Status |
|----------|--------|
| `osp_client_connect` / `release` / `disconnect` | ✅ HLS GMAC + MD5/SHA1/SHA256 |
| `osp_client_get` | ✅ + block reassembly |
| `osp_client_get_with_list` | ✅ |
| `osp_client_set` | ✅ + auto block transfer |
| `osp_client_set_with_list` | ✅ |
| `osp_client_action` | ✅ + param/return blocks |
| `osp_client_action_with_list` | ✅ |
| `osp_client_recv_data_notification` | ✅ |
| GBT runtime (xDLMS only) | ✅ unconfirmed + confirmed (window>0) |
| glo-ciphering | ✅ protect/unprotect + client/server session |
| HLS MD5/SHA1/SHA256 | ✅ pass 3/4 + E2E loopback |
| GBT confirmed (window>0) | ✅ ack + E2E loopback |
| HLS GOST (8–10) | ❌ |

## IC Classes (40 implemented)
Data(1) Register(3) ExtRegister(4) DemandRegister(5) RegisterActivation(6)
ProfileGeneric(7) Clock(8) ScriptTable(9) Schedule(10) SpecialDays(11)
AssociationLN(15) SAPAssignment(17) ImageTransfer(18) ActivityCalendar(20)
RegisterMonitor(21) SingleActionSchedule(22) IEC_HDLCSetup(23) UtilityTables(26 stub)
DataProtection(30) ProfileFilter(31 stub) PushSetup(40) IPv4Setup(42) MACAddress(43)
GPRSModemSetup(45) GSMDiagnostic(47) IPv6Setup(48) RegisterTable(61 stub)
CompactData(62) StatusMapping(63 stub) SecuritySetup(64) ParameterMonitor(65 stub)
Arbitrator(68) DisconnectControl(70) Limiter(71) MBusSlaveSetup(76 stub)
TableManager(8200) ProfileDataFilter(8201)

## Known gaps

### Protocol / implementation
- GBT streaming / lost-block recovery not implemented
- `GET_WITH_LIST_BLOCK` enum only (no codec)
- HLS GOST mechanisms 8–10 not implemented
- Selective access stubbed (encode writes 0)
- Event notification send not implemented
- Confirmed service error not implemented
- СПОДУС Concentrator / MeterProxy not implemented

### vs spodes-rs
- OpenSPODES ahead: ACTION blocks, compact-array, push E2E, client block transfer, IC 62
- spodes-rs ahead: GOST security, glo-ciphering, general-ciphering, Concentrator runtime, examples

## Next steps (suggested)
1. GOST mechanisms 8–10 (Kuznyechik/Streebog HAL)
2. GBT streaming / lost-block recovery
3. TCP wrapper example (like spodes-rs tcp_client/tcp_server)

## User Instructions (MUST follow)
- **Consult doc-rag-remote when implementing features**
- **BER length**: 1 byte if < 128, 2 bytes (`0x81`+byte) if 128–65535
- **COSEMpdu_GB83.asn**: ASN.1 reference (`docs/COSEMpdu_GB83.asn`)
- **Commit convention**: no `Co-Authored-By` trailers

## Reference files
- `docs/golden_vectors.txt` — BER/AXDR golden test vectors
- `thirdparty/dlms-codec/` — cross-check reference
- spodes-rs `/home/trgv/spodes-rs` — parity reference
