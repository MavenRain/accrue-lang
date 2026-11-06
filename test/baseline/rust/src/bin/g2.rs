//! G2: a closed linear stream and a cancel (g2-cancel.lang).
//!
//! hL streams 1000 from 10 to 17, so earned at the end is the deposit. On the
//! push history, the client funds 200 at 10 and the firm settles 30 at 20.
//! The cancel at 30 pays the unearned amount (funded - recognized) back to the
//! client, and the log gets (30, recognized).
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

#[derive(Clone, Copy)]
struct Stamp {
    time: Time,
    prev: U256,
    rx: U256,
    z: U256,
}

#[derive(Clone, Copy)]
struct Instruction {
    payee: U256,
    asset: U256,
    amount: U256,
    at: Stamp,
}

struct Book {
    log: Vec<(Time, U256)>,
    withdrawn: U256,
    closed: Option<Time>,
}

struct Escrow {
    eng: Engagement,
    book: Book,
}

/// An escrow step: the escrow after, and the payout.
type Step = Option<(Escrow, Vec<Instruction>)>;

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

/// The funded amount of the last entry at t or before, or 0 when there is none.
fn funded_at(log: &[(Time, U256)], t: Time) -> U256 {
    log.iter().fold(U256::ZERO, |f, (ti, fu)| if *ti <= t { *fu } else { f })
}

/// True when no entry of the log is after t.
fn log_ok(log: &[(Time, U256)], t: Time) -> bool {
    log.iter().all(|(ti, _)| *ti <= t)
}

/// log_ok, and t is after the last close c.
fn book_ok(log: &[(Time, U256)], c: Option<Time>, t: Time) -> bool {
    log_ok(log, t) && later(t, c)
}

/// min(funded t, earned t).
fn recognized_at(h: &[(Time, Policy)], e: &Engagement, log: &[(Time, U256)], t: Time) -> Option<U256> {
    earned_at(h, e, t).map(|x| funded_at(log, t).min(x))
}

/// funded t - recognized t.
fn unearned_at(h: &[(Time, Policy)], e: &Engagement, log: &[(Time, U256)], t: Time) -> Option<U256> {
    recognized_at(h, e, log, t).and_then(|r| u256_sub(funded_at(log, t), r))
}

/// The grant of the policy in force at t (the last entry before t) holds pull.
fn pull_at(h: &[(Time, Policy)], t: Time) -> bool {
    h.iter().fold(false, |v, (ti, p)| if *ti < t { p.grant.instrument == Instrument::Pull } else { v })
}

/// The book b with (t, f) at the end of its log.
fn put(b: &Book, t: Time, f: U256) -> Book {
    Book { log: b.log.iter().copied().chain([(t, f)]).collect(), withdrawn: b.withdrawn, closed: b.closed }
}

/// Add a to funded (not after a cancel).
fn esc_fund(s: &Stamp, a: U256, x: Escrow) -> Step {
    let t = s.time;
    let open = x.eng.cancelled.is_none() && book_ok(&x.book.log, x.book.closed, t);
    open.then(|| u256_add(funded_at(&x.book.log, t), a))
        .flatten()
        .map(|f| (Escrow { eng: x.eng, book: put(&x.book, t, f) }, Vec::new()))
}

/// Pay v to the firm.
///
/// Every settle logs the funded amount at its time, and a pull settle also
/// adds v to funded. The guard is on the book after: withdrawn + v <=
/// recognized after.
fn esc_settle(h: &[(Time, Policy)], s: &Stamp, v: U256, x: Escrow) -> Step {
    let t = s.time;
    let funded = funded_at(&x.book.log, t);
    let f = if pull_at(h, t) { u256_add(funded, v) } else { Some(funded) };
    book_ok(&x.book.log, x.book.closed, t)
        .then(|| f.zip(u256_add(x.book.withdrawn, v)))
        .flatten()
        .map(|(f, w2)| Book { withdrawn: w2, ..put(&x.book, t, f) })
        .filter(|b2| recognized_at(h, &x.eng, &b2.log, t).is_some_and(|r| b2.withdrawn <= r))
        .map(|b2| {
            let pay = Instruction { payee: x.eng.firm, asset: x.eng.asset, amount: v, at: *s };
            (Escrow { eng: x.eng, book: b2 }, vec![pay])
        })
}

/// Cancel at t (not after a cancel).
///
/// The client gets unearned t, and the log gets (t, funded - unearned).
fn esc_cancel(h: &[(Time, Policy)], s: &Stamp, x: Escrow) -> Step {
    let t = s.time;
    let open = x.eng.cancelled.is_none() && book_ok(&x.book.log, x.book.closed, t);
    open.then(|| unearned_at(h, &x.eng, &x.book.log, t))
        .flatten()
        .and_then(|u| u256_sub(funded_at(&x.book.log, t), u).map(|f| (u, f)))
        .map(|(u, f)| {
            let pay = Instruction { payee: x.eng.client, asset: x.eng.asset, amount: u, at: *s };
            (Escrow { eng: Engagement { cancelled: Some(t), ..x.eng }, book: put(&x.book, t, f) }, vec![pay])
        })
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

fn show_instruction(i: &Instruction) -> String {
    let s = &i.at;
    format!(
        "instructionOf {}u {}u {}u (stampOf {} {}u {}u {}u)",
        decimal(&i.payee),
        decimal(&i.asset),
        decimal(&i.amount),
        s.time,
        decimal(&s.prev),
        decimal(&s.rx),
        decimal(&s.z)
    )
}

/// A Payout as the kit prints it: fmUnit, fmEta, or fmMul to the left.
fn show_payout(p: &[Instruction]) -> String {
    let eta = |i: &Instruction| format!("fmEta Instruction ({})", show_instruction(i));
    p.split_first().map_or("fmUnit Instruction".to_string(), |(first, rest)| {
        rest.iter().fold(eta(first), |a, i| format!("fmMul Instruction ({a}) ({})", eta(i)))
    })
}

fn show_pay(o: Option<Vec<Instruction>>) -> String {
    o.map_or("none".to_string(), |p| format!("some ({})", show_payout(&p)))
}

fn r_b() -> Rate {
    Rate {
        segments: vec![Segment { start: 0, until: 50, num: u(2), den: u(1) }],
        masses: vec![Mass { at: 0, amount: u(40) }],
    }
}

fn hist_of(i: Instrument) -> Vec<(Time, Policy)> {
    let grant = Grant { instrument: i, paused: false };
    vec![(0, Policy { rate: r_b(), window: Window { start: 0, end: 100 }, grant, dispute: vec![2500, 5000] })]
}

fn s_at(t: Time) -> Stamp {
    Stamp { time: t, prev: U256::ZERO, rx: U256::ZERO, z: U256::ZERO }
}

fn main() {
    let grant_on = Grant { instrument: Instrument::Stream, paused: false };
    let e_l = Engagement {
        client: u(1),
        firm: u(2),
        asset: u(3),
        scope: u(4),
        window: Window { start: 10, end: 17 },
        signed: Some(10),
        cancelled: None,
    };
    let r_l = Rate { segments: vec![Segment { start: 0, until: 7, num: u(1000), den: u(7) }], masses: Vec::new() };
    let h_l = vec![(0, Policy { rate: r_l, window: Window { start: 0, end: 100 }, grant: grant_on, dispute: Vec::new() })];
    println!("{}", show_u256(earned_at(&h_l, &e_l, 12)));
    println!("{}", show_u256(earned_at(&h_l, &e_l, 17)));

    let h_push = hist_of(Instrument::Push);
    let e_b = Engagement { window: Window { start: 10, end: 60 }, ..e_l };
    let x_e = Escrow { eng: e_b, book: Book { log: Vec::new(), withdrawn: U256::ZERO, closed: None } };
    let x_c = esc_fund(&s_at(10), u(200), x_e)
        .and_then(|(x, _)| esc_settle(&h_push, &s_at(20), u(30), x))
        .and_then(|(x, _)| esc_cancel(&h_push, &s_at(30), x));
    println!("{}", show_pay(x_c.map(|(_, p)| p)));
}
