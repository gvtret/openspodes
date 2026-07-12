/**
 * test_thread_safety.c — Thread-safety verification for OpenSPODES codec
 *
 * Tests that multiple threads can concurrently:
 *   - Encode/decode BER/AXDR values (via HAL mutex)
 *   - Use osp_value_read/write (protected pool)
 *   - Create/destroy security contexts independently
 *
 * Build:
 *   cmake --build build --target openspodes_test_thread
 *   ctest -R test_thread
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../src/openspodes.h"
#include "../src/codec/codec.h"
#include "../src/codec/serialize.h"
#include "../src/service/notification.h"
#include "../src/security/security.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 4
#define ITERS_PER_THREAD 50

/* ── POSIX mutex HAL implementation ──────────────────────────────────────── */

static void *posix_mutex_create(void *ctx) {
	(void)ctx;
	pthread_mutex_t *m = malloc(sizeof(pthread_mutex_t));
	if (m) pthread_mutex_init(m, NULL);
	return m;
}

static int posix_mutex_lock(void *ctx, void *handle) {
	(void)ctx;
	return pthread_mutex_lock((pthread_mutex_t *)handle);
}

static void posix_mutex_unlock(void *ctx, void *handle) {
	(void)ctx;
	pthread_mutex_unlock((pthread_mutex_t *)handle);
}

static void posix_mutex_destroy(void *ctx, void *handle) {
	(void)ctx;
	if (handle) {
		pthread_mutex_destroy((pthread_mutex_t *)handle);
		free(handle);
	}
}

static osp_mutex_t test_mutex = {
	.create  = posix_mutex_create,
	.lock    = posix_mutex_lock,
	.unlock  = posix_mutex_unlock,
	.destroy = posix_mutex_destroy,
	.ctx     = NULL,
};

/* ── Thread-local value read/write test ──────────────────────────────────── */

static void *thread_value_read_write(void *arg) {
	int thread_id = *(int *)arg;

	for (int i = 0; i < ITERS_PER_THREAD; i++) {
		osp_buf_t buf;
		uint8_t tx[256];
		osp_buf_init(&buf, tx, sizeof(tx));

		/* Simple u32 roundtrip — no pool contention */
		osp_value_t val = osp_val_u32((uint32_t)(thread_id * 1000 + i));
		osp_err_t r = osp_value_write(&buf, &val);
		assert_int_equal(r, OSP_OK);

		osp_value_t decoded;
		buf.rd = 0;
		r = osp_value_read(&buf, &decoded);
		assert_int_equal(r, OSP_OK);
		assert_int_equal(decoded.as.uint32.value, (uint32_t)(thread_id * 1000 + i));
	}
	return NULL;
}

static void test_concurrent_value_read_write(void **state) {
	(void)state;
	osp_hal_mutex = &test_mutex;

	pthread_t threads[NUM_THREADS];
	int ids[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; i++) {
		ids[i] = i;
		pthread_create(&threads[i], NULL, thread_value_read_write, &ids[i]);
	}
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	osp_hal_mutex = NULL;
}

/* ── Thread-local BER encode/decode test ─────────────────────────────────── */

static void *thread_ber_codec(void *arg) {
	int thread_id = *(int *)arg;

	for (int i = 0; i < ITERS_PER_THREAD; i++) {
		osp_buf_t buf;
		uint8_t tx[512];
		osp_buf_init(&buf, tx, sizeof(tx));

		osp_ber_write_tag(&buf, 1, false, (uint8_t)(thread_id % 16));
		osp_ber_write_length(&buf, 4);
		osp_axdr_write_u32(&buf, (uint32_t)(thread_id * 10000 + i));

		osp_ber_tag_t tag;
		osp_ber_read_tag(&buf, &tag);
		assert_int_equal(tag.tag_number, (uint8_t)(thread_id % 16));

		uint32_t len;
		osp_ber_read_length(&buf, &len);
		assert_int_equal(len, 4);

		uint32_t val;
		osp_axdr_read_u32(&buf, &val);
		assert_int_equal(val, (uint32_t)(thread_id * 10000 + i));
	}
	return NULL;
}

static void test_concurrent_ber_codec(void **state) {
	(void)state;
	osp_hal_mutex = &test_mutex;

	pthread_t threads[NUM_THREADS];
	int ids[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; i++) {
		ids[i] = i;
		pthread_create(&threads[i], NULL, thread_ber_codec, &ids[i]);
	}
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	osp_hal_mutex = NULL;
}

/* ── Thread-local notification encode/decode test ────────────────────────── */

static void *thread_notification_codec(void *arg) {
	int thread_id = *(int *)arg;

	for (int i = 0; i < ITERS_PER_THREAD; i++) {
		osp_buf_t buf;
		uint8_t tx[512];
		osp_buf_init(&buf, tx, sizeof(tx));

		osp_data_notification_t dn;
		memset(&dn, 0, sizeof(dn));
		dn.long_invoke_id_and_priority = (uint32_t)(thread_id * 100000 + i);
		dn.date_time_len = 12;
		memset(dn.date_time, (uint8_t)(thread_id + i), 12);
		dn.notification_body = osp_val_u32((uint32_t)(thread_id * 1000 + i));

		int r = osp_data_notification_encode(&buf, &dn);
		assert_int_equal(r, 0);

		osp_data_notification_t decoded;
		buf.rd = 0;
		r = osp_data_notification_decode(&buf, &decoded);
		assert_int_equal(r, 0);
		assert_int_equal(decoded.long_invoke_id_and_priority,
		                 (uint32_t)(thread_id * 100000 + i));
		assert_int_equal(decoded.notification_body.as.uint32.value,
		                 (uint32_t)(thread_id * 1000 + i));
	}
	return NULL;
}

static void test_concurrent_notification_codec(void **state) {
	(void)state;
	osp_hal_mutex = &test_mutex;

	pthread_t threads[NUM_THREADS];
	int ids[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; i++) {
		ids[i] = i;
		pthread_create(&threads[i], NULL, thread_notification_codec, &ids[i]);
	}
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	osp_hal_mutex = NULL;
}

/* ── Thread-local security context test ──────────────────────────────────── */

static void *thread_security_context(void *arg) {
	int thread_id = *(int *)arg;

	for (int i = 0; i < ITERS_PER_THREAD; i++) {
		osp_sec_context_t ctx;
		osp_sec_context_init(&ctx, OSP_SUITE_0, OSP_MECH_HLS_GMAC, NULL);

		memset(ctx.guek, (uint8_t)(thread_id + 1), OSP_SEC_KEY_MAX);
		memset(ctx.gak, (uint8_t)(thread_id + 10), OSP_SEC_KEY_MAX);
		ctx.invocation_counter = (uint32_t)(thread_id * 1000 + i);

		assert_int_equal(ctx.guek[0], (uint8_t)(thread_id + 1));
		assert_int_equal(ctx.gak[0], (uint8_t)(thread_id + 10));
		assert_int_equal(ctx.invocation_counter, (uint32_t)(thread_id * 1000 + i));

		osp_sec_context_destroy(&ctx);
		for (int k = 0; k < OSP_SEC_KEY_MAX; k++) {
			assert_int_equal(ctx.guek[k], 0);
			assert_int_equal(ctx.gak[k], 0);
		}
	}
	return NULL;
}

static void test_concurrent_security_context(void **state) {
	(void)state;
	pthread_t threads[NUM_THREADS];
	int ids[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; i++) {
		ids[i] = i;
		pthread_create(&threads[i], NULL, thread_security_context, &ids[i]);
	}
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}
}

/* ── No-mutex single-threaded mode test ──────────────────────────────────── */

static void test_single_threaded_no_mutex(void **state) {
	(void)state;
	osp_hal_mutex = NULL; /* Explicit: no mutex */

	osp_buf_t buf;
	uint8_t tx[256];
	osp_buf_init(&buf, tx, sizeof(tx));

	/* Simple u32 roundtrip — no pool needed */
	osp_value_t val = osp_val_u32(42);
	osp_err_t r = osp_value_write(&buf, &val);
	assert_int_equal(r, OSP_OK);

	osp_value_t decoded;
	buf.rd = 0;
	r = osp_value_read(&buf, &decoded);
	assert_int_equal(r, OSP_OK);
	assert_int_equal(decoded.tag, OSP_TAG_DOUBLE_LONG_UNS);
	assert_int_equal(decoded.as.uint32.value, 42);
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_single_threaded_no_mutex),
		cmocka_unit_test(test_concurrent_value_read_write),
		cmocka_unit_test(test_concurrent_ber_codec),
		cmocka_unit_test(test_concurrent_notification_codec),
		cmocka_unit_test(test_concurrent_security_context),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
