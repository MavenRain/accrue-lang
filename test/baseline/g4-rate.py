"""G4: the rate changes during the engagement (g4-rate.lang).

hA holds pA at 0 and pB at 30, and hC adds pC at 45. Earned at t is the sum
of the entry terms of the history. An entry (ti, p) is in force on
(ti, next], and the windows, the signed time and t cut it. Earned at each
t <= 45 is the same for hA and hC, and after 45 pC has an effect.
"""
import functools
from enum import Enum
from typing import NamedTuple, Optional

U256_END = 1 << 256


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


def show_u256(o):
    return "none" if o is None else f"{o}u"


grant_on = Grant(Instrument.STREAM, False)
r_a = Rate((Segment(0, 20, 3, 2), Segment(20, 40, 1, 1)), (Mass(0, 50), Mass(25, 7)))
r_b = Rate((Segment(0, 50, 2, 1),), (Mass(30, 9),))
p_a = Policy(r_a, Window(0, 100), grant_on, ())
p_b = Policy(r_b, Window(0, 100), grant_on, ())
p_c = Policy(Rate((Segment(0, 50, 5, 1),), ()), Window(0, 100), grant_on, ())
e_a = Engagement(1, 2, 3, 4, Window(10, 60), 10, None)
h_a = ((0, p_a), (30, p_b))
h_c = h_a + ((45, p_c),)
print(show_u256(earned_at(h_a, e_a, 37)))
print(show_u256(earned_at(h_c, e_a, 45)))
print(show_u256(earned_at(h_c, e_a, 50)))
