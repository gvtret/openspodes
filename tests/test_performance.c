/**
 * test_performance.c — Performance benchmarks for OpenSPODES
 *
 * Measures throughput and latency for key operations:
 * - BER/AXDR codec encode/decode
 * - Value serialization roundtrip
 * - HDLC frame/deframe
 * - Glo-ciphering protect/unprotect
 * - IC serialize/deserialize
 *
 * Build:
 *   cmake --build build --target openspodes_bench
 *   ./build/openspodes_bench
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "openspodes.h"
#include "codec/codec.h"
#include "codec/serialize.h"
#include "codec/structures.h"
#include "codec/ic_serialize.h"
#include "transport/transport.h"
#include "security/security.h"
#include "ic/data.h"
#include "ic/register.h"
#include "ic/clock.h"
#include "mock_crypto.h"

#define BENCH_ITERATIONS 1000
#define BENCH_WARMUP 100

static double time_diff_ms(struct timespec *start, struct timespec *end) {
	return (end->tv_sec - start->tv_sec) * 1000.0 + (end->tv_nsec - start->tv_nsec) / 1000000.0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BER/AXDR Codec Benchmarks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void bench_axdr_u32_roundtrip(void) {
	osp_buf_t buf;
	uint8_t tx[64];
	struct timespec start, end;

	/* Warmup */
	for (int i = 0; i < BENCH_WARMUP; i++) {
		osp_buf_init(&buf, tx, sizeof(tx));
		osp_axdr_write_u32(&buf, 0x12345678);
		buf.rd = 0;
		uint32_t val;
		osp_axdr_read_u32(&buf, &val);
	}

	/* Benchmark */
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		osp_buf_init(&buf, tx, sizeof(tx));
		osp_axdr_write_u32(&buf, 0x12345678);
		buf.rd = 0;
		uint32_t val;
		osp_axdr_read_u32(&buf, &val);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	double ms = time_diff_ms(&start, &end);
	double ops_per_sec = BENCH_ITERATIONS / (ms / 1000.0);
	printf("  AXDR u32 roundtrip:  %8.1f us/op  (%.0f ops/sec)\n", ms * 1000 / BENCH_ITERATIONS, ops_per_sec);
}

static void bench_axdr_octet_string(void) {
	osp_buf_t buf;
	uint8_t tx[300];
	uint8_t data[64];
	memset(data, 0xAB, sizeof(data));
	struct timespec start, end;

	/* Warmup */
	for (int i = 0; i < BENCH_WARMUP; i++) {
		osp_buf_init(&buf, tx, sizeof(tx));
		osp_axdr_write_octet_string(&buf, data, 64);
		buf.rd = 0;
		uint8_t out[64];
		uint32_t out_len;
		osp_axdr_read_octet_string(&buf, out, 64, &out_len);
	}

	/* Benchmark */
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		osp_buf_init(&buf, tx, sizeof(tx));
		osp_axdr_write_octet_string(&buf, data, 64);
		buf.rd = 0;
		uint8_t out[64];
		uint32_t out_len;
		osp_axdr_read_octet_string(&buf, out, 64, &out_len);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	double ms = time_diff_ms(&start, &end);
	double ops_per_sec = BENCH_ITERATIONS / (ms / 1000.0);
	printf("  AXDR octet string:   %8.1f us/op  (%.0f ops/sec)\n", ms * 1000 / BENCH_ITERATIONS, ops_per_sec);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Value Serialization Benchmarks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void bench_value_roundtrip_u32(void) {
	osp_buf_t buf;
	uint8_t tx[64];
	struct timespec start, end;

	/* Warmup */
	for (int i = 0; i < BENCH_WARMUP; i++) {
		osp_buf_init(&buf, tx, sizeof(tx));
		osp_value_t val = osp_val_u32(0xDEADBEEF);
		osp_value_write(&buf, &val);
		buf.rd = 0;
		osp_value_t decoded;
		osp_value_read(&buf, &decoded);
	}

	/* Benchmark */
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		osp_buf_init(&buf, tx, sizeof(tx));
		osp_value_t val = osp_val_u32(0xDEADBEEF);
		osp_value_write(&buf, &val);
		buf.rd = 0;
		osp_value_t decoded;
		osp_value_read(&buf, &decoded);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	double ms = time_diff_ms(&start, &end);
	double ops_per_sec = BENCH_ITERATIONS / (ms / 1000.0);
	printf("  Value u32 roundtrip: %8.1f us/op  (%.0f ops/sec)\n", ms * 1000 / BENCH_ITERATIONS, ops_per_sec);
}

static void bench_value_roundtrip_structure(void) {
	osp_buf_t buf;
	uint8_t tx[256];
	osp_value_t items[3];
	struct timespec start, end;

	/* Warmup */
	for (int i = 0; i < BENCH_WARMUP; i++) {
		osp_buf_init(&buf, tx, sizeof(tx));
		items[0] = osp_val_u32(42);
		items[1] = osp_val_u16(123);
		items[2] = osp_val_u8(7);
		osp_value_t val = osp_val_u32(0);
		val.tag = OSP_TAG_STRUCTURE;
		val.as.structure.elements.items = items;
		val.as.structure.elements.count = 3;
		val.as.structure.elements.capacity = 3;
		osp_value_write(&buf, &val);
		buf.rd = 0;
		osp_value_t decoded;
		osp_value_read(&buf, &decoded);
	}

	/* Benchmark */
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		osp_buf_init(&buf, tx, sizeof(tx));
		items[0] = osp_val_u32(42);
		items[1] = osp_val_u16(123);
		items[2] = osp_val_u8(7);
		osp_value_t val = osp_val_u32(0);
		val.tag = OSP_TAG_STRUCTURE;
		val.as.structure.elements.items = items;
		val.as.structure.elements.count = 3;
		val.as.structure.elements.capacity = 3;
		osp_value_write(&buf, &val);
		val.as.structure.elements.items[1] = osp_val_u16(123);
		val.as.structure.elements.items[2] = osp_val_u8(7);
		osp_value_write(&buf, &val);
		buf.rd = 0;
		osp_value_t decoded;
		osp_value_read(&buf, &decoded);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	double ms = time_diff_ms(&start, &end);
	double ops_per_sec = BENCH_ITERATIONS / (ms / 1000.0);
	printf("  Value struct roundtrip: %5.1f us/op  (%.0f ops/sec)\n", ms * 1000 / BENCH_ITERATIONS, ops_per_sec);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  HDLC Frame Benchmarks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void bench_hdlc_frame_roundtrip(void) {
	osp_hdlc_frame_t frame;
	uint8_t out[512];
	uint32_t out_len;
	struct timespec start, end;

	/* Setup frame */
	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 0x42, 1);
	osp_hdlc_address_init(&frame.source, 0x41, 1);
	frame.control.type = OSP_HDLC_TYPE_I;
	frame.control.send_seq = 0;
	frame.control.recv_seq = 0;
	frame.control.poll_final = true;
	frame.info_len = 32;
	memset(frame.info, 0xAB, 32);

	/* Warmup */
	for (int i = 0; i < BENCH_WARMUP; i++) {
		osp_hdlc_frame(&frame, out, sizeof(out), &out_len);
		osp_hdlc_deframe(out, out_len, &frame);
	}

	/* Benchmark */
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		osp_hdlc_frame(&frame, out, sizeof(out), &out_len);
		osp_hdlc_deframe(out, out_len, &frame);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	double ms = time_diff_ms(&start, &end);
	double ops_per_sec = BENCH_ITERATIONS / (ms / 1000.0);
	printf("  HDLC frame roundtrip: %4.1f us/op  (%.0f ops/sec)\n", ms * 1000 / BENCH_ITERATIONS, ops_per_sec);
}

static void bench_hdlc_crc(void) {
	uint8_t data[128];
	memset(data, 0xAB, sizeof(data));
	struct timespec start, end;

	/* Warmup */
	for (int i = 0; i < BENCH_WARMUP; i++) {
		osp_hdlc_fcs16(data, sizeof(data));
	}

	/* Benchmark */
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		osp_hdlc_fcs16(data, sizeof(data));
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	double ms = time_diff_ms(&start, &end);
	double ops_per_sec = BENCH_ITERATIONS / (ms / 1000.0);
	printf("  HDLC CRC-16 (128B):   %4.1f us/op  (%.0f ops/sec)\n", ms * 1000 / BENCH_ITERATIONS, ops_per_sec);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Security Benchmarks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void bench_glo_protect(void) {
	printf("  Glo protect:         SKIPPED (requires OpenSSL + running test suite)\n");
}

static void bench_ic_serialize(void) {
	osp_buf_t buf;
	uint8_t tx[512];
	osp_ic_data_t data_ic;
	osp_ic_data_init(&data_ic, (osp_obis_t){0, 0, 1, 8, 0, 255});
	data_ic.value = osp_val_u32(123456);
	struct timespec start, end;

	/* Warmup */
	for (int i = 0; i < BENCH_WARMUP; i++) {
		osp_buf_init(&buf, tx, sizeof(tx));
		osp_ic_serialize(osp_ic_data_class(), &data_ic, &buf);
		buf.rd = 0;
		osp_ic_deserialize(osp_ic_data_class(), &data_ic, &buf);
	}

	/* Benchmark */
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		osp_buf_init(&buf, tx, sizeof(tx));
		osp_ic_serialize(osp_ic_data_class(), &data_ic, &buf);
		buf.rd = 0;
		osp_ic_deserialize(osp_ic_data_class(), &data_ic, &buf);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	double ms = time_diff_ms(&start, &end);
	double ops_per_sec = BENCH_ITERATIONS / (ms / 1000.0);
	printf("  IC Data roundtrip:   %6.1f us/op  (%.0f ops/sec)\n", ms * 1000 / BENCH_ITERATIONS, ops_per_sec);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
	printf("OpenSPODES Performance Benchmarks\n");
	printf("=================================\n\n");

	printf("Codec:\n");
	bench_axdr_u32_roundtrip();
	bench_axdr_octet_string();

	printf("\nValue Serialization:\n");
	bench_value_roundtrip_u32();
	bench_value_roundtrip_structure();

	printf("\nHDLC:\n");
	bench_hdlc_frame_roundtrip();
	bench_hdlc_crc();

	printf("\nSecurity:\n");
	bench_glo_protect();

	printf("\nIC:\n");
	bench_ic_serialize();

	printf("\n=================================\n");
	printf("Iterations: %d (+ %d warmup)\n", BENCH_ITERATIONS, BENCH_WARMUP);

	return 0;
}
