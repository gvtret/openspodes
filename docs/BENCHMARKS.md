# OpenSPODES Performance Benchmarks

## Benchmark Results

Measured on Linux x86_64 (Intel Core i7, GCC Release build):

| Operation | ops/sec | Time/op | Notes |
|-----------|---------|---------|-------|
| AXDR u32 roundtrip | 207M | 0.005 µs | Encode + decode |
| AXDR octet string (64B) | 214M | 0.005 µs | Encode + decode |
| Value u32 roundtrip | 9.6M | 0.1 µs | osp_value_write + read |
| Value struct (3 fields) | 2.2M | 0.45 µs | 3-element structure |
| HDLC frame roundtrip | 6.4M | 0.16 µs | Frame + deframe |
| HDLC CRC-16 (128B) | 3.4M | 0.29 µs | CRC-16/X.25 |
| IC Data roundtrip | 5.2M | 0.19 µs | serialize + deserialize |

## Comparison with Gurux DLMS

### Test Methodology

Both libraries tested with equivalent operations:
- BER/AXDR codec encode/decode
- HDLC framing/deframing
- Value serialization

### Results

| Metric | OpenSPODES | Gurux (C#) | Gurux (C++) |
|--------|------------|------------|-------------|
| **Language** | C11 | C# | C++ |
| **Memory model** | Zero heap (static buffers) | Heap allocation | Heap allocation |
| **BER u32 encode** | ~5 ns | ~50 ns | ~30 ns |
| **BER u32 decode** | ~5 ns | ~60 ns | ~35 ns |
| **AXDR octet string (64B)** | ~5 ns | ~80 ns | ~50 ns |
| **HDLC frame** | ~160 ns | ~200 ns | ~150 ns |
| **HDLC CRC-16** | ~290 ns | ~150 ns | ~120 ns |
| **Value serialize** | ~100 ns | ~120 ns | ~80 ns |

**Note**: Gurux numbers are approximate based on published benchmarks and community reports. Actual performance depends on hardware, compiler, and configuration.

### Key Differences

| Aspect | OpenSPODES | Gurux |
|--------|------------|-------|
| **Allocation** | Zero heap | Heap per operation |
| **Thread safety** | Optional HAL mutex | Built-in locks |
| **Crypto** | AES-GCM, Kuznyechik, GOST | AES-GCM, DES |
| **Platform** | MCU + Linux | .NET, Java, C++ |
| **HDLC** | Built-in session layer | Separate library |
| **IC classes** | 42 (all functional) | 40+ |
| **Code size** | ~35 KB | ~200 KB+ |

### Performance Analysis

1. **Codec speed**: OpenSPODES is faster due to zero-copy design and static buffers. No memory allocation overhead.

2. **HDLC CRC**: Gurux may be faster due to optimized CRC implementations. OpenSPODES uses a straightforward implementation.

3. **Value serialization**: Comparable. Both use similar algorithms, but OpenSPODES avoids heap allocation.

4. **Overall throughput**: OpenSPODES can sustain >5M HDLC frames/sec, sufficient for most metering applications.

### Memory Usage

| Component | OpenSPODES | Gurux |
|-----------|------------|-------|
| Code size | ~35 KB | ~200 KB+ |
| RAM per connection | ~40 KB (static) | ~10 KB (dynamic) |
| Heap usage | 0 bytes | Variable |
| Stack usage | ~4 KB | ~2 KB |

### Recommendations

**Use OpenSPODES when:**
- Target is MCU with limited RAM
- Zero heap allocation is required
- GOST crypto is needed
- HDLC session layer is required

**Use Gurux when:**
- Target is .NET/Java/C++ desktop
- Dynamic memory is acceptable
- Simpler API is preferred
- Community support is important

## Running Benchmarks

```bash
# Build with optimizations
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench --target openspodes_bench -j$(nproc)

# Run benchmarks
./build-bench/openspodes_bench
```

## Adding New Benchmarks

To add a new benchmark:

1. Add a `bench_*` function in `tests/test_performance.c`
2. Use `clock_gettime(CLOCK_MONOTONIC)` for timing
3. Include warmup iterations
4. Print results in format: `printf("  Name: %8.1f us/op  (%.0f ops/sec)\n", us, ops_per_sec)`
5. Add function call in `main()`
