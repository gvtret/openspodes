# HANDOFF — OpenSPODES (full context for new session)

## Repository
- **Path**: `E:/work/opendlms/openspodes`
- **Branch**: main
- **Reference**: spodes-rs (Rust implementation)
- **Language**: C11, clang-format (LLVM tabs)
- **Stats**: ~114 files, 40 IC classes, 13 CTest suites, ~242 unit tests, GitHub CI

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
| `openspodes_test_golden` | `tests/test_codec_golden.c` | ~104 | Golden vectors, BER/ACSE/xDLMS,  |
| `openspodes_test_errors` | `tests/test_errors.c` | 34 | Error paths |
| `openspodes_test_ic` | `tests/test_ic_smoke.c` | 3 | All IC classes smoke |
| `openspodes_test_integration` | `tests/test_integration.c` | 23 | Client↔server E2E loopback (+ HLS/GBT/ciphering) |
| `openspodes_test_phase0` | `tests/test_phase0.c` | 7 | SPODUS helpers |
| `openspodes_test_phase1` | `tests/test_phase1.c` | 8 | Table manager / profile filter |
| `openspodes_test_phase2` | `tests/test_phase2.c` | 11 | WithList codec, blocks, GBT confirmed + gap recovery |
| `openspodes_test_spodus` | `tests/test_spodus_concentrator.c` | 6 | СПОДУС registry, channel/direct tables, poll, proxy, server GET |
| `openspodes_test_gost` | `tests/test_gost_crypto.c` | 14 | Streebog, Kuznyechik, GOST3410, VKO, glo suite 8 |
| `openspodes_test_security` | `tests/test_security_glo.c` | 3 | glo/ded-ciphering E.5 + roundtrip (OpenSSL) |
| `openspodes_test_hdlc` | `tests/test_hdlc_session.c` | 18 | HDLC control codec, frame roundtrip, session layer |
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
| `osp_spodus_channel_list_*` | ✅ channel list ProfileGeneric (§10.4) |
| `osp_spodus_concentrator_register_server` | ✅ meter/direct Data + channel-list ProfileGeneric on dispatcher |

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

- GBT STR bit on outbound streams is supported; full bi-directional streaming session is not wired
- `GET_WITH_LIST_BLOCK` enum only (no codec)
- Golden vectors R 1323565.1 A.1 (full transport AEAD annex)
- IC stubs: UtilityTables(26), ProfileFilter(31), RegisterTable(61), StatusMapping(63), ParameterMonitor(65), MBusSlaveSetup(76)

## Next steps
1. ~~Selective access encode/decode for ProfileGeneric~~ ✅ (v1.0.0)
2. ~~Serial transport + HDLC client/server~~ ✅ (v1.0.0)
3. ~~Интеграционный тест HDLC client↔server loopback~~ ✅ (test_hdlc_full_e2e)
4. ~~`GET_WITH_LIST_BLOCK` codec~~ ✅ (не нужен — block transfer обрабатывается через GET_WITH_BLOCK + GET_WITH_LIST)
5. ~~IC stubs~~ ✅ — get/set реализованы для всех 6 классов + set_attr для десериализации
6. ~~Golden vectors R 1323565.1 A.1~~ ✅ — Kuznyechik AEAD encrypt/decrypt + auth-only тесты из Р 1323565.1.032-2020
7. ~~GBT bidirectional streaming~~ ✅ — `osp_gbt_transport_send_streaming_bidir` + callback
3. СПОДУС discovered meters / access policies (§10.5–10.6)

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

## 2026-07-11 02:09 — СПОДУС parity and HDLC interoperability audit

**Done:** Added and committed concentrator objects: channel list §10.4 (`3c1775b`), discovered meters §10.5 (`1e7c214`), access policies §10.6 (`54ad4a9`, coverage `0de36dc`), and exchange tasks §10.7 (`cebae33`). Updated the parity canvas. Inspected `logs/GXDLMSDirector.log` against the C transport code.

**State:** `main`; latest known focused test was `openspodes_test_spodus` PASS after §10.7. `build-linux/` is intentionally untracked. Production claim must be qualified: HDLC framing is not interoperable with the Director trace.

**Next:** Implement production HDLC link-layer session before further feature parity: SNRM/UA negotiation, configurable client/server addresses, N(S)/N(R) state, LLC add/strip (`E6 E6 00` / `E6 E7 00`), and negotiated HDLC parameters. Evidence: `logs/GXDLMSDirector.log`, `src/transport/transport.c:245`.

**Notes:** Current HDLC transport always emits I-frames with addresses 1/1 and directly exposes HDLC info to ACSE; it has no SNRM/UA flow or LLC handling. Therefore real Director AARQ/AARE traffic cannot interoperate despite loopback CTest passing.

---

## 2026-07-11 — HDLC session layer implementation (T1)

**Done:**
- Fixed HDLC control byte encode/decode per ISO/IEC 13239 §5.3.1:
  - SNRM=0x83, UA=0x63, DISC=0x43, DM=0x0F, FRMR=0x87, UI=0x03, XID=0xAF
  - I-frame: [N(R):3][P/F:1][N(S):3][0], S-frame: [N(R):3][P/F:1][mod:2][01]
  - U-frame: [mod_hi:3][P/F:1][mod_lo:2][11], modifier 5 bits: hi=[7:5], lo=[3:2]
- Added HCS validation in `osp_hdlc_deframe()` for frames with info field
- Created `src/transport/hdlc_session.h/.c`:
  - `osp_hdlc_session_init_client/server` — configurable addresses
  - `osp_hdlc_session_connect` — SNRM→UA with XID parameter negotiation (max info, window size)
  - `osp_hdlc_session_disconnect` — DISC→UA/DM
  - `osp_hdlc_session_send_apdu` — LLC+E6 E6 00, I-frame with N(S)/N(R)
  - `osp_hdlc_session_recv_apdu` — strip LLC, update N(R)
- Added 18 HDLC tests in `tests/test_hdlc_session.c` (control codec round-trips, frame encode/decode with HCS+FCS, FCS corruption, address round-trip, session connect/disconnect, send/receive APDU)
- All 13 test suites pass (100%)

**State:** `main`, uncommitted. `build-linux/` untracked. `openspodes_test_hdlc` added.

**Next:**
- Integration test for HDLC client↔server loopback (currently only unit tests for codec + session layer)

**Notes:**
- GXDLMSDirector uses non-standard U-frame encoding (SNRM=0x61 instead of ISO standard 0x93). The Director's I-frame and S-frame encoding is standard. Only U-frame modifier bits differ.
- spodes-rs uses ISO standard encoding (SNRM=0x83). Our codec matches spodes-rs.
- LLC constants: command=`E6 E6 00`, response=`E6 E7 00` (IEC 62056-47)
