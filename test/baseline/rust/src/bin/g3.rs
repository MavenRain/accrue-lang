//! G3: an encrypted policy vote (g3-vote.lang).
//!
//! Enc is the cell of an encrypted value: op2 joins two encrypted values under
//! the seal, thus the tally of a decision stays encrypted. A policy ballot holds
//! one encrypted bit for each decision (exactly one yes) and a link tag. A
//! quorum opens the tally of each decision, and the rule (0 plurality, 1
//! majority) gives the choice.
#![allow(dead_code)]

#[derive(Clone, Copy)]
struct Enc<T>(T);

impl<T> Enc<T> {
    fn op2<U, V>(self, f: impl FnOnce(T, U) -> V, other: Enc<U>) -> Enc<V> {
        Enc(f(self.0, other.0))
    }
}

struct PolicyBallot {
    votes: Vec<Enc<bool>>,
    tag: u64,
}

struct Quorum {
    t: u64,
    n: u64,
}

fn op_succ(n: u64) -> Option<u64> {
    n.checked_add(1)
}

/// The number of yes votes, or None on a carry.
fn count_votes(vs: &[bool]) -> Option<u64> {
    vs.iter().fold(Some(0), |n, f| if *f { n.and_then(op_succ) } else { n })
}

/// The ballot of the plain bits vs, or None when vs has not exactly one yes.
fn mk_policy_ballot(vs: &[bool], tag: u64) -> Option<PolicyBallot> {
    (count_votes(vs) == Some(1)).then(|| PolicyBallot { votes: vs.iter().map(|v| Enc(*v)).collect(), tag })
}

/// The bit of decision k, or an encrypted no after the end.
fn slot_of(k: usize, cs: &[Enc<bool>]) -> Enc<bool> {
    cs.get(k).copied().unwrap_or(Enc(false))
}

/// The encrypted count of the yes bits of decision k in the box.
fn tally_at(k: usize, ballots: &[PolicyBallot]) -> Enc<Option<u64>> {
    ballots.iter().fold(Enc(Some(0)), |t, b| {
        t.op2(|n, f| if f { n.and_then(op_succ) } else { n }, slot_of(k, &b.votes))
    })
}

fn tally_all(ds: &[usize], ballots: &[PolicyBallot]) -> Vec<Enc<Option<u64>>> {
    ds.iter().map(|k| tally_at(*k, ballots)).collect()
}

/// The plain value of c. The quorum is the public part of the opening.
fn open_enc<T>(_q: &Quorum, c: Enc<T>) -> T {
    c.0
}

fn open_tally(q: &Quorum, ds: &[usize], ballots: &[PolicyBallot]) -> Vec<Option<u64>> {
    tally_all(ds, ballots).into_iter().map(|c| open_enc(q, c)).collect()
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
    t.iter().fold(Some(0), |n, c| c.and_then(|b| if b == m { n.and_then(op_succ) } else { n }))
}

/// The position (from 0) of the first count equal to m.
///
/// The run goes from the last count to the first. It holds 0 until it finds
/// m, then 1 at m and one more for each count before it.
fn pos_of(m: u64, t: &[Option<u64>]) -> Option<u64> {
    t.iter()
        .rev()
        .fold(Some(0), |f, c| {
            c.zip(f).and_then(|(b, g)| match () {
                () if b == m => Some(1),
                () if g == 0 => Some(0),
                () => op_succ(g),
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

fn show_nat(o: Option<u64>) -> String {
    o.map_or("none".to_string(), |n| n.to_string())
}

fn show_enc_count(o: Option<Enc<Option<u64>>>) -> String {
    o.map_or("none".to_string(), |c| {
        c.0.map_or("encEta (Option Nat) none".to_string(), |v| format!("encEta (Option Nat) (some {v})"))
    })
}

fn main() {
    let hot1 = [false, true, false];
    let hot2 = [false, false, true];
    let box3: Option<Vec<PolicyBallot>> =
        [mk_policy_ballot(&hot1, 11), mk_policy_ballot(&hot1, 12), mk_policy_ballot(&hot2, 13)]
            .into_iter()
            .collect();
    let decs3 = [0, 1, 2];
    let q22 = Quorum { t: 2, n: 2 };
    let t_box = box3.as_ref().map(|b| open_tally(&q22, &decs3, b));
    println!("{}", show_nat(t_box.as_ref().and_then(|t| choose_d(0, t))));
    println!("{}", show_nat(t_box.as_ref().and_then(|t| choose_d(1, t))));
    println!("{}", show_enc_count(box3.as_ref().map(|b| tally_at(1, b))));
}
