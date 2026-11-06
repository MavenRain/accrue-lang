"""G3: an encrypted policy vote (g3-vote.lang).

Enc is the cell of an encrypted value: op2 joins two encrypted values under
the seal, thus the tally of a decision stays encrypted. A policy ballot holds
one encrypted bit for each decision (exactly one yes) and a link tag. A
quorum opens the tally of each decision, and the rule (0 plurality, 1
majority) gives the choice.
"""
import functools
from typing import NamedTuple

NAT_END = 1 << 64


class Enc(NamedTuple):
    value: object

    def op2(self, f, other):
        return Enc(f(self.value, other.value))


class PolicyBallot(NamedTuple):
    votes: tuple
    tag: int


class Quorum(NamedTuple):
    t: int
    n: int


def bind(o, f):
    return None if o is None else f(o)


def nat_add(a, b):
    """a + b, or None on a carry."""
    total = a + b
    return total if total < NAT_END else None


def nat_sub(a, b):
    """a - b, or None when b is more than a."""
    return a - b if b <= a else None


def op_succ(n):
    return nat_add(n, 1)


def count_votes(vs):
    """The number of yes votes, or None on a carry."""
    return functools.reduce(lambda n, f: bind(n, op_succ) if f else n, vs, 0)


def mk_policy_ballot(vs, tag):
    """The ballot of the plain bits vs, or None when vs has not exactly one yes."""
    return PolicyBallot(tuple(Enc(v) for v in vs), tag) if count_votes(vs) == 1 else None


def slot_of(k, cs):
    """The bit of decision k, or an encrypted no after the end."""
    return cs[k] if k < len(cs) else Enc(False)


def tally_at(k, box):
    """The encrypted count of the yes bits of decision k in the box."""
    return functools.reduce(
        lambda t, b: t.op2(lambda n, f: bind(n, op_succ) if f else n, slot_of(k, b.votes)),
        box,
        Enc(0),
    )


def tally_all(ds, box):
    return tuple(tally_at(k, box) for k in ds)


def open_enc(q, c):
    """The plain value of c. The quorum is the public part of the opening."""
    return c.value


def open_tally(q, ds, box):
    return tuple(open_enc(q, c) for c in tally_all(ds, box))


def tally_max(t):
    """The largest count (0 for an empty tally), or None when a count is None."""
    return functools.reduce(lambda m, c: bind(m, lambda a: bind(c, lambda b: max(a, b))), t, 0)


def tally_sum(t):
    """The sum of the counts, or None when a count is None or on a carry."""
    return functools.reduce(lambda m, c: bind(m, lambda a: bind(c, lambda b: nat_add(a, b))), t, 0)


def at_count(m, t):
    """The number of counts equal to m."""
    return functools.reduce(
        lambda n, c: bind(c, lambda b: bind(n, lambda k: nat_add(k, 1)) if b == m else n), t, 0
    )


def pos_of(m, t):
    """The position (from 0) of the first count equal to m.

    The run goes from the last count to the first. It holds 0 until it finds
    m, then 1 at m and one more for each count before it.
    """

    def run(f, c):
        return bind(
            c,
            lambda b: bind(
                f, lambda g: 1 if b == m else (0 if g == 0 else nat_add(g, 1))
            ),
        )

    return bind(functools.reduce(run, reversed(t), 0), lambda g: nat_sub(g, 1))


def rule_plurality(t):
    """The position of the largest count, when only one count has it and it is not 0."""
    return bind(
        tally_max(t),
        lambda m: None
        if m == 0
        else bind(at_count(m, t), lambda k: pos_of(m, t) if k == 1 else None),
    )


def rule_majority(t):
    """The position of the largest count, when it is more than the sum of the others."""
    return bind(
        tally_max(t),
        lambda m: bind(
            tally_sum(t),
            lambda s: bind(nat_sub(s, m), lambda rest: None if m <= rest else pos_of(m, t)),
        ),
    )


def choose_d(r, t):
    return rule_plurality(t) if r == 0 else (rule_majority(t) if r == 1 else None)


def show_nat(o):
    return "none" if o is None else str(o)


def show_enc_count(c):
    return "none" if c is None else (
        "encEta (Option Nat) none" if c.value is None else f"encEta (Option Nat) (some {c.value})"
    )


hot1 = (False, True, False)
hot2 = (False, False, True)
pb_a = mk_policy_ballot(hot1, 11)
pb_b = mk_policy_ballot(hot1, 12)
pb_c = mk_policy_ballot(hot2, 13)
box3 = functools.reduce(
    lambda box, b: bind(box, lambda xs: bind(b, lambda x: xs + (x,))), (pb_a, pb_b, pb_c), ()
)
decs3 = (0, 1, 2)
q22 = Quorum(2, 2)
t_box = bind(box3, lambda box: open_tally(q22, decs3, box))
print(show_nat(bind(t_box, lambda t: choose_d(0, t))))
print(show_nat(bind(t_box, lambda t: choose_d(1, t))))
print(show_enc_count(bind(box3, lambda box: tally_at(1, box))))
