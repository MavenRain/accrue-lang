//! G5: the DAO amends tAmend and the rule (g5-amend.lang).
//!
//! The polity s0 has 3 members, each threshold two and the plurality rule
//! (rule 0). A step enacts a proposal when its yes count meets tAmend, else it
//! keeps the polity. sLow lowers tAmend to one, so a yes count of one then
//! enacts pMaj (the majority rule, rule 1). govC gives the decision of a tally
//! under the rule of the polity in force, or None when there is no decision.
#![allow(dead_code)]

#[derive(Clone)]
struct Polity {
    members: Vec<u64>,
    t_append: u64,
    t_amend: u64,
    t_open: u64,
    key_open: u64,
    key_append: u64,
    t_stamp: u64,
    t_pay: u64,
    key_stamp: u64,
    key_pay: u64,
    rule: u64,
}

fn step(polity: Polity, (proposal, yes): (Polity, u64)) -> Polity {
    if polity.t_amend <= yes { proposal } else { polity }
}

/// The largest count (0 for an empty tally), or None when a count is None.
fn tally_max(t: &[Option<u64>]) -> Option<u64> {
    t.iter().fold(Some(0), |m, c| m.zip(*c).map(|(a, b)| a.max(b)))
}

/// The sum of the counts, or None when a count is None or on a carry.
fn tally_sum(t: &[Option<u64>]) -> Option<u64> {
    t.iter().fold(Some(0), |m, c| m.zip(*c).and_then(|(a, b)| a.checked_add(b)))
}

/// The number of counts equal to m.
fn at_count(m: u64, t: &[Option<u64>]) -> Option<u64> {
    t.iter().fold(Some(0), |n, c| c.and_then(|b| if b == m { n.and_then(|k| k.checked_add(1)) } else { n }))
}

/// The position (from 0) of the first count equal to m.
///
/// The run goes from the last count to the first. It holds 0 until it finds
/// m, then 1 at m and one more for each count before it.
fn pos_of(m: u64, t: &[Option<u64>]) -> Option<u64> {
    t.iter()
        .rev()
        .fold(Some(0_u64), |f, c| {
            c.zip(f).and_then(|(b, g)| match () {
                () if b == m => Some(1),
                () if g == 0 => Some(0),
                () => g.checked_add(1),
            })
        })
        .and_then(|g| g.checked_sub(1))
}

/// The position of the largest count, when only one count has it and it is not 0.
fn rule_plurality(t: &[Option<u64>]) -> Option<u64> {
    tally_max(t)
        .filter(|m| *m != 0)
        .and_then(|m| at_count(m, t).filter(|k| *k == 1).and_then(|_| pos_of(m, t)))
}

/// The position of the largest count, when it is more than the sum of the others.
fn rule_majority(t: &[Option<u64>]) -> Option<u64> {
    tally_max(t).and_then(|m| {
        tally_sum(t)
            .and_then(|s| s.checked_sub(m))
            .filter(|rest| m > *rest)
            .and_then(|_| pos_of(m, t))
    })
}

fn choose_d(r: u64, t: &[Option<u64>]) -> Option<u64> {
    match () {
        () if r == 0 => rule_plurality(t),
        () if r == 1 => rule_majority(t),
        () => None,
    }
}

/// The decision of the rule r, when it is one of the n policies.
fn gov_d(n: u64, r: u64, t: &[Option<u64>]) -> Option<u64> {
    choose_d(r, t).filter(|d| *d < n)
}

fn gov_c(s: &Polity, n: u64, t: &[Option<u64>]) -> Option<u64> {
    gov_d(n, s.rule, t)
}

fn show_nat(o: Option<u64>) -> String {
    o.map_or("none".to_string(), |n| n.to_string())
}

fn main() {
    let s0 = Polity {
        members: vec![1, 2, 3],
        t_append: 2,
        t_amend: 2,
        t_open: 2,
        key_open: 1,
        key_append: 2,
        t_stamp: 3,
        t_pay: 4,
        key_stamp: 5,
        key_pay: 6,
        rule: 0,
    };
    let p_maj = Polity { rule: 1, ..s0.clone() };
    let p_amend1 = Polity { t_amend: 1, ..s0.clone() };
    let t_split = [Some(3), Some(2), Some(2)];
    let s_maj = step(s0.clone(), (p_maj.clone(), 2));
    let s_low = step(s0.clone(), (p_amend1, 2));
    let s_low_maj = step(s_low.clone(), (p_maj, 1));
    println!("{}", s_low.t_amend);
    println!("{}", s_low_maj.rule);
    println!("{}", show_nat(gov_c(&s0, 3, &t_split)));
    println!("{}", show_nat(gov_c(&s_maj, 3, &t_split)));
}
