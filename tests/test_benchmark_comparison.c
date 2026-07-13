/**
 * test_benchmark_comparison.c — Performance comparison: OpenSPODES vs dlms-codec
 *
 * Build:
 *   cmake --build build --target openspodes_benchmark
 *   ./build/openspodes_benchmark
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "openspodes.h"
#include "codec/codec.h"
#include "codec/serialize.h"
#include "codec/structures.h"
#include "transport/transport.h"
#include "dlms_codec.h"

#define BENCH_ITERATIONS 100000
#define BENCH_WARMUP 10000

static double time_diff_ms(struct timespec *start, struct timespec *end) {
	return (end->tv_sec - start->tv_sec) * 1000.0 + (end->tv_nsec - start->tv_nsec) / 1000000.0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  OpenSPODES Benchmarks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void bench_osp_axdr_u32(void) {
	osp_buf_t buf;
	uint8_t tx[64];
	struct timespec start, end;

	for (int i = 0; i < BENCH_WARMUP; i++) {
		osp_buf_init(&buf, tx, sizeof(tx));
		osp_axdr_write_u32(&buf, 0x12345678);
		buf.rd = 0;
		uint32_t val;
		osp_axdr_read_u32(&buf, &val);
	}

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
	printf("  AXDR u32 roundtrip:  %8.2f ns/op\n", ms * 1000000 / BENCH_ITERATIONS);
}

static void bench_osp_axdr_octet_string(void) {
	osp_buf_t buf;
	uint8_t tx[300];
	uint8_t data[64];
	memset(data, 0xAB, sizeof(data));
	struct timespec start, end;

	for (int i = 0; i < BENCH_WARMUP; i++) {
		osp_buf_init(&buf, tx, sizeof(tx));
		osp_axdr_write_octet_string(&buf, data, 64);
		buf.rd = 0;
		uint8_t out[64];
		uint32_t out_len;
		osp_axdr_read_octet_string(&buf, out, 64, &out_len);
	}

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
	printf("  AXDR octet string:   %8.2f ns/op\n", ms * 1000000 / BENCH_ITERATIONS);
}

static void bench_osp_value_u32(void) {
	osp_buf_t buf;
	uint8_t tx[64];
	struct timespec start, end;

	for (int i = 0; i < BENCH_WARMUP; i++) {
		osp_buf_init(&buf, tx, sizeof(tx));
		osp_value_t val = osp_val_u32(0xDEADBEEF);
		osp_value_write(&buf, &val);
		buf.rd = 0;
		osp_value_t decoded;
		osp_value_read(&buf, &decoded);
	}

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
	printf("  Value u32 roundtrip: %8.2f ns/op\n", ms * 1000000 / BENCH_ITERATIONS);
}

static void bench_osp_hdlc_frame(void) {
	osp_hdlc_frame_t frame;
	uint8_t out[512];
	uint32_t out_len;
	struct timespec start, end;

	memset(&frame, 0, sizeof(frame));
	osp_hdlc_address_init(&frame.destination, 0x42, 1);
	osp_hdlc_address_init(&frame.source, 0x41, 1);
	frame.control.type = OSP_HDLC_TYPE_I;
	frame.control.send_seq = 0;
	frame.control.recv_seq = 0;
	frame.control.poll_final = true;
	frame.info_len = 32;
	memset(frame.info, 0xAB, 32);

	for (int i = 0; i < BENCH_WARMUP; i++) {
		osp_hdlc_frame(&frame, out, sizeof(out), &out_len);
		osp_hdlc_deframe(out, out_len, &frame);
	}

	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		osp_hdlc_frame(&frame, out, sizeof(out), &out_len);
		osp_hdlc_deframe(out, out_len, &frame);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	double ms = time_diff_ms(&start, &end);
	printf("  HDLC frame roundtrip:%8.2f ns/op\n", ms * 1000000 / BENCH_ITERATIONS);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  dlms-codec Benchmarks
 * ═══════════════════════════════════════════════════════════════════════════ */

static void bench_dlms_write_read_len(void) {
	dlms_reader r;
	dlms_writer w;
	uint8_t tx[64];
	uint32_t len;
	struct timespec start, end;

	for (int i = 0; i < BENCH_WARMUP; i++) {
		dlms_writer_init(&w, tx, sizeof(tx));
		dlms_write_len(&w, 0x12345678);
		dlms_reader_init(&r, tx, w.pos);
		dlms_read_len(&r, &len);
	}

	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		dlms_writer_init(&w, tx, sizeof(tx));
		dlms_write_len(&w, 0x12345678);
		dlms_reader_init(&r, tx, w.pos);
		dlms_read_len(&r, &len);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	double ms = time_diff_ms(&start, &end);
	printf("  Length encode/decode: %8.2f ns/op\n", ms * 1000000 / BENCH_ITERATIONS);
}

static void bench_dlms_ber_tag(void) {
	dlms_reader r;
	dlms_writer w;
	uint8_t tx[64];
	ber_tag tag = {.cls = BER_CLASS_APPLICATION, .constructed = true, .number = 0};
	ber_tag decoded;
	struct timespec start, end;

	for (int i = 0; i < BENCH_WARMUP; i++) {
		dlms_writer_init(&w, tx, sizeof(tx));
		ber_write_tag(&w, tag);
		dlms_reader_init(&r, tx, w.pos);
		ber_read_tag(&r, &decoded);
	}

	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		dlms_writer_init(&w, tx, sizeof(tx));
		ber_write_tag(&w, tag);
		dlms_reader_init(&r, tx, w.pos);
		ber_read_tag(&r, &decoded);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	double ms = time_diff_ms(&start, &end);
	printf("  BER tag roundtrip:   %8.2f ns/op\n", ms * 1000000 / BENCH_ITERATIONS);
}

static void bench_dlms_ber_length(void) {
	dlms_reader r;
	dlms_writer w;
	uint8_t tx[64];
	uint32_t len;
	struct timespec start, end;

	for (int i = 0; i < BENCH_WARMUP; i++) {
		dlms_writer_init(&w, tx, sizeof(tx));
		dlms_write_len(&w, 256);
		dlms_reader_init(&r, tx, w.pos);
		dlms_read_len(&r, &len);
	}

	clock_gettime(CLOCK_MONOTONIC, &start);
	for (int i = 0; i < BENCH_ITERATIONS; i++) {
		dlms_writer_init(&w, tx, sizeof(tx));
		dlms_write_len(&w, 256);
		dlms_reader_init(&r, tx, w.pos);
		dlms_read_len(&r, &len);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	double ms = time_diff_ms(&start, &end);
	printf("  BER length (256):    %8.2f ns/op\n", ms * 1000000 / BENCH_ITERATIONS);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
	printf("OpenSPODES vs dlms-codec Performance Comparison\n");
	printf("================================================\n\n");

	printf("OpenSPODES (v1.5.0):\n");
	bench_osp_axdr_u32();
	bench_osp_axdr_octet_string();
	bench_osp_value_u32();
	bench_osp_hdlc_frame();

	printf("\ndlms-codec (reference):\n");
	bench_dlms_write_read_len();
	bench_dlms_ber_tag();
	bench_dlms_ber_length();

	printf("\n================================================\n");
	printf("Iterations: %d (+ %d warmup)\n", BENCH_ITERATIONS, BENCH_WARMUP);
	printf("Lower is better.\n");

	return 0;
}
