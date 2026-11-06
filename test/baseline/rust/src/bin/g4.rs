//! G4: the rate changes during the engagement (g4-rate.lang).
//!
//! hA holds pA at 0 and pB at 30, and hC adds pC at 45. Earned at t is the sum
//! of the entry terms of the history. An entry (ti, p) is in force on
//! (ti, next], and the windows, the signed time and t cut it. Earned at each
//! t <= 45 is the same for hA and hC, and after 45 pC has an effect.
#![allow(dead_code)]
use k256::elliptic_curve::bigint::{CheckedAdd, CheckedSub, Encoding, NonZero, U512};
use k256::U256;

type Time = u64;

#[derive(Clone, Copy, PartialEq)]
enum Instrument {
    Push,
    Pull,
    Stream,
}

#[derive(Clone, Copy)]
struct Segment {
    start: Time,
    until: Time,
    num: U256,
    den: U256,
}

#[derive(Clone, Copy)]
struct Mass {
    at: Time,
    amount: U256,
}

#[derive(Clone)]
struct Rate {
    segments: Vec<Segment>,
    masses: Vec<Mass>,
}

#[derive(Clone, Copy)]
struct Window {
    start: Time,
    end: Time,
}

#[derive(Clone, Copy)]
struct Grant {
    instrument: Instrument,
    paused: bool,
}

#[derive(Clone)]
struct Policy {
    rate: Rate,
    window: Window,
    grant: Grant,
    dispute: Vec<u64>,
}

#[derive(Clone, Copy)]
struct Engagement {
    client: U256,
    firm: U256,
    asset: U256,
    scope: U256,
    window: Window,
    signed: Option<Time>,
    cancelled: Option<Time>,
}

fn u(n: u64) -> U256 {
    U256::from_u64(n)
}

/// a + b, or None on an overflow.
fn u256_add(a: U256, b: U256) -> Option<U256> {
    Option::from(a.checked_add(&b))
}

/// a - b, or None when b is more than a.
fn u256_sub(a: U256, b: U256) -> Option<U256> {
    Option::from(a.checked_sub(&b))
}

/// floor(a * b / d), or None when the quotient is not a 256-bit word.
fn mul_div(a: U256, b: U256, d: U256) -> Option<U256> {
    let (lo, hi) = a.mul_wide(&b);
    Option::<NonZero<U512>>::from(NonZero::new(U256::ZERO.concat(&d))).and_then(|d| {
        let (q_hi, q_lo) = hi.concat(&lo).div_rem(&d).0.split();
        (q_hi == U256::ZERO).then_some(q_lo)
    })
}

/// True when o is None or o is before t.
fn later(t: Time, o: Option<Time>) -> bool {
    o.is_none_or(|o| o < t)
}

/// min(x, o); a None bound has no effect.
fn opt_min(x: Time, o: Option<Time>) -> Time {
    o.map_or(x, |o| x.min(o))
}

/// max(x, o); a None bound has no effect.
fn opt_max(x: Time, o: Option<Time>) -> Time {
    o.map_or(x, |o| x.max(o))
}

/// An Acc is a sum, or None after a None term or an overflow.
fn acc_add(acc: Option<U256>, term: Option<U256>) -> Option<U256> {
    acc.zip(term).and_then(|(total, term)| u256_add(total, term))
}

/// floor(num * (clamp(x, start, until) - start) / den).
///
/// None only when until is before start (a bad Segment).
fn seg_term(s: &Segment, x: Time) -> Option<U256> {
    x.max(s.start).min(s.until).checked_sub(s.start).and_then(|d| mul_div(s.num, u(d), s.den))
}

/// The sum of the segment terms at the offset x, and each mass at x or before.
fn g_at(r: &Rate, x: Time) -> Option<U256> {
    let segs = r.segments.iter().fold(Some(U256::ZERO), |a, s| acc_add(a, seg_term(s, x)));
    r.masses.iter().fold(segs, |a, m| if m.at <= x { acc_add(a, Some(m.amount)) } else { a })
}

/// G(hi - s) - G(lo - s) on (lo, hi], or G(hi - s) on the closed [s, hi].
///
/// An empty interval gives 0 without evaluating G.
fn accrue(r: &Rate, s: Time, lo: Time, hi: Time, closed: bool) -> Option<U256> {
    let g = |x: Time| x.checked_sub(s).and_then(|d| g_at(r, d));
    match () {
        () if !(lo < hi || (closed && lo == hi)) => Some(U256::ZERO),
        () if closed => g(hi),
        () => g(hi).and_then(|gh| g(lo).and_then(|gl| u256_sub(gh, gl))),
    }
}

/// The term of the entry (ti, p), in force on (ti, nxt].
///
/// A paused grant or no signed time adds 0. An entry in force at the start
/// S of e adds G on the closed [S, hi], so a mass at the offset 0 counts.
fn entry_term(e: &Engagement, lower: Option<Time>, t: Time, nxt: Option<Time>, ti: Time, p: &Policy) -> Option<U256> {
    e.signed.filter(|_| !p.grant.paused).map_or(Some(U256::ZERO), |sg| {
        let start = e.window.start;
        let lo = opt_max(ti.max(start).max(p.window.start).max(sg), lower);
        let hi = opt_min(opt_min(e.window.end.min(p.window.end).min(t), nxt), e.cancelled);
        let closed = ti < start && p.window.start <= start && sg <= start && later(start, lower);
        accrue(&p.rate, start, lo, hi, closed)
    })
}

/// The sum of the entry terms of h, from the last entry to the first.
fn earned_from(lower: Option<Time>, h: &[(Time, Policy)], e: &Engagement, t: Time) -> Option<U256> {
    h.iter()
        .rev()
        .fold((Some(U256::ZERO), None), |(acc, nxt), (ti, p)| {
            (acc_add(acc, entry_term(e, lower, t, nxt, *ti, p)), Some(*ti))
        })
        .0
}

fn earned_at(h: &[(Time, Policy)], e: &Engagement, t: Time) -> Option<U256> {
    earned_from(None, h, e, t)
}

/// The decimal digits of x: each big-endian byte b takes the digits d (from
/// the lowest) to d * 256 + b.
fn decimal(x: &U256) -> String {
    let digits = x.to_be_bytes().iter().fold(Vec::<u32>::new(), |ds, b| {
        let (low, carry) = ds.iter().fold((Vec::new(), u32::from(*b)), |(low, c), d| {
            let v = d * 256 + c;
            ([low, vec![v % 10]].concat(), v / 10)
        });
        let high = std::iter::successors(Some(carry).filter(|c| *c > 0), |c| Some(c / 10).filter(|c| *c > 0));
        low.into_iter().chain(high.map(|c| c % 10)).collect()
    });
    let text: String = digits.iter().rev().filter_map(|d| char::from_digit(*d, 10)).collect();
    if text.is_empty() { "0".to_string() } else { text }
}

fn show_u256(o: Option<U256>) -> String {
    o.map_or("none".to_string(), |x| format!("{}u", decimal(&x)))
}

fn main() {
    let grant_on = Grant { instrument: Instrument::Stream, paused: false };
    let seg = |start, until, num, den| Segment { start, until, num: u(num), den: u(den) };
    let mass = |at, amount| Mass { at, amount: u(amount) };
    let policy = |rate| Policy { rate, window: Window { start: 0, end: 100 }, grant: grant_on, dispute: Vec::new() };
    let r_a = Rate { segments: vec![seg(0, 20, 3, 2), seg(20, 40, 1, 1)], masses: vec![mass(0, 50), mass(25, 7)] };
    let r_b = Rate { segments: vec![seg(0, 50, 2, 1)], masses: vec![mass(30, 9)] };
    let p_a = policy(r_a);
    let p_b = policy(r_b);
    let p_c = policy(Rate { segments: vec![seg(0, 50, 5, 1)], masses: Vec::new() });
    let e_a = Engagement {
        client: u(1),
        firm: u(2),
        asset: u(3),
        scope: u(4),
        window: Window { start: 10, end: 60 },
        signed: Some(10),
        cancelled: None,
    };
    let h_a = vec![(0, p_a), (30, p_b)];
    let h_c = [h_a.clone(), vec![(45, p_c)]].concat();
    println!("{}", show_u256(earned_at(&h_a, &e_a, 37)));
    println!("{}", show_u256(earned_at(&h_c, &e_a, 45)));
    println!("{}", show_u256(earned_at(&h_c, &e_a, 50)));
}
