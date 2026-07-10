# HANDOFF — OpenSPODES (full context for new session)

## Repository
- **Path**: `E:/work/opendlms/openspodes` (also `/mnt/e/work/opendlms/openspodes`)
- **Branch**: main, HEAD `d0ffc8f` (+ local commits pending)
- **Reference**: spodes-rs (Rust implementation)
- **Language**: C11, clang-format (LLVM tabs)
- **Stats**: ~100 files, 38 IC classes, 5 CTest suites

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

### Test suites (all PASS)

| Target | File | Tests | Purpose |
|--------|------|-------|---------|
| `openspodes_test` | `tests/test_core.c` | ~27 | Core codec, transport, IC, dispatcher |
| `openspodes_test_golden` | `tests/test_codec_golden.c` | ~88 | Golden vectors, BER/ACSE/xDLMS roundtrips |
| `openspodes_test_errors` | `tests/test_errors.c` | 29 | Error paths: codec, transport, client, server, HLS |
| `openspodes_test_ic` | `tests/test_ic_smoke.c` | 3 | All 38 IC classes smoke + association_ln access |
| `openspodes_test_integration` | `tests/test_integration.c` | 7 | Client↔server loopback: AARQ, HLS, GET/SET/ACTION |

**cmocka**: FetchContent `cmocka-1.1.7`, link target `cmocka` (not `cmocka_static`).

### Coverage (2026-07-10, `build-coverage`)

| Metric | Value |
|--------|-------|
| Lines | **90.4%** (2302/2547) |
| Branches | **99.2%** (1288/1298) |

| Module | Lines | Branches |
|--------|-------|----------|
| ic | 98.0% | 100.0% |
| transport | 93.1% | 98.2% |
| service | 90.9% | 100.0% |
| client | 89.8% | 100.0% |
| security | 85.4% | 100.0% |
| server | 85.3% | 92.6% |
| codec | 83.9% | 100.0% |

No file below 80% line coverage.

## Recent commits (main)

1. `77a97bd` — test suites, coverage tooling, service-layer decode fixes
2. `235f108` — serialize/BER/HLS/client connect error tests
3. `d0ffc8f` — client GET/SET error paths after association

## Service-layer fixes (in tree)

- `osp_action_request_decode` / `osp_action_response_decode`: null `data` when `have_data == 0`
- GET response datablock type mapping (encoder/decoder alignment)
- GET response error: separate DAR byte when `dar == 1`
- AARQ/AARE: BER context-specific tags, length backpatch (`ber_backpatch_length`)
- All receive buffers: `osp_buf_init` + `buf.wr = rx_len` after recv

## User Instructions (MUST follow)
- **Consult doc-rag-remote when implementing features**
- **BER length**: 1 byte if < 128, 2 bytes (`0x81`+byte) if 128–65535
- **COSEMpdu_GB83.asn**: ASN.1 reference (untracked in repo)
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
- Global crypto HAL: `osp_hal_gcm_init/update/finish` (mock in tests)
- No malloc in core; 1KB fixed PDU buffers

## Known issues / gaps

### Protocol / implementation
- `osp_server_accept` detects RLRQ by raw first byte `0x62`; `osp_rlrq_encode` emits BER context tag (`0x9F…`), not `0x62` — release path mismatch (integration passes because client ignores server decode)
- `osp_hdlc_deframe` multi-byte address roundtrip incomplete (extension-bit parsing)
- `osp_ber_write_length` unsupported for lengths > 65535
- `osp_ber_write_tag` takes `uint8_t` — tags ≥ 256 not encodable
- `osp_value_write` missing BITSTRING/UTF8STRING/BCD/FLOAT32/FLOAT64
- Data-notification encoder/decoder not implemented
- Access selection stubbed (always writes 0)
- `osp_glo_protect` / `osp_glo_unprotect` stubs
- `osp_server_run` loop branches untested (infinite loop on timeout)

### Coverage remaining (~245 lines, 10 branches)
- **client.c**: HLS GMAC error paths (bad pass4), encode failures, ACTION with return data
- **codec.c**: BER write NOMEM edge cases
- **transport.c**: HDLC deframe multi-byte address while-loops
- **server.c**: `osp_server_run` timeout/retry branches

### Untracked / debug (not in CMake)
- `tests/test_debug_flow.c`, `tests/test_aarq_direct.c`, `tests/test_ber_minimal.c`
- `docs/COSEMpdu_GB83.asn`

## Open questions
- LICENSE: needs THIRD-PARTY-LICENSES.txt for Apache 2.0 mbed TLS
- spodes-rs spodus/ module not ported (14 files)
- No example applications yet

## Next steps (suggested)
1. Fix RLRQ/RLRE: server should `osp_ber_read_tag` + match `OSP_ACSE_RLRQ_TAG`, not raw `0x62`
2. Client HLS GMAC failure tests (bad pass4 response)
3. HDLC multi-byte address deframe fix + tests
4. `osp_server_run` test harness (inject transport errors)
5. Example app: loopback client/server CLI

## Reference files
- `docs/golden_vectors.txt` — BER/AXDR golden test vectors
- `scripts/coverage_report.py` — parses `.gcov` from `build-coverage/`
- spodes-rs `src/service/acse.rs` — ACSE tag reference
