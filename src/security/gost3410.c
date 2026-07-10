/**
 * GOST R 34.10-2018-256 (paramSetB) for DLMS HLS mechanism 10.
 * Curve OID 1.2.643.7.1.2.1.1.2; Streebog-256 message digest.
 */
#include "gost_crypto.h"
#include <string.h>

typedef struct {
	uint64_t w[4]; /* little-endian */
} fe;

static const fe FE_P = {{0xFFFFFFFFFFFFFD97ULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL}};
static const fe FE_A = {{0xFFFFFFFFFFFFFD94ULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL}};
static const fe FE_B = {{0x00000000000000A6ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL}};
static const fe FE_Q = {{0x45841B09B761B893ULL, 0x6C611070995AD100ULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL}};
static const fe FE_GX = {{1, 0, 0, 0}};
static const fe FE_GY = {{0x22ACC99C9E9F1E14ULL, 0x35294F2DDF23E3B1ULL, 0x27DF505A453F2B76ULL, 0x8D91E471E0989CDAULL}};
static const fe FE_2_256_MOD_P = {{617ULL, 0, 0, 0}}; /* 2^256 mod p */
static const fe FE_2_256_MOD_Q = {{0xBA7BE4F6489E476DULL, 0x939EEF8F66A52EFFULL, 0, 0}}; /* 2^256 mod q */

static int fe_cmp(const fe *a, const fe *b) {
	for (int i = 3; i >= 0; i--) {
		if (a->w[i] > b->w[i]) {
			return 1;
		}
		if (a->w[i] < b->w[i]) {
			return -1;
		}
	}
	return 0;
}

static bool fe_is_zero(const fe *a) {
	return a->w[0] == 0 && a->w[1] == 0 && a->w[2] == 0 && a->w[3] == 0;
}

static void fe_from_le(fe *r, const uint8_t in[32]) {
	r->w[0] = ((uint64_t)in[0]) | ((uint64_t)in[1] << 8) | ((uint64_t)in[2] << 16) | ((uint64_t)in[3] << 24) |
	          ((uint64_t)in[4] << 32) | ((uint64_t)in[5] << 40) | ((uint64_t)in[6] << 48) | ((uint64_t)in[7] << 56);
	r->w[1] = ((uint64_t)in[8]) | ((uint64_t)in[9] << 8) | ((uint64_t)in[10] << 16) | ((uint64_t)in[11] << 24) |
	          ((uint64_t)in[12] << 32) | ((uint64_t)in[13] << 40) | ((uint64_t)in[14] << 48) | ((uint64_t)in[15] << 56);
	r->w[2] = ((uint64_t)in[16]) | ((uint64_t)in[17] << 8) | ((uint64_t)in[18] << 16) | ((uint64_t)in[19] << 24) |
	          ((uint64_t)in[20] << 32) | ((uint64_t)in[21] << 40) | ((uint64_t)in[22] << 48) | ((uint64_t)in[23] << 56);
	r->w[3] = ((uint64_t)in[24]) | ((uint64_t)in[25] << 8) | ((uint64_t)in[26] << 16) | ((uint64_t)in[27] << 24) |
	          ((uint64_t)in[28] << 32) | ((uint64_t)in[29] << 40) | ((uint64_t)in[30] << 48) | ((uint64_t)in[31] << 56);
}

static void fe_to_le(uint8_t out[32], const fe *a) {
	for (int i = 0; i < 4; i++) {
		uint64_t v = a->w[i];
		out[i * 8 + 0] = (uint8_t)(v);
		out[i * 8 + 1] = (uint8_t)(v >> 8);
		out[i * 8 + 2] = (uint8_t)(v >> 16);
		out[i * 8 + 3] = (uint8_t)(v >> 24);
		out[i * 8 + 4] = (uint8_t)(v >> 32);
		out[i * 8 + 5] = (uint8_t)(v >> 40);
		out[i * 8 + 6] = (uint8_t)(v >> 48);
		out[i * 8 + 7] = (uint8_t)(v >> 56);
	}
}

static void fe_copy(fe *r, const fe *a) {
	memcpy(r, a, sizeof(*r));
}

static void fe_mod(fe *r, const fe *m) {
	while (fe_cmp(r, m) >= 0) {
		uint64_t borrow = 0;
		for (int i = 0; i < 4; i++) {
			uint64_t diff = r->w[i] - m->w[i] - borrow;
			borrow = (r->w[i] < m->w[i] + borrow) ? 1 : 0;
			r->w[i] = diff;
		}
	}
}

static void fe_mod_p(fe *r) {
	fe_mod(r, &FE_P);
}

static void fe_mod_q(fe *r) {
	fe_mod(r, &FE_Q);
}

static void sc_add(fe *r, const fe *a, const fe *b) {
	uint64_t carry = 0;
	for (int i = 0; i < 4; i++) {
		__uint128_t sum = (__uint128_t)a->w[i] + b->w[i] + carry;
		r->w[i] = (uint64_t)sum;
		carry = (uint64_t)(sum >> 64);
	}
	if (carry) {
		carry = 0;
		for (int i = 0; i < 4; i++) {
			__uint128_t sum = (__uint128_t)r->w[i] + FE_2_256_MOD_Q.w[i] + carry;
			r->w[i] = (uint64_t)sum;
			carry = (uint64_t)(sum >> 64);
		}
	}
	fe_mod_q(r);
}

static void sc_sub(fe *r, const fe *a, const fe *b) {
	fe t = *b;
	if (fe_cmp(a, b) < 0) {
		uint64_t borrow = 0;
		for (int i = 0; i < 4; i++) {
			uint64_t diff = FE_Q.w[i] - t.w[i] - borrow;
			borrow = (FE_Q.w[i] < t.w[i] + borrow) ? 1 : 0;
			t.w[i] = diff;
		}
		sc_add(r, a, &t);
	} else {
		uint64_t borrow = 0;
		for (int i = 0; i < 4; i++) {
			uint64_t diff = a->w[i] - t.w[i] - borrow;
			borrow = (a->w[i] < t.w[i] + borrow) ? 1 : 0;
			r->w[i] = diff;
		}
	}
}

static void mul512(uint64_t t[8], const fe *a, const fe *b) {
	memset(t, 0, sizeof(uint64_t) * 8);
	for (int i = 0; i < 4; i++) {
		uint64_t carry = 0;
		for (int j = 0; j < 4; j++) {
			__uint128_t p = (__uint128_t)a->w[i] * b->w[j] + t[i + j] + carry;
			t[i + j] = (uint64_t)p;
			carry = (uint64_t)(p >> 64);
		}
		t[i + 4] += carry;
	}
}

static void fe_add_raw(fe *r, const fe *a, const fe *b) {
	uint64_t carry = 0;
	for (int i = 0; i < 4; i++) {
		__uint128_t sum = (__uint128_t)a->w[i] + b->w[i] + carry;
		r->w[i] = (uint64_t)sum;
		carry = (uint64_t)(sum >> 64);
	}
}

static void reduce512(fe *r, uint64_t t[8], const fe *r256) {
	for (int iter = 0; iter < 8; iter++) {
		fe hi = {{t[4], t[5], t[6], t[7]}};
		if (fe_is_zero(&hi)) {
			break;
		}
		uint64_t fold[8];
		mul512(fold, &hi, r256);
		t[4] = t[5] = t[6] = t[7] = 0;
		uint64_t carry = 0;
		for (int i = 0; i < 8; i++) {
			__uint128_t sum = (__uint128_t)t[i] + fold[i] + carry;
			t[i] = (uint64_t)sum;
			carry = (uint64_t)(sum >> 64);
		}
		if (carry) {
			t[4] += carry;
		}
	}
	*r = (fe){{t[0], t[1], t[2], t[3]}};
}

static void sc_mul(fe *r, const fe *a, const fe *b) {
	uint64_t t[8];
	mul512(t, a, b);
	reduce512(r, t, &FE_2_256_MOD_Q);
	fe_mod_q(r);
}

static void sc_pow(fe *r, const fe *base, const fe *exp) {
	fe result = {{1, 0, 0, 0}};
	fe b;
	fe_copy(&b, base);
	for (int limb = 3; limb >= 0; limb--) {
		for (int bit = 63; bit >= 0; bit--) {
			fe tmp;
			sc_mul(&tmp, &result, &result);
			result = tmp;
			if ((exp->w[limb] >> bit) & 1) {
				sc_mul(&tmp, &result, &b);
				result = tmp;
			}
		}
	}
	*r = result;
}

static void sc_inv(fe *r, const fe *a) {
	fe exp = {{0x45841B09B761B891ULL, 0x6C611070995AD100ULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL}}; /* q-2 */
	sc_pow(r, a, &exp);
}

static void fe_add(fe *r, const fe *a, const fe *b) {
	uint64_t carry = 0;
	for (int i = 0; i < 4; i++) {
		__uint128_t sum = (__uint128_t)a->w[i] + b->w[i] + carry;
		r->w[i] = (uint64_t)sum;
		carry = (uint64_t)(sum >> 64);
	}
	if (carry) {
		carry = FE_2_256_MOD_P.w[0];
		for (int i = 0; i < 4 && carry; i++) {
			__uint128_t sum = (__uint128_t)r->w[i] + carry;
			r->w[i] = (uint64_t)sum;
			carry = (uint64_t)(sum >> 64);
		}
	}
	fe_mod_p(r);
}

static void fe_sub(fe *r, const fe *a, const fe *b) {
	fe t = *b;
	if (fe_cmp(a, b) < 0) {
		uint64_t borrow = 0;
		for (int i = 0; i < 4; i++) {
			uint64_t diff = FE_P.w[i] - t.w[i] - borrow;
			borrow = (FE_P.w[i] < t.w[i] + borrow) ? 1 : 0;
			t.w[i] = diff;
		}
		fe_add(r, a, &t);
	} else {
		uint64_t borrow = 0;
		for (int i = 0; i < 4; i++) {
			uint64_t diff = a->w[i] - t.w[i] - borrow;
			borrow = (a->w[i] < t.w[i] + borrow) ? 1 : 0;
			r->w[i] = diff;
		}
	}
}

static void fe_fold_mod_p(fe *r, const uint64_t t[8]) {
	uint64_t buf[8];
	memcpy(buf, t, sizeof(buf));
	reduce512(r, buf, &FE_2_256_MOD_P);
	fe_mod_p(r);
}

static void fe_mul(fe *r, const fe *a, const fe *b) {
	uint64_t t[8];
	mul512(t, a, b);
	fe_fold_mod_p(r, t);
}

static void fe_pow(fe *r, const fe *base, const fe *exp) {
	fe result = {{1, 0, 0, 0}};
	fe b;
	fe_copy(&b, base);
	for (int limb = 3; limb >= 0; limb--) {
		for (int bit = 63; bit >= 0; bit--) {
			fe tmp;
			fe_mul(&tmp, &result, &result);
			result = tmp;
			if ((exp->w[limb] >> bit) & 1) {
				fe_mul(&tmp, &result, &b);
				result = tmp;
			}
		}
	}
	*r = result;
}

static void fe_inv(fe *r, const fe *a) {
	fe exp = {{0xFFFFFFFFFFFFFD95ULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL}};
	fe_pow(r, a, &exp);
}

typedef struct {
	bool inf;
	fe x;
	fe y;
} ec_point;

static void point_double(ec_point *r, const ec_point *p) {
	if (p->inf || fe_is_zero(&p->y)) {
		r->inf = true;
		return;
	}
	fe lambda, t;
	fe_mul(&t, &p->x, &p->x);
	fe tmp;
	fe_add(&tmp, &t, &t);
	fe_add(&tmp, &tmp, &t);
	fe_add(&lambda, &tmp, &FE_A);
	fe two_y;
	fe_add(&two_y, &p->y, &p->y);
	fe inv_y;
	fe_inv(&inv_y, &two_y);
	fe_mul(&lambda, &lambda, &inv_y);

	fe_mul(&t, &lambda, &lambda);
	fe_sub(&r->x, &t, &p->x);
	fe_sub(&r->x, &r->x, &p->x);
	fe_sub(&t, &p->x, &r->x);
	fe_mul(&t, &lambda, &t);
	fe_sub(&r->y, &t, &p->y);
	r->inf = false;
}

static void point_add(ec_point *r, const ec_point *a, const ec_point *b) {
	if (a->inf) {
		*r = *b;
		return;
	}
	if (b->inf) {
		*r = *a;
		return;
	}
	if (fe_cmp(&a->x, &b->x) == 0) {
		if (fe_cmp(&a->y, &b->y) == 0) {
			point_double(r, a);
			return;
		}
		r->inf = true;
		return;
	}
	fe lambda, t, inv;
	fe_sub(&t, &b->y, &a->y);
	fe_sub(&inv, &b->x, &a->x);
	fe_inv(&inv, &inv);
	fe_mul(&lambda, &t, &inv);
	fe_mul(&t, &lambda, &lambda);
	fe_sub(&r->x, &t, &a->x);
	fe_sub(&r->x, &r->x, &b->x);
	fe_sub(&t, &a->x, &r->x);
	fe_mul(&t, &lambda, &t);
	fe_sub(&r->y, &t, &a->y);
	r->inf = false;
}

static void point_mul(ec_point *r, const fe *k, const ec_point *p) {
	ec_point result = {.inf = true};
	ec_point addend = *p;
	fe n;
	fe_copy(&n, k);
	for (;;) {
		if (n.w[0] & 1) {
			ec_point tmp;
			point_add(&tmp, &result, &addend);
			result = tmp;
		}
		uint64_t carry = 0;
		for (int i = 3; i >= 0; i--) {
			uint64_t old = n.w[i];
			n.w[i] = (old >> 1) | (carry << 63);
			carry = old & 1;
		}
		if (fe_is_zero(&n)) {
			break;
		}
		ec_point tmp;
		point_double(&tmp, &addend);
		addend = tmp;
	}
	*r = result;
}

static int hash_to_fe(fe *r, const uint8_t *msg, uint32_t msg_len) {
	uint8_t h[32];
	if (osp_gost_streebog256(msg, msg_len, h) != 0) {
		return -1;
	}
	fe_from_le(r, h);
	fe_mod_q(r);
	if (fe_is_zero(r)) {
		r->w[0] = 1;
	}
	return 0;
}

static bool on_curve(const fe *x, const fe *y) {
	fe lhs, rhs, t;
	fe_mul(&lhs, y, y);
	fe_mul(&rhs, x, x);
	fe_mul(&rhs, &rhs, x);
	fe_mul(&t, &FE_A, x);
	fe_add(&rhs, &rhs, &t);
	fe_add(&rhs, &rhs, &FE_B);
	return fe_cmp(&lhs, &rhs) == 0;
}

int osp_gost3410_public_key(const uint8_t d[32], uint8_t pk[64]) {
	if (!d || !pk) {
		return -1;
	}
	fe sk;
	fe_from_le(&sk, d);
	if (fe_is_zero(&sk) || fe_cmp(&sk, &FE_Q) >= 0) {
		return -1;
	}
	ec_point g = {.inf = false, .x = FE_GX, .y = FE_GY};
	ec_point q;
	point_mul(&q, &sk, &g);
	if (q.inf) {
		return -1;
	}
	fe_to_le(pk, &q.x);
	fe_to_le(pk + 32, &q.y);
	return 0;
}

int osp_gost3410_sign_with_k(const uint8_t d[32], const uint8_t *msg, uint32_t msg_len, const uint8_t k[32], uint8_t sig[64]) {
	if (!d || !msg || !sig || !k) {
		return -1;
	}
	fe sk, e, k_fe, r_fe, s_fe;
	fe_from_le(&sk, d);
	fe_from_le(&k_fe, k);
	if (fe_is_zero(&sk) || fe_cmp(&sk, &FE_Q) >= 0 || fe_is_zero(&k_fe) || fe_cmp(&k_fe, &FE_Q) >= 0) {
		return -1;
	}
	if (hash_to_fe(&e, msg, msg_len) != 0) {
		return -1;
	}
	ec_point g = {.inf = false, .x = FE_GX, .y = FE_GY};
	ec_point c;
	point_mul(&c, &k_fe, &g);
	if (c.inf) {
		return -1;
	}
	fe_copy(&r_fe, &c.x);
	fe_mod_q(&r_fe);
	if (fe_is_zero(&r_fe)) {
		return -1;
	}
	fe tmp1, tmp2;
	sc_mul(&tmp1, &r_fe, &sk);
	sc_mul(&tmp2, &k_fe, &e);
	sc_add(&s_fe, &tmp1, &tmp2);
	if (fe_is_zero(&s_fe)) {
		return -1;
	}
	fe_to_le(sig, &r_fe);
	fe_to_le(sig + 32, &s_fe);
	return 0;
}

int osp_gost3410_sign(const uint8_t d[32], const uint8_t *msg, uint32_t msg_len, uint8_t sig[64]) {
	if (!d || !msg || !sig) {
		return -1;
	}
	uint8_t kb[64];
	memcpy(kb, d, 32);
	if (msg_len > 32) {
		osp_gost_streebog256(msg, msg_len, kb + 32);
	} else {
		memcpy(kb + 32, msg, msg_len);
		osp_gost_streebog256(kb, 32 + msg_len, kb + 32);
	}
	return osp_gost3410_sign_with_k(d, msg, msg_len, kb + 32, sig);
}

int osp_gost3410_verify(const uint8_t pk[64], const uint8_t *msg, uint32_t msg_len, const uint8_t sig[64]) {
	if (!pk || !msg || !sig) {
		return -1;
	}
	fe r, s, qx, qy, e, v, z1, z2;
	fe_from_le(&r, sig);
	fe_from_le(&s, sig + 32);
	fe_from_le(&qx, pk);
	fe_from_le(&qy, pk + 32);
	if (fe_is_zero(&r) || fe_cmp(&r, &FE_Q) >= 0 || fe_is_zero(&s) || fe_cmp(&s, &FE_Q) >= 0) {
		return -1;
	}
	if (fe_cmp(&qx, &FE_P) >= 0 || fe_cmp(&qy, &FE_P) >= 0 || !on_curve(&qx, &qy)) {
		return -1;
	}
	if (hash_to_fe(&e, msg, msg_len) != 0) {
		return -1;
	}
	sc_inv(&v, &e);
	sc_mul(&z1, &s, &v);
	fe rv;
	sc_mul(&rv, &r, &v);
	sc_sub(&z2, &FE_Q, &rv);

	ec_point g = {.inf = false, .x = FE_GX, .y = FE_GY};
	ec_point q = {.inf = false, .x = qx, .y = qy};
	ec_point p1, p2, sum;
	point_mul(&p1, &z1, &g);
	point_mul(&p2, &z2, &q);
	point_add(&sum, &p1, &p2);
	if (sum.inf) {
		return -1;
	}
	fe xr;
	fe_copy(&xr, &sum.x);
	fe_mod_q(&xr);
	return (fe_cmp(&xr, &r) == 0) ? 0 : -1;
}
