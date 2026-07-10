# HANDOFF — OpenSPODES (full context for new session)

## Repository
- **Path**: `E:/work/opendlms/openspodes`
- **Branch**: main, HEAD `e7f3828`
- **Reference**: spodes-rs (Rust implementation)
- **Language**: C11, clang-format (LLVM tabs)
- **Stats**: ~100 files, 38 IC classes, 5 CTest suites, ~160 unit tests

## Project: OpenSPODES — DLMS/COSEM protocol stack in C11
Portable C11 implementation of IEC 62056 DLMS/COSEM, modeled after spodes-rs.
No malloc in core. HAL via function pointers. MCU-pluggable.

## Architecture
```
src/codec/     BER/AXDR read/write, serialize/deserialize, composite structures
src/transport/ HDLC (IEC 62056-46) + COSEM wrapper (IEC 62056-47)
src/service/   ACSE (AARQ/AARE/RLRQ/RLRE) + GET/SET/ACTION + ExceptionResponse
src/security/  HLS GMAC (pass 3/4) + replay protection + glo stubs
src/server/    RequestDispatcher (class_id+OBIS), osp_server_accept/run
src/client/    osp_client_connect/get/set/action/release/disconnect
src/ic/        38 IC classes, vtable pattern
tests/         CMocka unit/golden/error/ic/integration + mock transport/crypto
scripts/       coverage_report.py (gcov summary)
```

## Build & Test

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

# Coverage (gcov)
cmake -S . -B build-coverage -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-coverage --target coverage
```

### Test suites — all PASS (5/5)

| Target | File | Tests | Purpose |
|--------|------|-------|---------|
| `openspodes_test` | `tests/test_core.c` | ~27 | Core codec, transport, IC, dispatcher |
| `openspodes_test_golden` | `tests/test_codec_golden.c` | ~88 | Golden vectors, BER/ACSE/xDLMS roundtrips |
| `openspodes_test_errors` | `tests/test_errors.c` | 34 | Error paths: codec, transport, client, server, HLS |
| `openspodes_test_ic` | `tests/test_ic_smoke.c` | 3 | All 38 IC classes smoke + association_ln access |
| `openspodes_test_integration` | `tests/test_integration.c` | 7 | Client↔server loopback: AARQ, HLS, GET/SET/ACTION |

**cmocka**: FetchContent `cmocka-1.1.7`, link target `cmocka`.

### Coverage (`build-coverage`, 2026-07-10)

| Metric | Value |
|--------|-------|
| Lines | **90.1%** (2328/2584) |
| Branches | **99.3%** (1334/1344) |

| Module | Lines | Branches |
|--------|-------|----------|
| ic | 98.0% | 100.0% |
| transport | 91.8% | 98.4% |
| client | 90.8% | 100.0% |
| service | 89.8% | 100.0% |
| security | 85.6% | 100.0% |
| server | 84.7% | 92.9% |
| codec | 83.9% | 100.0% |

No file below 80% line coverage.

## Session accomplishments (2026-07-10)

### Test infrastructure
- 5 CTest suites with FetchContent cmocka, mock transport/crypto
- `ENABLE_COVERAGE` + `coverage` CMake target + `scripts/coverage_report.py`
- Coverage: 53% → **90.1%** lines, 73% → **99.3%** branches

### Protocol fixes
- **Service layer**: ACTION null data, GET datablock types, GET error DAR byte
- **AARQ/AARE**: BER context tags, `ber_backpatch_length`
- **RLRQ/RLRE**: APPLICATION tags 2/3 (`0x62`/`0x63`), decode validation, client verifies RLRE
- **HDLC addresses**: IEC 62056-46 7-bit encoding + extension bit
- **Loopback transport**: `mock_loopback_send()` propagates `osp_server_accept` errors (no silent hang)

### Debug helpers (committed, not in CMake)
- `tests/test_debug_flow.c` — printf loopback trace
- `tests/test_ber_minimal.c` — BER [1] EXPLICIT length backpatch demo

## Recent commits (main)

1. `77a97bd` — test suites, coverage tooling, service decode fixes
2. `235f108` — serialize/BER/HLS/client connect error tests
3. `d0ffc8f` — client GET/SET error paths
4. `6e79d4f` — transport/server tests
5. `90d990c` — RLRQ/RLRE APPLICATION tag fix
6. `0ae03be` — HDLC 7-bit address + HLS GMAC client error tests
7. `e7f3828` — loopback error propagation + BER backpatch debug test

## User Instructions (MUST follow)
- **Consult doc-rag-remote when implementing features**
- **BER length**: 1 byte if < 128, 2 bytes (`0x81`+byte) if 128–65535
- **COSEMpdu_GB83.asn**: ASN.1 reference (untracked: `docs/COSEMpdu_GB83.asn`)
- **Commit convention**: no `Co-Authored-By` trailers

## IC Classes (38 implemented)
Data(1) Register(3) ExtRegister(4) DemandRegister(5) RegisterActivation(6)
ProfileGeneric(7) Clock(8) ScriptTable(9) Schedule(10) SpecialDays(11)
AssociationLN(15) SAPAssignment(17) ImageTransfer(18) IEC_HDLCSetup(23)
UtilityTables(26) DataProtection(30) ProfileFilter(31) PushSetup(40)
IPv4Setup(42) MACAddress(43) IPv6Setup(48) RegisterTable(61) CompactData(62)
StatusMapping(63) SecuritySetup(64) ParameterMonitor(65) Arbitrator(68)
DisconnectControl(70) Limiter(71) ActivityCalendar(20) RegisterMonitor(21)
SingleActionSchedule(22) IEC_LocalPortSetup(19) GPRSModemSetup(45)
GSMDiagnostic(47) MBusSlaveSetup(76) TableManager(8200)

## HAL
- `osp_transport_t`: open/send/recv/close/is_connected
- `mock_loopback_send(pair, server, data, len)` — test helper in `mock_transport.c`
- Global crypto HAL: `osp_hal_gcm_init/update/finish` (mock in tests)
- No malloc in core; 1KB fixed PDU buffers

## Known gaps

### Protocol / implementation
- `osp_server_run` loop untested (infinite retry on timeout)
- `osp_ber_write_tag` takes `uint8_t` — tags ≥ 256 not encodable
- `osp_value_write` missing BITSTRING/UTF8STRING/BCD/FLOAT32/FLOAT64
- Data-notification encoder/decoder not implemented
- Access selection stubbed (always writes 0)
- `osp_glo_protect` / `osp_glo_unprotect` stubs

### Coverage remaining (~256 lines, 10 branches)
- **client.c**: encode failures, ACTION with return data (non-HLS)
- **codec.c**: BER write NOMEM edge cases
- **server.c**: `osp_server_run` timeout/retry branches

### Untracked
- `tests/test_aarq_direct.c`
- `docs/COSEMpdu_GB83.asn`

## Next steps (suggested)
1. `osp_server_run` test harness
2. Example app: loopback client/server CLI
3. Client ACTION with non-null return data (non-HLS methods)

## Reference files
- `docs/golden_vectors.txt` — BER/AXDR golden test vectors
- `scripts/coverage_report.py` — gcov report from `build-coverage/`
- spodes-rs `src/service/acse.rs` — ACSE tag reference
