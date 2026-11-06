"""G1: a retainer opened and countersigned with Schnorr signatures (g1-retainer.lang).

An Auth (rx, z) is a 2 of 3 FROST signature on secp256k1. It verifies as one
Schnorr signature of the x-only key on the message (tag, engagement digest).
A point is lifted from x with the smaller y. engage opens the engagement with
the client signature, and countersign sets the signed time with the firm
signature. csFundCtl funds 200 at 30, countersigns at 40 and reads the
recognized value at 50.
"""
import functools
import hashlib
from enum import Enum
from typing import NamedTuple, Optional

U256_END = 1 << 256
P = 2**256 - 2**32 - 977
N = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141
G = (
    0x79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798,
    0x483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8,
)
TAG_AUTH = int.from_bytes(b"accrue-auth", "big")
TAG_ENGAGE = int.from_bytes(b"accrue-engage", "big")
TAG_COUNTERSIGN = int.from_bytes(b"accrue-countersign", "big")


class Instrument(Enum):
    PUSH = 0
    PULL = 1
    STREAM = 2


class Segment(NamedTuple):
    start: int
    until: int
    num: int
    den: int


class Mass(NamedTuple):
    at: int
    amount: int


class Rate(NamedTuple):
    segments: tuple
    masses: tuple


class Window(NamedTuple):
    start: int
    end: int


class Grant(NamedTuple):
    instrument: Instrument
    paused: bool


class Policy(NamedTuple):
    rate: Rate
    window: Window
    grant: Grant
    dispute: tuple


class Engagement(NamedTuple):
    client: int
    firm: int
    asset: int
    scope: int
    window: Window
    signed: Optional[int]
    cancelled: Optional[int]


class Stamp(NamedTuple):
    time: int
    prev: int
    rx: int
    z: int


class Book(NamedTuple):
    log: tuple
    withdrawn: int
    closed: Optional[int]


class Escrow(NamedTuple):
    eng: Engagement
    book: Book


def bind(o, f):
    return None if o is None else f(o)


def word_hash(a, b):
    """SHA-256 of two big-endian 32-byte words, read as a big-endian word."""
    data = a.to_bytes(32, "big") + b.to_bytes(32, "big")
    return int.from_bytes(hashlib.sha256(data).digest(), "big")


def h3(t, d, e):
    return word_hash(word_hash(t, d), e)


def point_add(a, b):
    """The affine sum of two points; None is the point at infinity."""
    if a is None:
        return b
    if b is None:
        return a
    if a[0] == b[0] and (a[1] + b[1]) % P == 0:
        return None
    if a == b:
        s = 3 * a[0] * a[0] * pow(2 * a[1], -1, P) % P
    else:
        s = (b[1] - a[1]) * pow(b[0] - a[0], -1, P) % P
    x = (s * s - a[0] - b[0]) % P
    return (x, (s * (a[0] - x) - a[1]) % P)


def point_mul(k, point):
    return functools.reduce(
        lambda acc, bit: point_add(point_add(acc, acc), point if bit == "1" else None),
        bin(k)[2:],
        None,
    )


def lift_x(x):
    """The point (x, y) with the smaller y, or None."""
    if x >= P:
        return None
    rhs = (pow(x, 3, P) + 7) % P
    y = pow(rhs, (P + 1) // 4, P)
    return (x, min(y, P - y)) if y * y % P == rhs else None


def schnorr_ok(px, rx, z, head, e):
    """z G = R + c P, with c the hash chain of (TAG_AUTH, px, rx, head, e) mod N."""
    key, nonce = lift_x(px), lift_x(rx)
    if z >= N or key is None or nonce is None:
        return False
    c = word_hash(word_hash(word_hash(word_hash(TAG_AUTH, px), rx), head), e) % N
    return point_mul(z, G) == point_add(nonce, point_mul(c, key))


def u256_add(a, b):
    """a + b, or None on an overflow."""
    total = a + b
    return total if total < U256_END else None


def u256_sub(a, b):
    """a - b, or None when b is more than a."""
    return a - b if b <= a else None


def nat_sub(a, b):
    """a - b, or None when b is more than a."""
    return a - b if b <= a else None


def mul_div(a, b, d):
    """floor(a * b / d), or None when the quotient is not a 256-bit word."""
    q = a * b // d
    return q if q < U256_END else None


def later(t, o):
    """True when o is None or o is before t."""
    return o is None or o < t


def opt_min(x, o):
    """min(x, o); a None bound has no effect."""
    return x if o is None else min(x, o)


def opt_max(x, o):
    """max(x, o); a None bound has no effect."""
    return x if o is None else max(x, o)


# An Acc is an ok flag and a sum. A None term clears the flag, and then the
# sum is None.
ACC_ZERO = (True, 0)


def acc_add(acc, term):
    ok, total = acc
    if not ok:
        return acc
    after = None if term is None else u256_add(total, term)
    return (False, 0) if after is None else (True, after)


def acc_out(acc):
    ok, total = acc
    return total if ok else None


def seg_term(s, x):
    """floor(num * (clamp(x, start, until) - start) / den).

    None only when until is before start (a bad Segment).
    """
    d = nat_sub(min(max(x, s.start), s.until), s.start)
    return None if d is None else mul_div(s.num, d, s.den)


def g_at(r, x):
    """The sum of the segment terms at the offset x, and each mass at x or before."""
    segs = functools.reduce(lambda a, s: acc_add(a, seg_term(s, x)), r.segments, ACC_ZERO)
    total = functools.reduce(
        lambda a, m: acc_add(a, m.amount) if m.at <= x else a, r.masses, segs
    )
    return acc_out(total)


def accrue(r, s, lo, hi, closed):
    """G(hi - s) - G(lo - s) on (lo, hi], or G(hi - s) on the closed [s, hi].

    An empty interval gives 0 without evaluating G.
    """
    if not (lo <= hi if closed else later(hi, lo)):
        return 0
    dh = nat_sub(hi, s)
    gh = None if dh is None else g_at(r, dh)
    if closed or gh is None:
        return gh
    dl = nat_sub(lo, s)
    gl = None if dl is None else g_at(r, dl)
    return None if gl is None else u256_sub(gh, gl)


def entry_term(e, lower, t, nxt, ti, p):
    """The term of the entry (ti, p), in force on (ti, nxt].

    A paused grant or no signed time adds 0. An entry in force at the start
    S of e adds G on the closed [S, hi], so a mass at the offset 0 counts.
    """
    sg = e.signed
    if p.grant.paused or sg is None:
        return 0
    start = e.window.start
    lo = opt_max(max(ti, start, p.window.start, sg), lower)
    hi = opt_min(opt_min(min(e.window.end, p.window.end, t), nxt), e.cancelled)
    closed = ti < start and p.window.start <= start and sg <= start and later(start, lower)
    return accrue(p.rate, start, lo, hi, closed)


def earned_from(lower, h, e, t):
    """The sum of the entry terms of h, from the last entry to the first."""
    acc, _ = functools.reduce(
        lambda st, x: (acc_add(st[0], entry_term(e, lower, t, st[1], x[0], x[1])), x[0]),
        reversed(h),
        (ACC_ZERO, None),
    )
    return acc_out(acc)


def earned_at(h, e, t):
    return earned_from(None, h, e, t)


def funded_at(log, t):
    """The funded amount of the last entry at t or before, or 0 when there is none."""
    return functools.reduce(lambda f, x: x[1] if x[0] <= t else f, log, 0)


def log_ok(log, t):
    """True when no entry of the log is after t."""
    return functools.reduce(lambda v, x: v if x[0] <= t else False, log, True)


def book_ok(log, c, t):
    """log_ok, and t is after the last close c."""
    return log_ok(log, t) and later(t, c)


def recognized_at(h, e, log, t):
    """min(funded t, earned t)."""
    return bind(earned_at(h, e, t), lambda x: min(funded_at(log, t), x))


def put(b, t, f):
    """The book b with (t, f) at the end of its log."""
    return b._replace(log=b.log + ((t, f),))


def esc_fund(s, a, x):
    """Add a to funded (not after a cancel)."""
    e, b = x
    t = s.time
    if e.cancelled is not None or not book_ok(b.log, b.closed, t):
        return None
    return bind(u256_add(funded_at(b.log, t), a), lambda f: (Escrow(e, put(b, t, f)), ()))


def eng_digest(e):
    """The digest of the client, the firm, the asset, the scope and the window."""
    w = e.window
    return h3(
        TAG_ENGAGE,
        word_hash(e.client, e.firm),
        h3(TAG_ENGAGE, word_hash(e.asset, e.scope), word_hash(w.start, w.end)),
    )


def engage(cl, fm, asset, scope, wn, auth):
    """An escrow with an empty book, when auth is the client signature."""
    rx, z = auth
    e = Engagement(cl, fm, asset, scope, wn, None, None)
    return Escrow(e, Book((), 0, None)) if schnorr_ok(cl, rx, z, TAG_ENGAGE, eng_digest(e)) else None


def countersign(s, auth, x):
    """Set the signed time to the stamp time, when e is not signed, the book is
    ok at that time and auth is the firm signature."""
    e, b = x
    rx, z = auth
    ok = (
        book_ok(b.log, b.closed, s.time)
        and e.signed is None
        and schnorr_ok(e.firm, rx, z, TAG_COUNTERSIGN, eng_digest(e))
    )
    return Escrow(e._replace(signed=s.time), b) if ok else None


def bk_rec(h, t, x):
    return recognized_at(h, x.eng, x.book.log, t)


def show_opt_time(o):
    return "none" if o is None else f"(some {o})"


def show_funding(log):
    """A Funding as the kit prints it: fmUnit, fmEta, or fmMul to the left."""

    def eta(x):
        return f"fmEta (Prod Nat U256) (pair {x[0]} {x[1]}u)"

    if not log:
        return "fmUnit (Prod Nat U256)"
    return functools.reduce(lambda a, x: f"fmMul (Prod Nat U256) ({a}) ({eta(x)})", log[1:], eta(log[0]))


def show_escrow(x):
    e, b = x
    w = e.window
    return (
        f"escrowOf (engagementOf {e.client}u {e.firm}u {e.asset}u {e.scope}u"
        f" (windowOf {w.start} {w.end}) {show_opt_time(e.signed)} {show_opt_time(e.cancelled)})"
        f" (bookOf ({show_funding(b.log)}) {b.withdrawn}u {show_opt_time(b.closed)})"
    )


def show_opt_escrow(o):
    return "none" if o is None else f"some ({show_escrow(o)})"


def show_u256(o):
    return "none" if o is None else f"{o}u"


r_b = Rate((Segment(0, 50, 2, 1),), (Mass(0, 40),))
d_b = (2500, 5000)


def hist_of(i):
    return ((0, Policy(r_b, Window(0, 100), Grant(i, False), d_b)),)


def s_at(t):
    return Stamp(t, 0, 0, 0)


h_push = hist_of(Instrument.PUSH)
C_K = 0xF2BB893B2497C632D4B2E1BCC9F3565EFE400130E944EA3A4BA689A46220BAA2
F_K = 0xFA043432A49AFE1C8E4AD857EE8935E7CAE984E9F655FBD12117D48849D5727D
ENG = (
    0xAE00A0168CDCF5811726C150336492D5BE4EBB5857965DDB7ACDE38FFCA7A33C,
    0x2C8CC28029354CA5E4F7D87C6807BD594A4BB1CE4DE3FB875BBFCF9DBB033828,
)
CTR = (
    0x4C496FBAFA6F2C546472F4B21BCEB13B71945C5B6E6339783A0DB63BE6B38CF,
    0x64E219DEE5E8683EE2FB3F4FF4BB43F5A29B34A0393E883C6B88327E5F1C55CB,
)
e_s = Engagement(C_K, F_K, 3, 4, Window(10, 60), None, None)
x_s = Escrow(e_s, Book((), 0, None))
cs_late_fund = esc_fund(s_at(30), 200, x_s)


def cs_fund_at(t):
    return bind(cs_late_fund, lambda p: bind(countersign(s_at(t), CTR, p[0]), lambda x: bk_rec(h_push, 50, x)))


print(show_opt_escrow(engage(C_K, F_K, 3, 4, Window(10, 60), ENG)))
print(show_opt_escrow(countersign(s_at(20), CTR, x_s)))
print(show_u256(cs_fund_at(40)))
