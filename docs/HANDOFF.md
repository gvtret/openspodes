# HANDOFF — OpenSPODES (full context for new session)

## Repository
- **Path**: `E:/work/opendlms/openspodes`
- **Branch**: main
- **Reference**: spodes-rs (Rust implementation)
- **Language**: C11, clang-format (LLVM tabs)
- **Stats**: ~112 files, 40 IC classes, 12 CTest suites, ~224 unit tests, GitHub CI

## Project: OpenSPODES — DLMS/COSEM protocol stack in C11
Portable C11 implementation of IEC 62056 DLMS/COSEM, modeled after spodes-rs.
No malloc in core. HAL via function pointers. MCU-pluggable.

## Architecture
```
src/codec/     BER/AXDR read/write, serialize/deserialize, compact-array
src/transport/ HDLC (IEC 62056-46) + COSEM wrapper (IEC 62056-47)
src/service/   ACSE + xDLMS + notifications + GBT codec
src/security/  HLS + glo-ciphering (AES-GCM + KUZN-CTR-CMAC suite 8/9) + general ciphering/signing
src/server/    RequestDispatcher (class_id+OBIS), osp_server_accept/run
src/client/    connect/get/set/action + with-list + block transfer + recv notification
src/spodus/    СПОДУС concentrator: MeterRegistry, direct-channel table, poll, proxy
tests/         CMocka unit/golden/error/ic/integration/phase0-2 + mock transport/crypto
.github/       CI: build + ctest + optional coverage
scripts/       coverage_report.py (gcov summary)
README.md      Production integration guide
```

## Build & Test

```bash
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
```

### Test suites — all PASS (12/12)

| Target | File | Tests | Purpose |
|--------|------|-------|---------|
| `openspodes_test` | `tests/test_core.c` | ~27 | Core codec, transport, IC, dispatcher |
| `openspodes_test_golden` | `tests/test_codec_golden.c` | ~104 | Golden vectors, BER/ACSE/xDLMS, thirdparty cross-check |
| `openspodes_test_errors` | `tests/test_errors.c` | 34 | Error paths |
| `openspodes_test_ic` | `tests/test_ic_smoke.c` | 3 | All IC classes smoke |
| `openspodes_test_integration` | `tests/test_integration.c` | 23 | Client↔server E2E loopback (+ HLS/GBT/ciphering) |
| `openspodes_test_phase0` | `tests/test_phase0.c` | 7 | SPODUS helpers |
| `openspodes_test_phase1` | `tests/test_phase1.c` | 8 | Table manager / profile filter |
| `openspodes_test_phase2` | `tests/test_phase2.c` | 11 | WithList codec, blocks, GBT confirmed + gap recovery |
| `openspodes_test_spodus` | `tests/test_spodus_concentrator.c` | 4 | СПОДУС registry, direct table, poll, proxy |
| `openspodes_test_gost` | `tests/test_gost_crypto.c` | 14 | Streebog, Kuznyechik, GOST3410, VKO, glo suite 8 |
| `openspodes_test_security` | `tests/test_security_glo.c` | 3 | glo/ded-ciphering E.5 + roundtrip (OpenSSL) |
| `openspodes_loopback_cli` | `examples/loopback_cli.c` | demo | In-process GET/SET demo (CTest) |

## Production readiness (2026-07)

| Criterion | Status |
|-----------|--------|
| Core LN client/server E2E | ✅ |
| Security: HLS + glo/ded + GOST transport | ✅ |
| GBT confirmed + lost-block recovery | ✅ |
| Exception-response on server errors | ✅ |
| Confirmed-service-error codec (SN 0x0E) | ✅ |
| Event + data notifications | ✅ |
| СПОДУС Concentrator runtime (registry, poll, proxy) | ✅ |
| CI (build + ctest) | ✅ |
| README integration guide | ✅ |
| OpenSSL optional for AES/ECDSA tests | ✅ |

## Client API

| Function | Status |
|----------|--------|
| `osp_client_connect` / `release` / `disconnect` | ✅ HLS GMAC + MD5/SHA1/SHA256 + GOST 8–10 |
| `osp_client_get` | ✅ + block reassembly |
| `osp_client_get_with_list` | ✅ |
| `osp_client_set` | ✅ + auto block transfer |
| `osp_client_set_with_list` | ✅ |
| `osp_client_action` | ✅ + param/return blocks |
| `osp_client_action_with_list` | ✅ |
| `osp_client_recv_data_notification` | ✅ |
| `osp_client_recv_event_notification` | ✅ |
| GBT runtime (xDLMS only) | ✅ unconfirmed + confirmed + gap recovery |
| glo-ciphering | ✅ AES-GCM (suite 0–2) + KUZN-CTR-CMAC (suite 8/9) |
| ded-ciphering | ✅ DEK from InitiateRequest + tags 0xD0–0xD7 |

## Server API (highlights)

| Function | Status |
|----------|--------|
| `osp_server_accept` / `run` | ✅ |
| `osp_server_send_event_notification` | ✅ |
| Exception on unassociated / decipher / IC errors | ✅ |
| Push via IC 40 | ✅ |

## СПОДУС Concentrator API

| Function | Status |
|----------|--------|
| `osp_spodus_registry_*` | ✅ meter list + aggregation cache |
| `osp_spodus_direct_table_*` | ✅ direct-channel table (§10.3) |
| `osp_spodus_concentrator_*` | ✅ downstream links + connect |
| `osp_spodus_poll_meter` | ✅ GET + cache |
| `osp_spodus_proxy_forward` | ✅ transparent APDU pass-through |

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

## Known gaps (non-blocking for typical meter/concentrator integration)

- GBT **streaming** flag (STR bit) — gap recovery works; full bi-directional streaming not wired
- `GET_WITH_LIST_BLOCK` enum only (no codec)
- Selective access encode stubbed (decode skips)
- Wire Concentrator Data objects (0.0.94.7.128/129.255) into server dispatcher as IC 1
- Golden vectors R 1323565.1 A.1 (full transport AEAD annex)
- IC stubs: UtilityTables(26), ProfileFilter(31), RegisterTable(61), StatusMapping(63), ParameterMonitor(65), MBusSlaveSetup(76)

## Next steps (optional)
1. GBT streaming + bi-directional block transfer
2. Selective access encode/decode for ProfileGeneric
3. Concentrator OBIS objects in server dispatcher (meter list / direct table)

## User Instructions (MUST follow)
- **Consult doc-rag-remote when implementing features**
- **BER length**: 1 byte if < 128, 2 bytes (`0x81`+byte) if 128–65535
- **COSEMpdu_GB83.asn**: ASN.1 reference (`docs/COSEMpdu_GB83.asn`)
- **Commit convention**: no `Co-Authored-By` trailers

## Reference files
- `README.md` — production integration guide
- `docs/golden_vectors.txt` — BER/AXDR golden test vectors
- `thirdparty/dlms-codec/` — cross-check reference
- spodes-rs `/home/trgv/spodes-rs` — parity reference
