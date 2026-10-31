/* Host FROST regressions (slice S7 b2, Q-S7-2 to Q-S7-5 (a)): the threshold
   Schnorr signatures of front/frost.c under the append key. The keyAppend
   committee comes from the DKG of front/dkg.c as in test/dkgtool.c (2 of 3,
   then the reshare to 3 of 4); a second committee from other seeds gives a
   key with the other y. Rows: the known answers of rho and c, 2 of 3 signs
   and verifies, a bad share gives AUTH_BAD_SHARE at its member index,
   another commitment list gives another rho and R, two sessions give two R,
   a key with the larger y and a key with the smaller y sign, and the 3 of 4
   key signs. The message is the head of the empty record (hash tagGenesis
   0) and the entry 42. The seeds are fixed for reproducible vectors only.
   With the argument `words` it also prints the closed Auth words of
   examples/append.lang (slice S7 b5), one `name 0x<word>` per line. With
   the argument `stamps` it prints the stamp words of examples/stamps.lang
   (slice S4, Q-S4-3 (a)) in the same form. With the argument `engage` it
   prints the engage words of examples/books.lang (slice S5b) in the same
   form. With the argument `pay` it prints the pay words of
   examples/pay.lang (slice S7, Q-S7-1 to Q-S7-4 (a)) in the same form. */
#include <stdio.h>
#include <string.h>

#include "front/blsag.h"
#include "front/dkg.h"
#include "front/frost.h"

static int fail(const char *what) {
  fprintf(stderr, "FAIL frost %s\n", what);
  return 0;
}

static int holds(const char *name, int cond) {
  return cond ? 1 : fail(name);
}

static int expect(const char *name, AuthResult got, AuthResult want) {
  if (got == want)
    return 1;
  fprintf(stderr, "FAIL frost %s: result %d, want %d\n", name, (int)got, (int)want);
  return 0;
}

static void word(const char *name, const U256 *w) {
  printf("%s 0x%016llx%016llx%016llx%016llx\n", name, (unsigned long long)w->limb[3], (unsigned long long)w->limb[2],
         (unsigned long long)w->limb[1], (unsigned long long)w->limb[0]);
}

static int same(const U256 *a, const U256 *b) {
  return memcmp(a->limb, b->limb, sizeof a->limb) == 0;
}

/* 1 when P, a point on the curve, has the larger of its two y. */
static int larger_y(const GroupPoint *p) {
  GroupPoint low;
  U256 x;
  U256 y;
  group_to_words(&x, &y, p);
  return group_lift_x(&low, &x) && !group_eq(&low, p);
}

/* The binding factor of signer I as the header of front/frost.h states it:
   the chain of thr_hash from AUTH_RHO_TAG over I, PX, HEAD, E and then j,
   D.x, D.y, E.x, E.y for each of the K commitments of LIST, mod GROUP_N. */
static void rho_chain(U256 *out, uint32_t i, const U256 *px, const U256 *head, const U256 *e, const AuthCommit *list, uint32_t k) {
  U256 x;
  U256 y;
  uint32_t m;
  u256_from_u64(&x, i);
  thr_hash(out, &AUTH_RHO_TAG, &x);
  thr_hash(out, out, px);
  thr_hash(out, out, head);
  thr_hash(out, out, e);
  for (m = 0; m < k; m++) {
    u256_from_u64(&x, list[m].i);
    thr_hash(out, out, &x);
    group_to_words(&x, &y, &list[m].d);
    thr_hash(out, out, &x);
    thr_hash(out, out, &y);
    group_to_words(&x, &y, &list[m].e);
    thr_hash(out, out, &x);
    thr_hash(out, out, &y);
  }
  blsag_reduce(out, out);
}

/* The DKG of N members at the threshold T, member i with the seed SEEDS[i -
   1] and a_0 = thr_nonce(seed, 0): SHARES (x = 1 to N), the key PK and
   KEYS[x - 1] = Y_x. 1, or 0 when a dealing or a combine refuses or two
   members get different keys. */
static int committee(ThrShare *shares, GroupPoint *pk, GroupPoint *keys, const U256 *seeds, uint32_t t, uint32_t n) {
  DkgDeal deals[THR_N_MAX];
  U256 dealt[THR_N_MAX][THR_N_MAX];
  U256 col[THR_N_MAX];
  U256 a0;
  GroupPoint got;
  uint32_t i;
  uint32_t j;
  for (i = 0; i < n; i++) {
    thr_nonce(&a0, &seeds[i], 0u);
    if (!dkg_deal(&deals[i], dealt[i], &a0, &seeds[i], i + 1u, t, n))
      return 0;
  }
  for (j = 0; j < n; j++) {
    for (i = 0; i < n; i++)
      col[i] = dealt[i][j];
    if (!dkg_combine(&shares[j], &got, deals, col, n, t, n, j + 1u))
      return 0;
    if (j == 0u)
      *pk = got;
    if (!group_eq(pk, &got))
      return 0;
    keys[j] = shares[j].key;
  }
  return 1;
}

/* One signing session of the K members of SIGNERS (rising member indices)
   of the committee SHARES (T of N, KEYS) under PK for the message (HEAD,
   E): round 1 with the seeds blsag_key(SEED + m), round 2, then
   frost_aggregate into SIG. LIST and ZS get the commitments and the
   shares. */
static AuthResult session(AuthSig *sig, AuthCommit *list, AuthShare *zs, const ThrShare *shares, const uint32_t *signers, uint32_t k, const GroupPoint *keys, uint32_t t, uint32_t n, const GroupPoint *pk, const U256 *head, const U256 *e, uint64_t seed) {
  AuthNonce nonces[THR_N_MAX];
  AuthResult r;
  U256 s;
  uint32_t at;
  uint32_t m;
  for (m = 0; m < k; m++) {
    blsag_key(&s, seed + m);
    frost_commit(&nonces[m], &list[m], &shares[signers[m] - 1u], &s);
  }
  for (m = 0; m < k; m++) {
    r = frost_sign(&zs[m], &shares[signers[m] - 1u], &nonces[m], pk, head, e, list, k);
    if (r != AUTH_OK)
      return r;
  }
  return frost_aggregate(sig, &at, zs, list, k, keys, t, n, pk, head, e);
}

/* The stamp tags of domain/domain.lang (slice S4): the ASCII bytes of
   "accrue-stamp" and "accrue-stamp-genesis" as a big-endian U256. */
static void stamp_tags(U256 *tag, U256 *genesis) {
  u256_from_u64(tag, 0x75652d7374616d70ull);
  tag->limb[1] = 0x61636372ull;
  u256_from_u64(genesis, 0x2d67656e65736973ull);
  genesis->limb[1] = 0x75652d7374616d70ull;
  genesis->limb[2] = 0x61636372ull;
}

/* One stamp of examples/stamps.lang: a session of members 1 and 2 of the
   2 of 3 committee SHARES (KEYS) under PK on the message (PREV, T), with
   the seeds from SEED. It prints R_NAME and Z_NAME and puts the
   signature in SIG. 0 when the session or the check fails. */
static int stamp(const char *r_name, const char *z_name, AuthSig *sig, const ThrShare *shares, const GroupPoint *keys, const GroupPoint *pk, const U256 *px, const U256 *prev, uint64_t t, uint64_t seed) {
  static const uint32_t pair[2] = {1u, 2u};
  AuthCommit list[THR_N_MAX];
  AuthShare zs[THR_N_MAX];
  U256 e;
  u256_from_u64(&e, t);
  if (session(sig, list, zs, shares, pair, 2u, keys, 2u, 3u, pk, prev, &e, seed) != AUTH_OK || schnorr_verify(px, sig, prev, &e) != AUTH_OK)
    return fail(r_name);
  word(r_name, &sig->rx);
  word(z_name, &sig->z);
  return 1;
}

/* The stamp words (slice S4, Q-S4-3 (a)): the key, the head g0 of the
   empty chain (hash tagStampGenesis 0), the head h1 after s1 (H3 tagStamp
   g0 5) and the signatures of the keyAppend committee. s1 (5, g0) and s2
   (9, h1) are the good stamps. sOld (3, h1), sSame (5, h1) and sPrev (9,
   g0) have a good signature on their own message, so only one check of
   stampOk refuses each. s1ZBad is the z of s1 plus 1. */
static int stamp_words(const ThrShare *shares, const GroupPoint *keys, const GroupPoint *pk, const U256 *px) {
  U256 tag;
  U256 genesis;
  U256 zero;
  U256 one;
  U256 five;
  U256 g0;
  U256 h1;
  U256 z_bad;
  AuthSig s1;
  AuthSig x;
  stamp_tags(&tag, &genesis);
  u256_from_u64(&zero, 0u);
  u256_from_u64(&one, 1u);
  u256_from_u64(&five, 5u);
  thr_hash(&g0, &genesis, &zero);
  thr_hash(&h1, &tag, &g0);
  thr_hash(&h1, &h1, &five);
  word("stampKey", px);
  word("g0", &g0);
  word("h1", &h1);
  if (!(stamp("s1R", "s1Z", &s1, shares, keys, pk, px, &g0, 5u, 400u) && stamp("s2R", "s2Z", &x, shares, keys, pk, px, &h1, 9u, 410u)
        && stamp("sOldR", "sOldZ", &x, shares, keys, pk, px, &h1, 3u, 420u) && stamp("sSameR", "sSameZ", &x, shares, keys, pk, px, &h1, 5u, 430u)
        && stamp("sPrevR", "sPrevZ", &x, shares, keys, pk, px, &g0, 9u, 440u)))
    return 0;
  u256_add(&z_bad, &s1.z, &one);
  word("s1ZBad", &z_bad);
  return 1;
}

/* The engage tags of domain/domain.lang (slice S5b): the ASCII bytes of
   "accrue-engage" and "accrue-countersign" as a big-endian U256. */
static void engage_tags(U256 *engage, U256 *countersign) {
  u256_from_u64(engage, 0x652d656e67616765ull);
  engage->limb[1] = 0x6163637275ull;
  u256_from_u64(countersign, 0x6e7465727369676eull);
  countersign->limb[1] = 0x637275652d636f75ull;
  countersign->limb[2] = 0x6163ull;
}

/* One signature of members 1 and 2 of the 2 of 3 committee SHARES (KEYS)
   under PK on the message (TAG, D), with the seeds from SEED. It prints
   R_NAME and Z_NAME and puts the signature in SIG. 0 when the session or
   the check fails. */
static int signed_by(const char *r_name, const char *z_name, AuthSig *sig, const ThrShare *shares, const GroupPoint *keys, const GroupPoint *pk, const U256 *px, const U256 *tag, const U256 *d, uint64_t seed) {
  static const uint32_t pair[2] = {1u, 2u};
  AuthCommit list[THR_N_MAX];
  AuthShare zs[THR_N_MAX];
  if (session(sig, list, zs, shares, pair, 2u, keys, 2u, 3u, pk, tag, d, seed) != AUTH_OK || schnorr_verify(px, sig, tag, d) != AUTH_OK)
    return fail(r_name);
  word(r_name, &sig->rx);
  word(z_name, &sig->z);
  return 1;
}

/* The engage words (slice S5b, Q-S5-7 (a)): two 2 of 3 committees, the
   client (seeds blsag_key(500 + i)) and the firm (seeds blsag_key(510 +
   i)), and their keys cK and fK. The message digest is engDigest of the
   engagement (cK, fK, asset 3, scope 4, window 10 to 60): H3 tagEngage
   (hash cK fK) (H3 tagEngage (hash 3 4) (hash 10 60)), with H3 t x y =
   hash (hash t x) y. engR, engZ: the client on tagEngage. ctrR, ctrZ: the
   firm on tagCountersign. ctrWrongR, ctrWrongZ: the client on
   tagCountersign. engZBad is engZ plus 1. */
static int engage_words(void) {
  ThrShare client[THR_N_MAX];
  ThrShare firm[THR_N_MAX];
  GroupPoint client_pk;
  GroupPoint firm_pk;
  GroupPoint client_keys[THR_N_MAX];
  GroupPoint firm_keys[THR_N_MAX];
  U256 seeds[THR_N_MAX];
  U256 cx;
  U256 cy;
  U256 fx;
  U256 fy;
  U256 tag_engage;
  U256 tag_countersign;
  U256 asset;
  U256 scope;
  U256 s_e;
  U256 e_e;
  U256 one;
  U256 keys_h;
  U256 inner;
  U256 window_h;
  U256 d;
  U256 z_bad;
  AuthSig sig;
  uint32_t i;
  for (i = 0; i < 3u; i++)
    blsag_key(&seeds[i], 500u + i);
  if (!committee(client, &client_pk, client_keys, seeds, 2u, 3u))
    return fail("the client committee");
  for (i = 0; i < 3u; i++)
    blsag_key(&seeds[i], 510u + i);
  if (!committee(firm, &firm_pk, firm_keys, seeds, 2u, 3u))
    return fail("the firm committee");
  group_to_words(&cx, &cy, &client_pk);
  group_to_words(&fx, &fy, &firm_pk);
  engage_tags(&tag_engage, &tag_countersign);
  u256_from_u64(&asset, 3u);
  u256_from_u64(&scope, 4u);
  u256_from_u64(&s_e, 10u);
  u256_from_u64(&e_e, 60u);
  u256_from_u64(&one, 1u);
  thr_hash(&keys_h, &cx, &fx);
  thr_hash(&inner, &asset, &scope);
  thr_hash(&window_h, &s_e, &e_e);
  thr_hash(&inner, &tag_engage, &inner);
  thr_hash(&inner, &inner, &window_h);
  thr_hash(&d, &tag_engage, &keys_h);
  thr_hash(&d, &d, &inner);
  word("cK", &cx);
  word("fK", &fx);
  if (!signed_by("engR", "engZ", &sig, client, client_keys, &client_pk, &cx, &tag_engage, &d, 520u))
    return 0;
  u256_add(&z_bad, &sig.z, &one);
  word("engZBad", &z_bad);
  return signed_by("ctrR", "ctrZ", &sig, firm, firm_keys, &firm_pk, &fx, &tag_countersign, &d, 530u)
         && signed_by("ctrWrongR", "ctrWrongZ", &sig, client, client_keys, &client_pk, &cx, &tag_countersign, &d, 540u);
}

/* H3 t x y = hash (hash t x) y of domain/domain.lang. OUT can be Y. */
static void h3(U256 *out, const U256 *t, const U256 *x, const U256 *y) {
  U256 m;
  thr_hash(&m, t, x);
  thr_hash(out, &m, y);
}

/* ixDigest of domain/domain.lang (slice S7) for a PayEntry with the kind
   code KIND and the Instruction (PAYEE, ASSET, AMOUNT, sAt T) under TAG:
   H3 tagPay (hash kind payee) (H3 tagPay (hash asset amount) (H3 tagPay
   (hash t 0) (hash 0 0))). The stamp sAt t has prev, rx and z 0. */
static void ix_digest(U256 *d, const U256 *tag, uint64_t kind, uint64_t payee, uint64_t asset, uint64_t amount, uint64_t t) {
  U256 a;
  U256 b;
  U256 zero;
  U256 kp;
  U256 aa;
  U256 in;
  u256_from_u64(&zero, 0u);
  u256_from_u64(&a, kind);
  u256_from_u64(&b, payee);
  thr_hash(&kp, &a, &b);
  u256_from_u64(&a, asset);
  u256_from_u64(&b, amount);
  thr_hash(&aa, &a, &b);
  u256_from_u64(&a, t);
  thr_hash(&b, &a, &zero);
  thr_hash(&in, &zero, &zero);
  h3(&in, tag, &b, &in);
  h3(&in, tag, &aa, &in);
  h3(d, tag, &kp, &in);
}

/* One signature of members 1, 2 and 3 of the 3 of 4 committee SHARES
   (KEYS) under PK on the message (HEAD, D), with the seeds from SEED. It
   prints R_NAME and Z_NAME and puts the signature in SIG. 0 when the
   session or the check fails. */
static int pay_signed(const char *r_name, const char *z_name, AuthSig *sig, const ThrShare *shares, const GroupPoint *keys, const GroupPoint *pk, const U256 *px, const U256 *head, const U256 *d, uint64_t seed) {
  static const uint32_t trio[3] = {1u, 2u, 3u};
  AuthCommit list[THR_N_MAX];
  AuthShare zs[THR_N_MAX];
  if (session(sig, list, zs, shares, trio, 3u, keys, 3u, 4u, pk, head, d, seed) != AUTH_OK || schnorr_verify(px, sig, head, d) != AUTH_OK)
    return fail(r_name);
  word(r_name, &sig->rx);
  word(z_name, &sig->z);
  return 1;
}

/* The pay words (slice S7, Q-S7-1 to Q-S7-4 (a)): a 3 of 4 keyPay
   committee from the seeds blsag_key(600 + i) and its key payKey. p is
   iRes of examples/pay.lang as a kindSettle entry (code 0, payee 2, asset
   3, amount 60, sAt 20) and q is iZero (amount 0) as a kindSettle entry.
   G0 is the head of the empty record, and h1 = H3 tagAppend g0 (ixDigest
   p) is the head of the record [p]. payDigest is ixDigest p. payR, payZ: p
   at g0. payZBad is payZ plus 1. payReplayR, payReplayZ: p at h1. pay2R,
   pay2Z: q at h1. payZBad must not verify, and the signature of p at g0
   must not verify at h1. 0 on a failure. */
static int pay_words(const U256 *g0) {
  ThrShare shares[THR_N_MAX];
  GroupPoint pk;
  GroupPoint keys[THR_N_MAX];
  U256 seeds[THR_N_MAX];
  U256 tag;
  U256 append;
  U256 px;
  U256 py;
  U256 one;
  U256 dp;
  U256 dq;
  U256 h1;
  AuthSig sig;
  AuthSig bad;
  AuthSig x;
  uint32_t i;
  for (i = 0; i < 4u; i++)
    blsag_key(&seeds[i], 600u + i);
  if (!committee(shares, &pk, keys, seeds, 3u, 4u))
    return fail("the keyPay committee");
  group_to_words(&px, &py, &pk);
  u256_from_u64(&tag, 0x637275652d706179ull);
  tag.limb[1] = 0x6163ull;
  u256_from_u64(&append, 0x652d617070656e64ull);
  append.limb[1] = 0x6163637275ull;
  u256_from_u64(&one, 1u);
  ix_digest(&dp, &tag, 0u, 2u, 3u, 60u, 20u);
  ix_digest(&dq, &tag, 0u, 2u, 3u, 0u, 20u);
  h3(&h1, &append, g0, &dp);
  word("payKey", &px);
  word("payDigest", &dp);
  if (!pay_signed("payR", "payZ", &sig, shares, keys, &pk, &px, g0, &dp, 610u))
    return 0;
  bad = sig;
  u256_add(&bad.z, &sig.z, &one);
  word("payZBad", &bad.z);
  return holds("payZBad does not verify", schnorr_verify(&px, &bad, g0, &dp) != AUTH_OK)
         && holds("payR does not verify at h1", schnorr_verify(&px, &sig, &h1, &dp) != AUTH_OK)
         && pay_signed("payReplayR", "payReplayZ", &x, shares, keys, &pk, &px, &h1, &dp, 620u)
         && pay_signed("pay2R", "pay2Z", &x, shares, keys, &pk, &px, &h1, &dq, 630u);
}

int main(int argc, char **argv) {
  static const uint32_t pair[2] = {1u, 2u};
  static const uint32_t append_q[2] = {1u, 3u};
  static const uint32_t three[3] = {2u, 3u, 4u};
  ThrShare old[THR_N_MAX];
  ThrShare fresh[THR_N_MAX];
  ThrShare other[THR_N_MAX];
  GroupPoint pk;
  GroupPoint other_pk;
  GroupPoint old_keys[THR_N_MAX];
  GroupPoint new_keys[THR_N_MAX];
  GroupPoint other_keys[THR_N_MAX];
  DkgDeal deals[THR_N_MAX];
  U256 dealt[THR_N_MAX][THR_N_MAX];
  U256 col[THR_N_MAX];
  U256 seeds[THR_N_MAX];
  U256 genesis;
  U256 zero;
  U256 one;
  U256 head;
  U256 e;
  U256 px;
  U256 py;
  U256 ox;
  U256 oy;
  U256 rho_a;
  U256 rho_b;
  U256 want;
  U256 c;
  U256 z_bad;
  AuthCommit list_a[THR_N_MAX];
  AuthCommit list_b[THR_N_MAX];
  AuthCommit list_o[THR_N_MAX];
  AuthCommit list_t[THR_N_MAX];
  AuthShare zs_a[THR_N_MAX];
  AuthShare zs_b[THR_N_MAX];
  AuthShare zs_o[THR_N_MAX];
  AuthShare zs_t[THR_N_MAX];
  AuthShare bad[2];
  AuthSig sig_a;
  AuthSig sig_b;
  AuthSig sig_o;
  AuthSig sig_t;
  AuthSig sig_x;
  uint32_t at = 0u;
  uint32_t i;
  uint32_t j;
  int high;
  int ok = 1;
  /* The keyAppend committee of test/dkgtool.c: 2 of 3, seeds blsag_key(20 + i). */
  for (i = 0; i < 3u; i++)
    blsag_key(&seeds[i], 20u + i);
  if (!committee(old, &pk, old_keys, seeds, 2u, 3u)) {
    fail("the 2 of 3 DKG refused");
    return 1;
  }
  group_to_words(&px, &py, &pk);
  high = larger_y(&pk);
  printf("keyAppend key: the %s y\n", high ? "larger" : "smaller");
  /* The message: the head of the empty record and the entry 42. */
  u256_from_u64(&zero, 0u);
  u256_from_u64(&genesis, 0x2d67656e65736973ull);
  genesis.limb[1] = 0x616363727565ull;
  thr_hash(&head, &genesis, &zero);
  u256_from_u64(&e, 42u);
  u256_from_u64(&one, 1u);
  ok &= expect(high ? "2 of 3 signs (a key with the larger y)" : "2 of 3 signs (a key with the smaller y)",
               session(&sig_a, list_a, zs_a, old, pair, 2u, old_keys, 2u, 3u, &pk, &head, &e, 300u), AUTH_OK)
        & expect("2 of 3 verifies", schnorr_verify(&px, &sig_a, &head, &e), AUTH_OK);
  frost_rho(&rho_a, 1u, &px, &head, &e, list_a, 2u);
  rho_chain(&want, 1u, &px, &head, &e, list_a, 2u);
  ok &= holds("KAT rho (the chain of front/frost.h)", same(&rho_a, &want));
  frost_challenge(&c, &px, &sig_a.rx, &head, &e);
  thr_hash(&want, &AUTH_TAG, &px);
  thr_hash(&want, &want, &sig_a.rx);
  thr_hash(&want, &want, &head);
  thr_hash(&want, &want, &e);
  blsag_reduce(&want, &want);
  ok &= holds("KAT c (the chain of front/frost.h)", same(&c, &want));
  /* z + 1 on the share of member 2, at list position 1. */
  bad[0] = zs_a[0];
  bad[1] = zs_a[1];
  u256_add(&bad[1].z, &zs_a[1].z, &one);
  ok &= expect("a bad share", frost_aggregate(&sig_x, &at, bad, list_a, 2u, old_keys, 2u, 3u, &pk, &head, &e), AUTH_BAD_SHARE)
        & holds("a bad share is at its member index", at == list_a[1].i && at == 2u);
  ok &= expect("a second session signs", session(&sig_b, list_b, zs_b, old, pair, 2u, old_keys, 2u, 3u, &pk, &head, &e, 310u), AUTH_OK);
  frost_rho(&rho_b, 1u, &px, &head, &e, list_b, 2u);
  ok &= holds("another commitment list gives another rho and another R", !same(&rho_a, &rho_b) && !same(&sig_a.rx, &sig_b.rx))
        & holds("two sessions give two R", !same(&sig_a.rx, &sig_b.rx));
  /* The other y: the B1 smoke seeds base * 10 + i (base 1 gives the larger
     y, base 3 the smaller y). */
  for (i = 0; i < 3u; i++)
    u256_from_u64(&seeds[i], (high ? 3u : 1u) * 10u + i);
  if (!committee(other, &other_pk, other_keys, seeds, 2u, 3u)) {
    fail("the second 2 of 3 DKG refused");
    return 1;
  }
  group_to_words(&ox, &oy, &other_pk);
  ok &= holds("the second key has the other y", larger_y(&other_pk) != high);
  ok &= expect(high ? "a key with the smaller y signs" : "a key with the larger y signs",
               session(&sig_o, list_o, zs_o, other, pair, 2u, other_keys, 2u, 3u, &other_pk, &head, &e, 320u), AUTH_OK)
        & expect("the other key verifies", schnorr_verify(&ox, &sig_o, &head, &e), AUTH_OK);
  /* The reshare of test/dkgtool.c keyAppend: members 1 and 3 deal to 3 of 4. */
  for (i = 0; i < 2u; i++) {
    blsag_key(&seeds[i], 120u + i);
    if (!dkg_reshare(&deals[i], dealt[i], &old[append_q[i] - 1u], append_q, 2u, &seeds[i], 3u, 4u)) {
      fail("a reshare dealing refused");
      return 1;
    }
  }
  for (j = 0; j < 4u; j++) {
    for (i = 0; i < 2u; i++)
      col[i] = dealt[i][j];
    if (!dkg_reshare_combine(&fresh[j], deals, col, 2u, old_keys, 2u, 3u, &pk, j + 1u)) {
      fail("the reshare aborted");
      return 1;
    }
    new_keys[j] = fresh[j].key;
  }
  ok &= expect("the 3 of 4 key signs", session(&sig_t, list_t, zs_t, fresh, three, 3u, new_keys, 3u, 4u, &pk, &head, &e, 330u), AUTH_OK)
        & expect("the 3 of 4 key verifies", schnorr_verify(&px, &sig_t, &head, &e), AUTH_OK);
  if (!ok)
    return 1;
  if (argc == 2 && strcmp(argv[1], "stamps") == 0)
    return stamp_words(old, old_keys, &pk, &px) ? 0 : 1;
  if (argc == 2 && strcmp(argv[1], "engage") == 0)
    return engage_words() ? 0 : 1;
  if (argc == 2 && strcmp(argv[1], "pay") == 0)
    return pay_words(&head) ? 0 : 1;
  if (argc == 2 && strcmp(argv[1], "words") == 0) {
    u256_add(&z_bad, &sig_a.z, &one);
    word("key", &px);
    word("otherKey", &ox);
    word("head", &head);
    word("entry", &e);
    word("authR", &sig_a.rx);
    word("authZ", &sig_a.z);
    word("authZBad", &z_bad);
    word("authR34", &sig_t.rx);
    word("authZ34", &sig_t.z);
  }
  puts("frost regressions: passed");
  return 0;
}
