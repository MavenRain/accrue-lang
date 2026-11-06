"""G5: the DAO amends tAmend and the rule (g5-amend.lang).

The polity s0 has 3 members, each threshold two and the plurality rule
(rule 0). A step enacts a proposal when its yes count meets tAmend, else it
keeps the polity. sLow lowers tAmend to one, so a yes count of one then
enacts pMaj (the majority rule, rule 1). govC gives the decision of a tally
under the rule of the polity in force, or None when there is no decision.
"""
import functools
from typing import NamedTuple

NAT_END = 1 << 64


class Polity(NamedTuple):
    members: tuple
    t_append: int
    t_amend: int
    t_open: int
    key_open: int
    key_append: int
    t_stamp: int
    t_pay: int
    key_stamp: int
    key_pay: int
    rule: int


def bind(o, f):
    return None if o is None else f(o)


def nat_add(a, b):
    """a + b, or None on a carry."""
    total = a + b
    return total if total < NAT_END else None


def nat_sub(a, b):
    """a - b, or None when b is more than a."""
    return a - b if b <= a else None


def step(polity, decided):
    proposal, yes = decided
    return proposal if polity.t_amend <= yes else polity


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


def gov_d(n, r, t):
    """The decision of the rule r, when it is one of the n policies."""
    return bind(choose_d(r, t), lambda d: None if n <= d else d)


def gov_c(s, n, t):
    return gov_d(n, s.rule, t)


def show_nat(o):
    return "none" if o is None else str(o)


crew = (1, 2, 3)
s0 = Polity(crew, 2, 2, 2, 1, 2, 3, 4, 5, 6, 0)
p_maj = Polity(crew, 2, 2, 2, 1, 2, 3, 4, 5, 6, 1)
p_amend1 = Polity(crew, 2, 1, 2, 1, 2, 3, 4, 5, 6, 0)
t_split = (3, 2, 2)
s_maj = step(s0, (p_maj, 2))
s_low = step(s0, (p_amend1, 2))
s_low_maj = step(s_low, (p_maj, 1))
print(s_low.t_amend)
print(s_low_maj.rule)
print(show_nat(gov_c(s0, 3, t_split)))
print(show_nat(gov_c(s_maj, 3, t_split)))
