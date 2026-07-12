# OpenSPODES Troubleshooting Guide

Common issues and solutions for DLMS/COSEM integration with OpenSPODES.

## HDLC Issues

### No connection established (SNRM/UA fails)

**Symptoms**: `osp_hdlc_session_connect()` returns `OSP_ERR_TIMEOUT`

**Possible causes**:
1. Wrong addresses — check client/server HDLC addresses match
2. Baud rate mismatch — verify serial configuration
3. Physical layer issue — check wiring, RS-485 direction control
4. XID negotiation failure — some devices reject non-standard XID params

**Debug steps**:
```c
// Print session state before connect
printf("State: %d\n", osp_hdlc_session_state(&session));

// Verify addresses
printf("Client addr: %08X (%d bytes)\n", client_addr, client_addr_len);
printf("Server addr: %08X (%d bytes)\n", server_addr, server_addr_len);
```

### HDLC frames received but rejected

**Symptoms**: FCS errors, frames silently dropped

**Possible causes**:
1. CRC mismatch — wrong polynomial or byte order
2. Byte stuffing error — check flag detection (0x7E)
3. Frame too large — exceeds `OSP_HDLC_MAX_FRAME_SIZE` (512 bytes)

**Debug**: Enable raw frame logging in transport implementation.

### GXDLMSDirector interoperability

**Known issues**:
- GXDLMSDirector may use non-standard XID parameters
- Some versions expect specific system title format
- Try: disable HDLC encryption in Director settings first

## Authentication Issues

### AARE result = rejected (permanent)

**Symptoms**: Association rejected immediately after AARQ

**Possible causes**:
1. Wrong `system_title` — must match provisioned value
2. Wrong mechanism — server doesn't support requested HLS mechanism
3. Missing keys — `get_key()` returns NULL

**Fix**: Start with mechanism 0 (Lowest) to test basic connectivity, then add HLS.

### HLS pass 3 fails (client → server)

**Symptoms**: Server rejects f(StoC)

**Possible causes**:
1. Wrong GAK key on client side
2. Wrong StoC challenge (stale or mismatched)
3. Wrong hash implementation (MD5/SHA1/SHA256/GOST mismatch)

**Debug**:
```c
// Log the challenge values
printf("StoC: ");
for (int i = 0; i < ctx.stoc_len; i++) printf("%02X", ctx.stoc[i]);
printf("\n");
```

### HLS pass 4 fails (server → client)

**Symptoms**: Client rejects f(CtoS)

**Possible causes**:
1. Wrong GAK key on server side
2. Server computed wrong f(CtoS)
3. Client system_title mismatch

### Invocation counter error

**Symptoms**: `OSP_ERR_SECURITY` during glo-protect/unprotect

**Cause**: IC overflow (reached 0xFFFFFFFF) or IC mismatch between peers

**Fix**: Re-key the association. IC must be monotonically increasing.

## Encryption Issues

### Glo-unprotect fails with authentication error

**Symptoms**: `osp_glo_unprotect()` returns -1

**Possible causes**:
1. Wrong key (GUEK for decryption, GAK for GMAC)
2. Tampered ciphertext
3. Wrong IC (replay detection triggered)
4. Wrong system_title in IV

### General ciphering (0xDD) fails

**Symptoms**: `osp_gen_ciphering_unprotect()` returns -1

**Possible causes**:
1. Wrong transaction_id
2. Wrong recipient system_title
3. Underlying glo-protect failed

## Buffer Issues

### OSP_ERR_NOMEM during encode

**Symptoms**: Encoding fails with buffer full

**Fix**: Increase buffer size:
```c
// In CMakeLists.txt or compile flags:
#define OSP_CLIENT_MAX_PDU 2048  // default is 1024
#define OSP_SERVER_MAX_PDU 2048
```

### Stack overflow on MCU

**Symptoms**: Hard fault, watchdog reset during `osp_server_accept()`

**Cause**: `osp_server_t` is ~40KB — must be static/global, never stack-allocated

**Fix**:
```c
// GOOD
static osp_server_t server;

// BAD — will overflow MCU stack
void handler() {
    osp_server_t server;  // ~40KB on stack!
}
```

## Thread Safety Issues

### Corruption when using multiple threads

**Cause**: `osp_hal_mutex` not set, or same `osp_client_t`/`osp_server_t` shared between threads

**Fix**:
1. Set `osp_hal_mutex` to platform mutex
2. Each thread must use its own client/server context
3. Only the codec `value_read_pool` is shared (protected by mutex)

### Deadlock

**Cause**: Nested mutex acquisition (rare — library doesn't nest)

**Fix**: Ensure your mutex implementation is recursive or verify no nesting occurs.

## Performance Issues

### Slow throughput

**Possible causes**:
1. Small GBT block size — increase via `osp_client_enable_gbt(&client, 256)`
2. Serial baud rate too low — increase to 115200 or higher
3. Unnecessary encryption — disable glo-ciphering for trusted networks
4. Frequent retransmissions — check HDLC link quality

### High CPU usage in poll loop

**Fix**: Use `osp_server_accept()` with appropriate timeout:
```c
// Don't poll too fast
osp_server_accept(&server, 100);  // 100ms timeout
```

## Build Issues

### OpenSSL not found

**Symptoms**: AES-GCM tests skip, security tests limited

**Fix**:
```bash
# Debian/Ubuntu
sudo apt-get install libssl-dev

# Fedora/RHEL
sudo dnf install openssl-devel

# macOS
brew install openssl
```

### CMocka fetch fails

**Symptoms**: Build fails during FetchContent

**Fix**: Check internet connectivity, or manually download CMocka:
```bash
cd build/_deps
git clone https://git.cryptomilk.org/projects/cmocka.git
```

## Debug Tips

### Enable verbose logging

The library has no built-in logging, but you can add tracing at the transport layer:

```c
static osp_err_t debug_send(void *ctx, const uint8_t *data, uint32_t len) {
    printf("TX [%u bytes]: ", len);
    for (uint32_t i = 0; i < len && i < 32; i++) printf("%02X", data[i]);
    if (len > 32) printf("...");
    printf("\n");
    return real_send(ctx, data, len);
}
```

### Use loopback for debugging

The loopback example runs entirely in-process — no network issues:
```bash
./build/openspodes_loopback_cli demo
```

### Check security context state

```c
printf("Suite: %d, Mech: %d, IC: %u\n",
       ctx.suite, ctx.mechanism, ctx.invocation_counter);
printf("GUEK: "); for (int i=0; i<16; i++) printf("%02X", ctx.guek[i]); printf("\n");
printf("GAK:  "); for (int i=0; i<16; i++) printf("%02X", ctx.gak[i]); printf("\n");
```
