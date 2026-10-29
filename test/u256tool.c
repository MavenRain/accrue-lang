/* Regression checks for the 256-bit word helpers in src/front/u256.c: the
   exact 512-bit product, the floor of A * B / D (mulDiv) and the division.
   The want values come from bc. */
#include <stdio.h>
#include <string.h>

#include "front/u256.h"

/* 2^256 - 1, 2^256 - 2, 2^255 and 2^254 + 2. */
#define TOP "115792089237316195423570985008687907853269984665640564039457584007913129639935"
#define TOP_LESS1 "115792089237316195423570985008687907853269984665640564039457584007913129639934"
#define HALF "57896044618658097711785492504343953926634992332820282019728792003956564819968"
#define QUARTER_PLUS2 "28948022309329048855892746252171976963317496166410141009864396001978282409986"
/* 2^255 + 1, 2^255 + 3, 2^255 + 5 and 2^255 + 7. */
#define HALF_PLUS1 "57896044618658097711785492504343953926634992332820282019728792003956564819969"
#define HALF_PLUS3 "57896044618658097711785492504343953926634992332820282019728792003956564819971"
#define HALF_PLUS5 "57896044618658097711785492504343953926634992332820282019728792003956564819973"
#define HALF_PLUS7 "57896044618658097711785492504343953926634992332820282019728792003956564819975"

/* A NULL want is a quotient of 2^256 or more (mulDiv) or a 0 divisor. */
typedef struct {
  const char *a;
  const char *b;
  const char *d;
  const char *want;
} MulDivRow;

typedef struct {
  const char *a;
  const char *b;
  const char *hi;
  const char *lo;
} WideRow;

typedef struct {
  const char *a;
  const char *b;
  const char *want;
} DivRow;

static const MulDivRow MULDIV_ROWS[] = {
    {"7", "3", "2", "10"},
    {"0", "5", "3", "0"},
    {"5", "0", "3", "0"},
    {"12345", "678", "1", "8369910"},
    {TOP, "1", "1", TOP},
    {TOP, TOP, TOP, TOP},
    {TOP, "2", "1", NULL},
    {HALF, "2", "2", HALF},
    /* The product is 2^256 * D exactly: the high half equals D. */
    {HALF, "2", "1", NULL},
    /* Just below 2^256 * 3, then 2^256 * 3. */
    {TOP, "3", "3", TOP},
    {HALF, "6", "3", NULL},
    {TOP, TOP, TOP_LESS1, NULL},
    {TOP, TOP, HALF, NULL},
    {TOP, TOP, "1", NULL},
    {"1606938044258990275541962092341162602522202993782792835313721", "1267650600228229401496703205383",
     "1152921504606846979", "1766847064778384324985810878155008528739851925087968666816721669481037904"},
    {HALF_PLUS1, HALF_PLUS3, TOP, QUARTER_PLUS2},
    /* A divisor above 2^255: the remainder can pass 2^256 in the shift. */
    {TOP, HALF_PLUS5, HALF_PLUS7, "115792089237316195423570985008687907853269984665640564039457584007913129639931"},
    {"7", "3", "0", NULL},
};

static const WideRow WIDE_ROWS[] = {
    {TOP, TOP, TOP_LESS1, "1"},
    {"340282366920938463463374607431768211456", "340282366920938463463374607431768211456", "1", "0"},
    {HALF_PLUS1, HALF_PLUS3, QUARTER_PLUS2, "3"},
};

static const DivRow DIV_ROWS[] = {
    {"7", "2", "3"},
    {TOP, "1", TOP},
    {TOP, TOP, "1"},
    {TOP, HALF_PLUS7, "1"},
    {"7", "0", NULL},
};

/* Reads decimal digits. Returns 0 on an overflow. */
static int parse(U256 *out, const char *text) {
  const char *p;
  int ok = 1;
  u256_from_u64(out, 0);
  for (p = text; *p != '\0'; p++)
    ok &= u256_scale_add(out, 10u, (uint32_t)(*p - '0'));
  return ok;
}

/* 1 when X prints as the digits WANT. */
static int same(const U256 *x, const char *want) {
  char got[96];
  char text[96];
  word_text(WORD_U256, x, got, sizeof got);
  snprintf(text, sizeof text, "%su", want);
  return strcmp(got, text) == 0;
}

static int check_muldiv(const MulDivRow *row) {
  U256 a;
  U256 b;
  U256 d;
  U256 q;
  int ok = parse(&a, row->a) & parse(&b, row->b) & parse(&d, row->d);
  int fits = ok && u256_muldiv(&q, &a, &b, &d);
  int pass = ok && (row->want == NULL ? !fits : fits && same(&q, row->want));
  if (!pass)
    fprintf(stderr, "FAIL muldiv %s * %s / %s\n", row->a, row->b, row->d);
  return !pass;
}

static int check_wide(const WideRow *row) {
  U256 a;
  U256 b;
  U256 hi;
  U256 lo;
  int ok = parse(&a, row->a) & parse(&b, row->b);
  int pass;
  u256_mul_wide(&hi, &lo, &a, &b);
  pass = ok && same(&hi, row->hi) && same(&lo, row->lo);
  if (!pass)
    fprintf(stderr, "FAIL mul_wide %s * %s\n", row->a, row->b);
  return !pass;
}

static int check_div(const DivRow *row) {
  U256 a;
  U256 b;
  U256 q;
  int ok = parse(&a, row->a) & parse(&b, row->b);
  int done = ok && u256_div(&q, &a, &b);
  int pass = ok && (row->want == NULL ? !done : done && same(&q, row->want));
  if (!pass)
    fprintf(stderr, "FAIL div %s / %s\n", row->a, row->b);
  return !pass;
}

int main(void) {
  size_t i;
  int fail = 0;
  for (i = 0; i < sizeof MULDIV_ROWS / sizeof MULDIV_ROWS[0]; i++)
    fail += check_muldiv(&MULDIV_ROWS[i]);
  for (i = 0; i < sizeof WIDE_ROWS / sizeof WIDE_ROWS[0]; i++)
    fail += check_wide(&WIDE_ROWS[i]);
  for (i = 0; i < sizeof DIV_ROWS / sizeof DIV_ROWS[0]; i++)
    fail += check_div(&DIV_ROWS[i]);
  if (fail != 0)
    return 1;
  puts("word arithmetic regressions: passed");
  return 0;
}
