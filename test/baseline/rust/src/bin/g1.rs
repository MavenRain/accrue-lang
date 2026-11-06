//! G1: a retainer opened and countersigned with Schnorr signatures (g1-retainer.lang).
//!
//! An Auth (rx, z) is a 2 of 3 FROST signature on secp256k1. It verifies as one
//! Schnorr signature of the x-only key on the message (tag, engagement digest).
//! A point is lifted from x with the smaller y. engage opens the engagement with
//! the client signature, and countersign sets the signed time with the firm
//! signature. csFundCtl funds 200 at 30, countersigns at 40 and reads the
//! recognized value at 50.
#![allow(dead_code)]
use k256::elliptic_curve::bigint::{CheckedAdd, CheckedSub, Encoding, NonZero, U512};
use k256::elliptic_curve::ops::Reduce;
use k256::elliptic_curve::point::DecompressPoint;
use k256::elliptic_curve::sec1::ToEncodedPoint;
use k256::elliptic_curve::subtle::Choice;
use k256::elliptic_curve::PrimeField;
use k256::{AffinePoint, FieldBytes, ProjectivePoint, Scalar, U256};
use sha2::{Digest, Sha256};

type Word = [u8; 32];
type Time = u64;

const TAG_AUTH: &[u8] = b"accrue-auth";
const TAG_ENGAGE: &[u8] = b"accrue-engage";
const TAG_COUNTERSIGN: &[u8] = b"accrue-countersign";

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

#[derive(Clone)]
struct Book {
    log: Vec<(Time, U256)>,
    withdrawn: U256,
    closed: Option<Time>,
}

#[derive(Clone)]
struct Escrow {
    eng: Engagement,
    book: Book,
}

/// A big-endian 32-byte word with the bytes at the end.
fn word(bytes: &[u8]) -> Word {
    let mut w = [0; 32];
    w.iter_mut().rev().zip(bytes.iter().rev()).for_each(|(d, s)| *d = *s);
    w
}

/// SHA-256 of two big-endian 32-byte words, read as a big-endian word.
fn word_hash(a: &Word, b: &Word) -> Word {
    Sha256::new().chain_update(a).chain_update(b).finalize().into()
}

fn h3(t: &Word, d: &Word, e: &Word) -> Word {
    word_hash(&word_hash(t, d), e)
}

/// The point (x, y) with the smaller y, or None.
fn lift_x(x: &Word) -> Option<AffinePoint> {
    let lift = |odd: u8| -> Option<AffinePoint> {
        AffinePoint::decompress(&FieldBytes::from(*x), Choice::from(odd)).into()
    };
    let y = |p: &AffinePoint| p.to_encoded_point(false).y().map(|y| y.to_vec());
    lift(0).zip(lift(1)).map(|(a, b)| if y(&a) <= y(&b) { a } else { b })
}

/// z G = R + c P, with c the hash chain of (TAG_AUTH, px, rx, head, e) mod N.
fn schnorr_ok(px: &Word, rx: &Word, z: &Word, head: &Word, e: &Word) -> bool {
    let c = word_hash(&word_hash(&word_hash(&word_hash(&word(TAG_AUTH), px), rx), head), e);
    let c = <Scalar as Reduce<U256>>::reduce_bytes(&FieldBytes::from(c));
    let z: Option<Scalar> = Scalar::from_repr(FieldBytes::from(*z)).into();
    lift_x(px).zip(lift_x(rx)).zip(z).is_some_and(|((key, nonce), z)| {
        ProjectivePoint::GENERATOR * z == ProjectivePoint::from(nonce) + ProjectivePoint::from(key) * c
    })
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

/// The book b with (t, f) at the end of its log.
fn put(b: &Book, t: Time, f: U256) -> Book {
    Book { log: b.log.iter().copied().chain([(t, f)]).collect(), withdrawn: b.withdrawn, closed: b.closed }
}

/// Add a to funded (not after a cancel).
fn esc_fund(s: &Stamp, a: U256, x: Escrow) -> Option<Escrow> {
    let t = s.time;
    let open = x.eng.cancelled.is_none() && book_ok(&x.book.log, x.book.closed, t);
    open.then(|| u256_add(funded_at(&x.book.log, t), a))
        .flatten()
        .map(|f| Escrow { eng: x.eng, book: put(&x.book, t, f) })
}

/// The digest of the client, the firm, the asset, the scope and the window.
fn eng_digest(e: &Engagement) -> Word {
    let tag = word(TAG_ENGAGE);
    let time = |t: Time| word(&t.to_be_bytes());
    h3(
        &tag,
        &word_hash(&e.client.to_be_bytes(), &e.firm.to_be_bytes()),
        &h3(
            &tag,
            &word_hash(&e.asset.to_be_bytes(), &e.scope.to_be_bytes()),
            &word_hash(&time(e.window.start), &time(e.window.end)),
        ),
    )
}

/// An escrow with an empty book, when auth is the client signature.
fn engage(cl: U256, fm: U256, asset: U256, scope: U256, wn: Window, (rx, z): (U256, U256)) -> Option<Escrow> {
    let e = Engagement { client: cl, firm: fm, asset, scope, window: wn, signed: None, cancelled: None };
    schnorr_ok(&cl.to_be_bytes(), &rx.to_be_bytes(), &z.to_be_bytes(), &word(TAG_ENGAGE), &eng_digest(&e))
        .then(|| Escrow { eng: e, book: Book { log: Vec::new(), withdrawn: U256::ZERO, closed: None } })
}

/// Set the signed time to the stamp time, when e is not signed, the book is
/// ok at that time and auth is the firm signature.
fn countersign(s: &Stamp, (rx, z): (U256, U256), x: Escrow) -> Option<Escrow> {
    let e = x.eng;
    let ok = book_ok(&x.book.log, x.book.closed, s.time)
        && e.signed.is_none()
        && schnorr_ok(&e.firm.to_be_bytes(), &rx.to_be_bytes(), &z.to_be_bytes(), &word(TAG_COUNTERSIGN), &eng_digest(&e));
    ok.then(|| Escrow { eng: Engagement { signed: Some(s.time), ..e }, book: x.book })
}

fn bk_rec(h: &[(Time, Policy)], t: Time, x: &Escrow) -> Option<U256> {
    recognized_at(h, &x.eng, &x.book.log, t)
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

fn show_opt_time(o: Option<Time>) -> String {
    o.map_or("none".to_string(), |t| format!("(some {t})"))
}

/// A Funding as the kit prints it: fmUnit, fmEta, or fmMul to the left.
fn show_funding(log: &[(Time, U256)]) -> String {
    let eta = |&(t, f): &(Time, U256)| format!("fmEta (Prod Nat U256) (pair {t} {}u)", decimal(&f));
    log.split_first().map_or("fmUnit (Prod Nat U256)".to_string(), |(first, rest)| {
        rest.iter().fold(eta(first), |a, x| format!("fmMul (Prod Nat U256) ({a}) ({})", eta(x)))
    })
}

fn show_escrow(x: &Escrow) -> String {
    let (e, b) = (&x.eng, &x.book);
    format!(
        "escrowOf (engagementOf {}u {}u {}u {}u (windowOf {} {}) {} {}) (bookOf ({}) {}u {})",
        decimal(&e.client),
        decimal(&e.firm),
        decimal(&e.asset),
        decimal(&e.scope),
        e.window.start,
        e.window.end,
        show_opt_time(e.signed),
        show_opt_time(e.cancelled),
        show_funding(&b.log),
        decimal(&b.withdrawn),
        show_opt_time(b.closed)
    )
}

fn show_opt_escrow(o: Option<Escrow>) -> String {
    o.map_or("none".to_string(), |x| format!("some ({})", show_escrow(&x)))
}

fn show_u256(o: Option<U256>) -> String {
    o.map_or("none".to_string(), |x| format!("{}u", decimal(&x)))
}

fn hist_of(i: Instrument) -> Vec<(Time, Policy)> {
    let r_b = Rate {
        segments: vec![Segment { start: 0, until: 50, num: u(2), den: u(1) }],
        masses: vec![Mass { at: 0, amount: u(40) }],
    };
    let grant = Grant { instrument: i, paused: false };
    vec![(0, Policy { rate: r_b, window: Window { start: 0, end: 100 }, grant, dispute: vec![2500, 5000] })]
}

fn s_at(t: Time) -> Stamp {
    Stamp { time: t, prev: U256::ZERO, rx: U256::ZERO, z: U256::ZERO }
}

const C_K: U256 = U256::from_be_hex("F2BB893B2497C632D4B2E1BCC9F3565EFE400130E944EA3A4BA689A46220BAA2");
const F_K: U256 = U256::from_be_hex("FA043432A49AFE1C8E4AD857EE8935E7CAE984E9F655FBD12117D48849D5727D");
const ENG: (U256, U256) = (
    U256::from_be_hex("AE00A0168CDCF5811726C150336492D5BE4EBB5857965DDB7ACDE38FFCA7A33C"),
    U256::from_be_hex("2C8CC28029354CA5E4F7D87C6807BD594A4BB1CE4DE3FB875BBFCF9DBB033828"),
);
const CTR: (U256, U256) = (
    U256::from_be_hex("04C496FBAFA6F2C546472F4B21BCEB13B71945C5B6E6339783A0DB63BE6B38CF"),
    U256::from_be_hex("64E219DEE5E8683EE2FB3F4FF4BB43F5A29B34A0393E883C6B88327E5F1C55CB"),
);

fn main() {
    let h_push = hist_of(Instrument::Push);
    let w_s = Window { start: 10, end: 60 };
    let e_s = Engagement { client: C_K, firm: F_K, asset: u(3), scope: u(4), window: w_s, signed: None, cancelled: None };
    let x_s = Escrow { eng: e_s, book: Book { log: Vec::new(), withdrawn: U256::ZERO, closed: None } };
    let cs_late_fund = esc_fund(&s_at(30), u(200), x_s.clone());
    let cs_fund_at = |t: Time| {
        cs_late_fund.clone().and_then(|p| countersign(&s_at(t), CTR, p)).and_then(|x| bk_rec(&h_push, 50, &x))
    };
    println!("{}", show_opt_escrow(engage(C_K, F_K, u(3), u(4), w_s, ENG)));
    println!("{}", show_opt_escrow(countersign(&s_at(20), CTR, x_s)));
    println!("{}", show_u256(cs_fund_at(40)));
}
