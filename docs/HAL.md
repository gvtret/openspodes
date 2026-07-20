# OpenSPODES HAL Porting Guide

How to port OpenSPODES to a new MCU/MPU platform by implementing the Hardware Abstraction Layer (HAL).

## Overview

OpenSPODES is a zero-heap, static-buffer DLMS/COSEM stack. The core library has **no platform dependencies** — all hardware interaction goes through HAL function pointers set at initialization time. You implement 5-6 HAL interfaces depending on your platform.

```
┌─────────────────────────────────────────────────┐
│  Your Application                               │
├─────────────────────────────────────────────────┤
│  OpenSPODES Library (platform-independent)      │
├─────────────────────────────────────────────────┤
│  HAL Interfaces (you implement these)           │
│  ┌────────────┬──────────┬────────┬───────────┐ │
│  │ Transport  │ Crypto   │ Random │ Timer     │ │
│  │ (UART/TCP) │ (AES-GCM)│ (RNG)  │ (SysTick) │ │
│  ├────────────┼──────────┼────────┼───────────┤ │
│  │ System     │ Mutex    │        │           │ │
│  │ (Keys/ID)  │ (RTOS)   │        │           │ │
│  └────────────┴──────────┴────────┴───────────┘ │
└─────────────────────────────────────────────────┘
```

## HAL Interfaces

### 1. Transport (`osp_transport_t`)

Required. Handles byte-level I/O (UART, TCP socket, etc.).

```c
typedef struct {
    osp_err_t (*open)(void *ctx);
    osp_err_t (*send)(void *ctx, const uint8_t *data, uint32_t len);
    osp_err_t (*recv)(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms);
    void (*close)(void *ctx);
    bool (*is_connected)(void *ctx);
    void *ctx;  // your private data (UART handle, socket fd, etc.)
} osp_transport_t;
```

| Function | Contract |
|----------|----------|
| `open` | Initialize the transport (open UART, connect TCP). Return `OSP_OK` on success. |
| `send` | Send `len` bytes. Block until all bytes sent or return `OSP_ERR_IO`. |
| `recv` | Read up to `size` bytes into `buf`. Wait up to `timeout_ms` ms. Set `*out_len` to actual bytes read. Return `OSP_ERR_TIMEOUT` if no data. |
| `close` | Release transport resources. |
| `is_connected` | Return true if the transport link is up. |

**Implementation notes:**
- `recv` may return 0 bytes with `OSP_OK` (poll-style) or block until timeout
- For HDLC framing, `open` typically sends SNRM; for WRAPPER, it's a no-op
- Each `osp_client_t` / `osp_server_t` needs its own transport instance

**Bare metal example (UART):**
```c
static osp_err_t uart_send(void *ctx, const uint8_t *data, uint32_t len) {
    uart_handle_t *uart = (uart_handle_t *)ctx;
    for (uint32_t i = 0; i < len; i++) {
        uart_putc(uart, data[i]);
    }
    return OSP_OK;
}

static osp_err_t uart_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms) {
    uart_handle_t *uart = (uart_handle_t *)ctx;
    uint32_t start = systick_ms();
    uint32_t got = 0;
    while (got < size && (systick_ms() - start) < timeout_ms) {
        if (uart_data_available(uart)) {
            buf[got++] = uart_getc(uart);
        }
    }
    *out_len = got;
    return (got > 0) ? OSP_OK : OSP_ERR_TIMEOUT;
}
```

**FreeRTOS example:**
```c
static osp_err_t freertos_recv(void *ctx, uint8_t *buf, uint32_t size, uint32_t *out_len, uint32_t timeout_ms) {
    freertos_uart_t *u = (freertos_uart_t *)ctx;
    uint32_t got = 0;
    while (got < size) {
        uint8_t byte;
        if (xQueueReceive(u->rx_queue, &byte, pdMS_TO_TICKS(timeout_ms)) != pdTRUE)
            break;
        buf[got++] = byte;
    }
    *out_len = got;
    return OSP_OK;
}
```

### 2. Crypto (`osp_hal_*` global function pointers)

Required for HLS authentication and glo-ciphering. Set global function pointers before calling `osp_client_connect` or `osp_server_accept`.

```c
// One-shot AES-GCM (preferred, simpler):
extern int (*osp_hal_gcm_crypt)(osp_gcm_dir_t dir, const uint8_t *key, uint32_t key_len,
    const uint8_t iv[12], const uint8_t *aad, uint32_t aad_len,
    const uint8_t *in, uint32_t in_len, uint8_t *out,
    const uint8_t tag_in[12], uint8_t tag_out[12]);

// Hash functions (required for HLS mechanisms 3/4/6):
extern void (*osp_hal_md5)(const uint8_t *input, uint32_t len, uint8_t output[16]);
extern void (*osp_hal_sha1)(const uint8_t *input, uint32_t len, uint8_t output[20]);
extern void (*osp_hal_sha256)(const uint8_t *input, uint32_t len, uint8_t output[32]);

// GOST crypto (required for suite 8/9):
extern void (*osp_hal_streebog256)(const uint8_t *input, uint32_t len, uint8_t output[32]);
extern int (*osp_hal_ecdsa_sign)(...);
extern int (*osp_hal_ecdsa_verify)(...);
```

| Pointer | Required for | Notes |
|---------|-------------|-------|
| `osp_hal_gcm_crypt` | HLS-GMAC (mech 5), glo-ciphering (suite 0) | AES-128-GCM, 12-byte IV, 12-byte tag |
| `osp_hal_md5` | HLS-MD5 (mech 3) | Optional if mech 3 not used |
| `osp_hal_sha1` | HLS-SHA1 (mech 4) | Optional if mech 4 not used |
| `osp_hal_sha256` | HLS-SHA256 (mech 6) | Optional if mech 6 not used |
| `osp_hal_streebog256` | Suite 8/9 GOST | Required for GOST suite |
| `osp_hal_ecdsa_sign/verify` | HLS-ECDSA (mech 7) | Required for ECDSA |

**Set to NULL** any function you don't need. The library checks for NULL before calling.

**OpenSSL example (Linux/macOS):**
```c
#include <openssl/evp.h>

static int openssl_gcm_crypt(osp_gcm_dir_t dir, const uint8_t *key, uint32_t key_len,
    const uint8_t iv[12], const uint8_t *aad, uint32_t aad_len,
    const uint8_t *in, uint32_t in_len, uint8_t *out,
    const uint8_t *tag_in, uint8_t *tag_out) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, key, iv);
    if (aad && aad_len) EVP_EncryptUpdate(ctx, NULL, NULL, aad, aad_len);
    if (dir == OSP_GCM_ENCRYPT) {
        EVP_EncryptUpdate(ctx, out, NULL, in, in_len);
        EVP_EncryptFinal_ex(ctx, out, NULL);
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 12, tag_out);
    } else {
        EVP_DecryptUpdate(ctx, out, NULL, in, in_len);
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 12, (void *)tag_in);
        EVP_DecryptFinal_ex(ctx, out, NULL);
    }
    EVP_CIPHER_CTX_free(ctx);
    return 0;
}

// At init:
osp_hal_gcm_crypt = openssl_gcm_crypt;
```

**Bare metal (hardware AES engine):**
```c
static int hw_gcm_crypt(osp_gcm_dir_t dir, const uint8_t *key, uint32_t key_len,
    const uint8_t iv[12], const uint8_t *aad, uint32_t aad_len,
    const uint8_t *in, uint32_t in_len, uint8_t *out,
    const uint8_t *tag_in, uint8_t *tag_out) {
    hw_aes_load_key(key, key_len);
    hw_aes_set_iv(iv, 12);
    if (aad && aad_len) hw_aes_gcm_aad(aad, aad_len);
    if (dir == OSP_GCM_ENCRYPT) {
        hw_aes_gcm_encrypt(in, in_len, out);
        hw_aes_gcm_get_tag(tag_out, 12);
    } else {
        hw_aes_gcm_decrypt(in, in_len, out);
        if (hw_aes_gcm_verify_tag(tag_in, 12) != 0) return -1;
    }
    return 0;
}
```

### 3. Random (`osp_hal_random_fill`)

Required for HLS challenge generation (server-side stoc). Without it, the server uses a deterministic fallback (insecure).

```c
extern int (*osp_hal_random_fill)(uint8_t *buf, uint32_t len);
```

Returns `OSP_OK` on success. Must fill `buf` with `len` cryptographically random bytes.

| Platform | Implementation |
|----------|---------------|
| Linux | `read("/dev/urandom", buf, len)` or `getrandom(buf, len, 0)` |
| FreeRTOS | `esp_random(buf, len)` or HAL RNG peripheral |
| Zephyr | `sys_rand_get(buf, len)` |
| Bare metal | Hardware TRNG peripheral, `RNG->DR` register |
| STM32 | `HAL_RNG_GenerateRandomNumber()` |

### 4. Timer (`osp_timer_t`)

Optional. Used for timeout calculations. If NULL, the library uses polling loops.

```c
typedef struct {
    uint32_t (*now_ms)(void *ctx);     // returns milliseconds since boot
    void (*delay_ms)(void *ctx, uint32_t ms);  // blocking delay
    void *ctx;
} osp_timer_t;
```

| Platform | Implementation |
|----------|---------------|
| Linux | `clock_gettime(CLOCK_MONOTONIC)` |
| FreeRTOS | `xTaskGetTickCount() * portTICK_PERIOD_MS` |
| Zephyr | `k_uptime_get_32()` |
| Bare metal | SysTick counter, `DWT->CYCCNT / (SystemCoreClock / 1000)` |

### 5. System (`osp_system_t`)

Required. Provides system title and key storage.

```c
typedef struct {
    uint8_t system_title[8];                          // unique device ID
    uint8_t *(*get_key)(void *ctx, uint8_t sap, uint8_t key_id);  // key lookup
    void *ctx;
} osp_system_t;
```

- `system_title`: 8-byte unique identifier (typically MAC address, serial number, or manufacturer-assigned)
- `get_key`: returns pointer to key buffer for given SAP and key_id. Return NULL if key not found.

**Key IDs** (per IEC 62056-5-3):

| key_id | Name | Size |
|--------|------|------|
| 0 | GUEK (Global Unicast Encryption Key) | 16 bytes |
| 1 | GAK (Global Authentication Key) | 16 bytes |
| 2 | GBEK (Global Broadcast Encryption Key) | 16 bytes |
| 3 | KEK (Key Encryption Key) | 16 bytes |

**Example:**
```c
static uint8_t g_guek[16] = {0x00, 0x01, ...};
static uint8_t g_gak[16]  = {0x10, 0x11, ...};

static uint8_t *system_get_key(void *ctx, uint8_t sap, uint8_t key_id) {
    (void)ctx; (void)sap;
    switch (key_id) {
        case 0: return g_guek;
        case 1: return g_gak;
        default: return NULL;
    }
}
```

### 6. Mutex (`osp_mutex_t`) — Optional

Required only for multi-threaded use (Linux, FreeRTOS, Zephyr). Leave `osp_hal_mutex = NULL` for bare-metal single-threaded operation (zero overhead).

```c
typedef struct {
    void *(*create)(void *ctx);
    int (*lock)(void *ctx, void *handle);
    void (*unlock)(void *ctx, void *handle);
    void (*destroy)(void *ctx, void *handle);
    void *ctx;
} osp_mutex_t;

extern osp_mutex_t *osp_hal_mutex;  // NULL = no locking
```

**Linux/pthreads:**
```c
#include <pthread.h>
#include <stdlib.h>

static void *pt_create(void *ctx) {
    (void)ctx;
    pthread_mutex_t *m = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(m, NULL);
    return m;
}
static int pt_lock(void *ctx, void *h) { (void)ctx; return pthread_mutex_lock(h); }
static void pt_unlock(void *ctx, void *h) { (void)ctx; pthread_mutex_unlock(h); }
static void pt_destroy(void *ctx, void *h) {
    (void)ctx;
    pthread_mutex_destroy(h);
    free(h);
}

osp_mutex_t hal_mutex = { pt_create, pt_lock, pt_unlock, pt_destroy, NULL };
// At init: osp_hal_mutex = &hal_mutex;
```

**FreeRTOS:**
```c
static void *freertos_create(void *ctx) { (void)ctx; return xSemaphoreCreateMutex(); }
static int freertos_lock(void *ctx, void *h) {
    (void)ctx;
    return (xSemaphoreTake(h, portMAX_DELAY) == pdTRUE) ? 0 : -1;
}
static void freertos_unlock(void *ctx, void *h) { (void)ctx; xSemaphoreGive(h); }
static void freertos_destroy(void *ctx, void *h) { (void)ctx; vSemaphoreDelete(h); }

osp_mutex_t hal_mutex = { freertos_create, freertos_lock, freertos_unlock, freertos_destroy, NULL };
```

**Zephyr:**
```c
#include <zephyr/kernel.h>

static void *zephyr_create(void *ctx) { (void)ctx; return NULL; }  // uses global
static int zephyr_lock(void *ctx, void *h) { (void)ctx; (void)h; k_mutex_lock(&g_mutex, K_FOREVER); return 0; }
static void zephyr_unlock(void *ctx, void *h) { (void)ctx; (void)h; k_mutex_unlock(&g_mutex); }
static void zephyr_destroy(void *ctx, void *h) { (void)ctx; (void)h; }

static K_MUTEX_DEFINE(g_mutex);
osp_mutex_t hal_mutex = { zephyr_create, zephyr_lock, zephyr_unlock, zephyr_destroy, NULL };
```

---

## Buffer Sizing

The library uses static buffers sized by these constants:

| Constant | Default | Purpose |
|----------|---------|---------|
| `OSP_CLIENT_MAX_PDU` | 1024 | Client TX/RX buffer size |
| `OSP_SERVER_MAX_PDU` | 1024 | Server TX/RX buffer size |
| `OSP_SERVER_PENDING_MAX` | 16384 | Block transfer reassembly buffer (×16 for ALN object_list) |
| `OSP_HDLC_MAX_FRAME_SIZE` | 1024 | Maximum HDLC frame (configurable via #ifndef) |
| `OSP_GLO_MAX_PLAIN` | 1024 | Max plaintext for glo-ciphering |

**Recommendations:**
- For small meters: keep defaults (total RAM ~12-16 KB)
- For concentrators: increase `OSP_SERVER_MAX_PDU` and `OSP_SPODUS_MAX_METERS`
- For constrained MCUs (< 32 KB RAM): reduce all buffers to 256-512 bytes

**Total static RAM usage** (default config, per connection):
- `osp_client_t`: ~8 KB (tx_buf + rx_buf + reassembly)
- `osp_server_t`: ~40 KB (tx_buf + rx_buf + 2× pending_get/set + action buffers)
- `osp_value_read_pool`: ~4 KB (shared, thread-safe via mutex)

---

## Static vs Stack Allocation

**Always allocate `osp_client_t` and `osp_server_t` as static or global variables:**

```c
// GOOD — static allocation (recommended for MCU)
static osp_server_t server;
static osp_client_t client;

// BAD — stack allocation (may overflow on MCU)
void handler(void) {
    osp_server_t server;  // ~40 KB on stack!
    osp_server_init(&server, ...);
}
```

On Linux/desktop, stack allocation is fine (8 MB default stack).

---

## Thread Safety

The library is safe for multi-threaded use when:
1. Each thread uses its own `osp_client_t` / `osp_server_t`
2. `osp_hal_mutex` is set to a platform-appropriate mutex implementation
3. The `value_read_pool` and crypto buffers use thread-local storage (automatic via `OSP_TLS`)

**How it works:**
- `OSP_TLS` (defined in `openspodes.h`) resolves to `_Thread_local` (C11), `__thread` (GCC/Clang), or `static` (bare-metal)
- All internal crypto and serialization buffers are TLS-allocated — no data races between threads
- On bare-metal (no OS), TLS falls back to plain `static`, which is safe for single-threaded use

**For bare-metal MCU:** Leave `osp_hal_mutex = NULL`. TLS becomes static — zero overhead, no threads to worry about.

---

## Porting Checklist

### Step 1: Transport
- [ ] Implement `osp_transport_t` for your physical layer (UART, TCP, SPI, etc.)
- [ ] Test: loop raw bytes through send/recv without OpenSPODES

### Step 2: Crypto
- [ ] Implement `osp_hal_gcm_crypt` (AES-128-GCM)
- [ ] Implement hash functions needed for your HLS mechanism:
  - [ ] Mechanism 5 (GMAC): only `osp_hal_gcm_crypt` needed
  - [ ] Mechanism 6 (SHA-256): also `osp_hal_sha256`
  - [ ] Suite 8/9 (GOST): use built-in Kuznyechik/Streebog (no HAL needed)
- [ ] Test: encrypt/decrypt a known AES-GCM vector

### Step 3: Random
- [ ] Implement `osp_hal_random_fill` using your platform's TRNG/RNG
- [ ] Test: generate 32 bytes, verify non-zero and non-repeating

### Step 4: Timer (optional)
- [ ] Implement `osp_timer_t` if using timeout-based logic
- [ ] Test: verify `now_ms()` increments correctly

### Step 5: System
- [ ] Set `system_title` to a unique 8-byte device identifier
- [ ] Implement `get_key` to return pointers to provisioned keys
- [ ] Verify keys are loaded from secure storage (not hardcoded!)

### Step 6: Mutex (multi-threaded only)
- [ ] Implement `osp_mutex_t` for your RTOS/OS
- [ ] Set `osp_hal_mutex` at init
- [ ] Test: run `openspodes_test_thread` to verify

### Step 7: Integration
- [ ] Initialize all HAL pointers at startup
- [ ] Call `osp_server_init` / `osp_client_init`
- [ ] Register IC objects with `osp_server_register`
- [ ] Run `openspodes_test_golden` to verify codec correctness
- [ ] Run `openspodes_test_security` to verify crypto (requires OpenSSL or hardware)
- [ ] Test end-to-end: connect, GET, SET, disconnect


### 7. Data HAL (`osp_hal_data_t`)

Optional. Provides read/write/execute access to hardware data (sensors, setpoints, commands).

```c
typedef struct {
    osp_err_t (*read)(void *ctx, const osp_obis_t *obis, uint8_t attr_id, osp_value_t *result);
    osp_err_t (*write)(void *ctx, const osp_obis_t *obis, uint8_t attr_id, const osp_value_t *value);
    osp_err_t (*execute)(void *ctx, const osp_obis_t *obis, uint8_t method_id,
                         const osp_value_t *param, osp_value_t *result);
    void *ctx;
} osp_hal_data_t;

extern osp_hal_data_t *osp_hal_data;  // NULL = no hardware access (default)
```

| Pointer | Purpose | Returns |
|---------|---------|---------|
| `read` | Read current value from hardware | OSP_OK, OSP_ERR_NOT_FOUND, OSP_ERR_IO |
| `write` | Write setpoint to hardware | OSP_OK, OSP_ERR_NOT_FOUND, OSP_ERR_IO |
| `execute` | Execute command on hardware | OSP_OK, OSP_ERR_NOT_FOUND, OSP_ERR_IO |

**Caching strategy:** HAL + cache. IC classes call HAL during polling/set operations. DLMS GET returns cached value (fast, no HAL in response path).

**Usage:**
```c
// 1. Implement HAL callbacks
static osp_err_t my_read(void *ctx, const osp_obis_t *obis, uint8_t attr_id, osp_value_t *result) {
    // Read from sensors, ADC, Modbus, etc.
    // Return OSP_ERR_NOT_FOUND if obis/attr_id not mapped to hardware
}

// 2. Set HAL pointer before server loop
osp_hal_data_t my_hal = { .read = my_read, .write = my_write, .execute = my_execute, .ctx = NULL };
osp_hal_data = &my_hal;

// 3. Optionally call osp_hal_data_poll() periodically to refresh caches
osp_hal_data_poll(&server);
```

---

---

## Quick Start (Linux)

```c
#include "openspodes.h"
#include "client/client.h"
#include "server/server.h"
#include "security/security.h"
#include "ic/data.h"

// Your transport, crypto, etc. implementations
#include "my_hal.h"

int main(void) {
    // 1. Set HAL pointers
    osp_hal_gcm_crypt = my_gcm_crypt;
    osp_hal_sha256 = my_sha256;
    osp_hal_random_fill = my_random;
    osp_hal_mutex = &my_mutex;  // optional

    // 2. Init transport
    osp_transport_t transport = { .open = my_open, .send = my_send, .recv = my_recv, ... };

    // 3. Init security
    osp_sec_context_t sec;
    osp_sec_context_init(&sec, OSP_SUITE_0, OSP_MECH_HLS_GMAC, my_system_title);
    sec.guek_len = 16;
    memcpy(sec.guek, my_guek, 16);
    sec.gak_len = 16;
    memcpy(sec.gak, my_gak, 16);

    // 4. Init server
    osp_server_t server;
    osp_server_init(&server, &transport, OSP_FRAMING_WRAPPER);
    osp_server_set_security(&server, &sec);

    // 5. Register IC objects
    osp_ic_data_t data_ic;
    osp_ic_data_init(&data_ic, (osp_obis_t){0, 0, 1, 0, 0, 255});
    data_ic.value = osp_val_u32(42);
    osp_server_register(&server, osp_ic_data_class(), &data_ic);

    // 6. Run server loop
    while (1) {
        osp_server_accept(&server, 1000);
    }
}
```
