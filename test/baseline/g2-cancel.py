"""G2: a closed linear stream and a cancel (g2-cancel.lang).

hL streams 1000 from 10 to 17, so earned at the end is the deposit. On the
push history, the client funds 200 at 10 and the firm settles 30 at 20.
The cancel at 30 pays the unearned amount (funded - recognized) back to the
client, and the log gets (30, recognized).
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


class Stamp(NamedTuple):
    time: int
    prev: int
    rx: int
    z: int


class Instruction(NamedTuple):
    payee: int
    asset: int
    amount: int
    at: Stamp


class Book(NamedTuple):
    log: tuple
    withdrawn: int
    closed: Optional[int]


class Escrow(NamedTuple):
    eng: Engagement
    book: Book


def bind(o, f):
    return None if o is None else f(o)


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


def unearned_at(h, e, log, t):
    """funded t - recognized t."""
    return bind(recognized_at(h, e, log, t), lambda r: u256_sub(funded_at(log, t), r))


def pull_at(h, t):
    """The grant of the policy in force at t (the last entry before t) holds pull."""
    return functools.reduce(
        lambda v, x: x[1].grant.instrument is Instrument.PULL if later(t, x[0]) else v, h, False
    )


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


def esc_settle(h, s, v, x):
    """Pay v to the firm.

    Every settle logs the funded amount at its time, and a pull settle also
    adds v to funded. The guard is on the book after: withdrawn + v <=
    recognized after.
    """
    e, b = x
    t = s.time
    if not book_ok(b.log, b.closed, t):
        return None
    f = u256_add(funded_at(b.log, t), v) if pull_at(h, t) else funded_at(b.log, t)
    w2 = u256_add(b.withdrawn, v)
    if f is None or w2 is None:
        return None
    b2 = put(b, t, f)._replace(withdrawn=w2)
    r = recognized_at(h, e, b2.log, t)
    if r is None or r < w2:
        return None
    return (Escrow(e, b2), (Instruction(e.firm, e.asset, v, s),))


def esc_cancel(h, s, x):
    """Cancel at t (not after a cancel).

    The client gets unearned t, and the log gets (t, funded - unearned).
    """
    e, b = x
    t = s.time
    if e.cancelled is not None or not book_ok(b.log, b.closed, t):
        return None
    u = unearned_at(h, e, b.log, t)
    f = bind(u, lambda u: u256_sub(funded_at(b.log, t), u))
    if f is None:
        return None
    return (
        Escrow(e._replace(cancelled=t), put(b, t, f)),
        (Instruction(e.client, e.asset, u, s),),
    )


def and_then(o, f):
    return bind(o, lambda p: f(p[0]))


def pay_of(o):
    return bind(o, lambda p: p[1])


def show_u256(o):
    return "none" if o is None else f"{o}u"


def show_instruction(i):
    s = i.at
    return (
        f"instructionOf {i.payee}u {i.asset}u {i.amount}u"
        f" (stampOf {s.time} {s.prev}u {s.rx}u {s.z}u)"
    )


def show_payout(p):
    """A Payout as the kit prints it: fmUnit, fmEta, or fmMul to the left."""

    def eta(i):
        return f"fmEta Instruction ({show_instruction(i)})"

    if not p:
        return "fmUnit Instruction"
    return functools.reduce(lambda a, i: f"fmMul Instruction ({a}) ({eta(i)})", p[1:], eta(p[0]))


def show_pay(o):
    return "none" if o is None else f"some ({show_payout(o)})"


grant_on = Grant(Instrument.STREAM, False)
e_l = Engagement(1, 2, 3, 4, Window(10, 17), 10, None)
h_l = ((0, Policy(Rate((Segment(0, 7, 1000, 7),), ()), Window(0, 100), grant_on, ())),)
print(show_u256(earned_at(h_l, e_l, 12)))
print(show_u256(earned_at(h_l, e_l, 17)))

r_b = Rate((Segment(0, 50, 2, 1),), (Mass(0, 40),))
d_b = (2500, 5000)


def hist_of(i):
    return ((0, Policy(r_b, Window(0, 100), Grant(i, False), d_b)),)


def s_at(t):
    return Stamp(t, 0, 0, 0)


h_push = hist_of(Instrument.PUSH)
e_b = Engagement(1, 2, 3, 4, Window(10, 60), 10, None)
x_e = Escrow(e_b, Book((), 0, None))
x_f = esc_fund(s_at(10), 200, x_e)
x_s1 = and_then(x_f, lambda x: esc_settle(h_push, s_at(20), 30, x))
x_c = and_then(x_s1, lambda x: esc_cancel(h_push, s_at(30), x))
print(show_pay(pay_of(x_c)))
