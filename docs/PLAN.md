# accrue-lang plan

accrue-lang is a Cubical TT language for one job: off-chain retainer
protocols. A client funds an engagement, a rate accrues over a window, and
the firm settles what it earned. A threshold DAO governs the policy (rate,
window, authorization, dispute) off chain: it encrypts its ballots (FHE),
proves each step (ZK) and opens only by threshold (MPC). The DAO can amend
its own rules. No program can trap.

The design is `design/DESIGN.md`: the request, verbatim, and the
retainer-lang domain design, verbatim. This file gives the readings of that
request, the route, the slices and the open questions. Section 12 lists the
questions for the USER. A reading that is not ruled stays a
recommendation.

Status: S0 DONE (seed 92349c9). S1 DONE: the USER took each
recommendation of section 12 (section 1). S2 DONE: the kernel is ported
at cairn-lang 0743108, the tags are renamed, the time parts are removed.
S3 DONE: the kernel is total. Each partial operation gives an `Option`
or takes a `NonZero` divisor, `REFUSE_PARTIAL` refuses a partial use,
and no value is a trap (section 2, Q-S3-1 to Q-S3-21).
S4 DONE: a program reads time only as a `Stamp` argument.
`REFUSE_CLOCK` refuses a clock word, and `stampSnoc` adds a stamp to the
chain only with a good signature, a later t and the right prev; L12
(section 5, Q-S4-1 to Q-S4-5).
S5a DONE: Part E is data (Segment, Mass, Rate, Window, Instrument,
Grant, Policy, Engagement, History). G (`gAt`) and `earnedAt` read
retainer-lang SPEC 6.1. L1, L2 and L6 are refl on closed values
(section 1, Q-S5-1 to Q-S5-10).
S5b DONE: a `Book` is a funding log, the withdrawn amount and the last
close. The ops `escFund`, `escSettle`, `escCancel`, `escDispute` and
`escClose` give an `Option` result. An op that moves value gives
`Instruction` data and does not move it. The CC dual is the cofamily
`Books` and `booksPair`. L3, L4 and L5 are refl on closed values
(section 1, Q-S5b-1 to Q-S5b-5). S5 DONE.
Next: S6 (Part D: PolicyBallot, Tally, the `choice` table, `GovPi`, the
regimes, AmendBallot, reshare; L7 to L11; section 4.2).

## 1. Rulings

| Date | Ruling | Source |
|---|---|---|
| 2026-10-27 | Targets: Wasm MVP (tcc-wasm kit), ES module + JSON (tcc-js kit), WGSL compute (tcc-webgpu kit). Wasm is the primary target. The gate runs it first, and the differential tests compare it with `langc eval`. | DESIGN.md, S0 answers |
| 2026-10-27 | Name: accrue-lang. | DESIGN.md, S0 answers |
| 2026-10-27 | Host: TinyCC. All of `langc` is C99 that TinyCC builds, `langc mcp` too. | DESIGN.md, request |
| 2026-10-27 | S1, verbatim: "take recommendations". So each S0 question takes (a): Q-S0-1 the cairn-lang crypto (secp256k1, SHA-256, exponential ElGamal, FROST, Pedersen DKG with Feldman commitments); Q-S0-2 a stamp chain signed by a `tStamp` threshold of the DAO; Q-S0-3 plain `U256` amounts, only the ballots encrypted; Q-S0-4 the protocol holds no money, and each value move is an `Instruction` with a threshold `PayAuth` on the record; Q-S0-5 one-hot policy ballots over `D`; Q-S0-6 the DAO amends the whole `Constitution`, `rule` too; Q-S0-7 a capacity for each run-time list, and static memory; Q-S0-8 the Anthropic `count_tokens` endpoint, and the Python and Rust programs run with outputs equal to the accrue-lang output. | USER, S1 |
| 2026-10-27 | Q-S2-1, verbatim: "(a) Port, then rename (Recommended)". S2 ports the cairn-lang bytes first and runs the gate. Then it renames the domain tags from cairn to accrue and makes the test vectors again with the same tools. | USER, S2 |
| 2026-10-27 | Q-S2-2, verbatim: "(a) Remove in S2 (Recommended)". S2 removes `REFUSE_TIME` and `OpPos`. A guest writes `Count` for a position. | USER, S2 |
| 2026-10-28 | Q-S3-1, verbatim: "(a) All programs, two sites (Recommended)". `REFUSE_PARTIAL` applies to kit, domain and guest programs at two sites: (1) a name with no total kit word (`div`, `mod`, `sub`, `mul`, `head`, `tail`, `pred`), (2) the `Option` result of a checked word where its payload type is wanted. The hint names the total form, or `option` and `bind`. | USER, S3 |
| 2026-10-28 | Q-S3-2, verbatim: "(a) Option in place (Recommended)". `u256Add` and a new `u256Mul` return `Option U256` (`none` above 2^256 - 1). No wrapping or trapping form stays. | USER, S3 |
| 2026-10-28 | Q-S3-3, verbatim: "(a) Option now, proof later (Recommended)". `natSub` and `u256Sub` return an `Option` (`none` when b > a). The floor-at-0 form goes. The residual with a proof argument waits (O8). | USER, S3 |
| 2026-10-28 | Q-S3-4, verbatim: "(b) NonZero divisor type". A kit type `NonZero` and `nonZero`, which returns an `Option`. `natDiv` and `natMod` take a `NonZero` divisor and stay total. | USER, S3 |
| 2026-10-28 | Q-S3-5, verbatim: "(a) In S3, NonZero over U256 (Recommended)". `mulDiv : U256 -> U256 -> NonZero U256 -> Option U256` = floor(a * b / d) with an exact 512-bit product, `none` only at a quotient above 2^256 - 1. `NonZero` takes its base type (`NonZero Nat`, `NonZero U256`); `nonZero` makes either one; `nonZeroValue` gives the base value back. A C tool tests 512-bit rows. | USER, S3 |
| 2026-10-28 | Q-S3-6, verbatim: "(a) Option in place (Recommended)". `natAdd` returns `Option Nat` (`none` above 2^64 - 1). Eval keeps no 2^64 - 1 stop. Each use takes the `Option` apart with `option` or `bind`. | USER, S3 |
| 2026-10-28 | Q-S3-7, verbatim: "(b) All in S3". S3 removes all 14 trap sites and `VAL_TRAP`. The crypto words (`ecAdd`, `ecMul`, the zk ring, `zkHp`, `zkSign`, the bLSAG hash, the bLSAG and ZK witness traps, `zkCheck`) return an `Option` now and can change again in S6. | USER, S3 |
| 2026-10-28 | Q-S3-8, verbatim: "(a) Checker takes a non-0 literal (Recommended)". Where a `NonZero` is wanted, a `Nat` or `U256` literal other than 0 checks as a `NonZero`. A literal 0 there is `REFUSE_PARTIAL` with the hint `nonZero`. A run-time value uses `nonZero`. | USER, S3 |
| 2026-10-28 | Q-S3-9, verbatim: "(a) Option entries (Recommended)". An entry can have the result type `Option Nat` or `Option Flag`. `langc eval` prints the payload or `none` (EVAL rows `=> none`). A `langc run` step reverts on `none`. `ratio` and `remainder` return `Option Nat` again. | USER, S3 |
| 2026-10-28 | Q-S3-10 (a). A view can return an `Option` of a view type. `langc run` prints `= none` or the payload, as in Q-S3-9. | USER, S3 |
| 2026-10-28 | Q-S3-11 (a). A new trusted constant `optNatIsSet : isSet (Option Nat)` (`docs/VALIDATION.md`). Not (b) `Prod Nat Flag`: (b) loses the refl law `l2` of `examples/record.lang`. | USER, S3 |
| 2026-10-28 | Q-S3-12 (a). Drop `zk_unroll`. An open `zkVerify` stays one stuck application. `src/front/lower.c` has no ec, hash or zk case, so no writer here emits the 6n precompile calls. | USER, S3 |
| 2026-10-28 | Q-S3-13 (a). `zkVerify` gives `Option Flag`: `none` on a bad ring and on the old bLSAG trap case (Q-S3-20). `flagNo` stays for a wrong length or a rejected signature. | USER, S3 |
| 2026-10-28 | Q-S3-14 (b). `zkCheck` gives `Option (Type 0)`: `none` on a bad member ring and on the old bLSAG trap case, `some unit` on accept. A reject stays the stuck (neutral) `zkCheck` application. | USER, S3 |
| 2026-10-28 | Q-S3-15 (a). The `zk_trap` rows of `test/fronttool.c` become `zk_none`. The bad point is `pair 1u 1u` (not on the curve). `zkHp`, `zkDigest` and `zkVerify` on it give `none`. The 2 rows inside `flagIf` go. | USER, S3 |
| 2026-10-28 | Q-S3-16 (a). `langc prove` on a ZK witness with no result prints `none` and exits 0, with no `EVAL_OVERFLOW` line. `ZK_WITNESS_TRAP` becomes `ZK_WITNESS_NONE`. | USER, S3 |
| 2026-10-28 | Q-S3-17 (a). The legacy trap names get new names: `addTrap` and `eqTrap` become `addNone` and `eqNone`; `upTrap`, `trapHead`, `trapSum`, `filterTrap` and `bindTrap` become `upMax`, `headMax`, `sumMax`, `filterMax` and `bindMax`. | USER, S3 |
| 2026-10-29 | Q-S3-18, verbatim: "(a) Defer to S8 (Recommended)". S3 makes no IR code edit. Section 11 (O9) keeps the N4 rule for the S8 writer. The entry ABI of an `Option` result is an open S8 question. | USER, S3 |
| 2026-10-29 | Q-S3-19, verbatim: "(a) Keep for S8 (Recommended)". S3 makes no edit in `test/ir`. The Q-S3-17 names do not apply there. | USER, S3 |
| 2026-10-29 | Q-S3-20, verbatim: "(a) Rename to BLSAG_NONE (Recommended)". `BLSAG_TRAP` becomes `BLSAG_NONE`. | USER, S3 |
| 2026-10-29 | Q-S3-21, verbatim: "(a) Kit words only (Recommended)". `REFUSE_PARTIAL` site 2 covers only the checked words of the `BUILTINS` table (an `Option` result, internal, not an `isSet` rule). A guest program cannot name a kit word (`REFUSE_KIT` comes first). A domain definition with an `Option` result where its payload is wanted (`opSucc n` where a `Count` is wanted) stays `TYPE_MISMATCH`. | USER, S3 |
| 2026-10-29 | Q-S4-1, verbatim: "(a) 4 words, all (Recommended)". `REFUSE_CLOCK` refuses the clock words `now`, `block`, `timestamp` and `clock` as a declaration, a binder or a use, in kit, domain and guest programs. The hint names `Stamp`. `time` and `epoch` stay free names. | USER, S4 |
| 2026-10-29 | Q-S4-2, verbatim: "(a) Time := Nat (Recommended)". The domain definition `Time := Nat`. The stamp signature message uses `toU256 t`. Two times compare with `natLe`. | USER, S4 |
| 2026-10-29 | Q-S4-3, verbatim: "(a) Domain check + refl laws (Recommended)". The domain word `stampSnoc : Point -> Stamp -> Stamps -> Option Stamps` uses `schnorrOk`, `natLe` and `u256Eq`. It gives `none` on a bad signature, an old t or a wrong prev. L12 is refl paths on closed values (`examples/stamps.lang`), with mutants. | USER, S4 |
| 2026-10-29 | Q-S4-4, verbatim: "(a) As section 10 places them (Recommended)". S4 = the stamp chain CM, `Stamp`, `stampSnoc`, `lastT`, the stamp head, `REFUSE_CLOCK`, L12. The CC dual and close at T go to S5. Causality goes to S5 L6 or to S6. | USER, S4 |
| 2026-10-29 | Q-S4-5, verbatim: "(a) Remove it (Recommended)". The `var_core` clock test goes. `resolve_head` gives `var_core` only a local or a definition, so the test cannot run (mutant M3 survived: an equivalent mutant). `unknown_name` refuses each clock use (mutant M11 killed). | USER, S4 |
| 2026-10-29 | Not asked, a recommendation (not a ruling): the stamp key is a `Point` argument of `stampSnoc`. Polity does not change. S6 puts `keyStamp` and `tStamp` in the `Constitution`. | S4 default |
| 2026-10-29 | Not asked, a recommendation (not a ruling): an empty chain has no last t. The first stamp needs a good signature, and prev must be the stamp genesis digest. | S4 default |
| 2026-10-29 | Q-S5-1, verbatim: "(a) Split S5a / S5b (Recommended)". S5a: the Part E types, G, earned and the policy history as data; L1, L2, L6. S5b: the books, fund, settle, cancel, dispute, close, Instruction, the CC dual and close at T; L3, L4, L5. | USER, S5 |
| 2026-10-29 | Q-S5-2, verbatim: "(a) Instruction data now (Recommended)". S5b adds the family `Instruction` (payee : Point, asset : Digest, amount : U256, at : Stamp) as data only. S7 adds `PayAuth` and the record. | USER, S5 |
| 2026-10-29 | Q-S5-3, verbatim: "(a) Refl on closed values, O8 open (Recommended)". L4 is refl on closed `Flag` results. The residual stays an `Option`, and O8 stays open. | USER, S5 |
| 2026-10-29 | Q-S5-4, verbatim: "(a) Domain cofamily Books (Recommended)". `cofamily Books (A) := booksCorec (booksNow : A) (booksNext : Stamp -> Books A)`. eps is `booksNow` and advance is `booksNext`. The pairing walks `Stamps`, with a pairing law as refl. | USER, S5 |
| 2026-10-29 | Q-S5-5, verbatim: "(a) S5a, history as data (Recommended)". `History := FreeMon (Prod Time Policy)` as data, and L6 is refl on closed values in S5a. Gov, `choice`, the append by ballot and the regime checks go to S6. | USER, S5 |
| 2026-10-29 | Q-S5-6, verbatim: "(a) The previous close (Recommended)". T^- is the stamp of the previous close (retainer-lang T_prev). The first close uses the start. | USER, S5 |
| 2026-10-29 | Q-S5-7, verbatim: "(a) New Part E names (Recommended)". `Auth` becomes `Grant` and `open` becomes `engage`. The `Instrument` constructors are `instrPush`, `instrPull` and `instrStream`. The S2 to S4 names stay. | USER, S5 |
| 2026-10-29 | Q-S5-8, verbatim: "(a) O1 equal books, O3 split unearned (Recommended)". O1: a pull settle adds w to funded and to withdrawn. O3: a dispute splits unearned. The firm gets floor(u * bps / 10000) by `mulDiv` and the client gets the rest. A verdict outside the list gives `none`. | USER, S5 |
| 2026-10-30 | Q-S5-9, verbatim: "(b) Prod Verdict U256 fold". Each sum of G and earned folds into `Acc := Prod Verdict U256` (an ok flag and a value). `accIsSet` is `isSetProd Flag U256 flagIsSet u256IsSet`, so no trusted row is necessary. `accOut` makes the `Option` at the end. | USER, S5 |
| 2026-10-30 | Q-S5-10, verbatim: "(a) Add a killer row (Recommended)". The mutant M22 (no engagement end in hi) survived: in each row with t after the end of the engagement, the rate was flat. `eEndAt50` in examples/earned.lang (the engagement ends at 30, the rate of hA accrues to 60) kills it. | USER, S5 |
| 2026-10-30 | Not asked, a recommendation (not a ruling): the earned word is `earnedAt h e t`, because `earned` is a `State` field in 13 test/check files and 2 examples (R4). `earnedBetween h e t1 t2` is earned on (t1, t2] (L1). | S5a default |
| 2026-10-30 | Q-S5b-1, verbatim: "(a) Funding log (Recommended)". `Book := bookOf (bookLog : Funding) (bookWithdrawn : U256) (bookClosed : Option Time)`, with `Funding := FreeMon (Prod Time U256)`. funded at T is the last log entry at T or before (0 if none). fund, cancel, dispute and a pull settle add (t, funded after) to the log. So close T is exact for each T <= now. | USER, S5b |
| 2026-10-30 | Q-S5b-2, verbatim: "(a) Guard on the book after (Recommended)". settle makes the book after, then requires withdrawn after <= recognized after. For push and stream this is w <= withdrawable. For pull it is w <= earned - withdrawn. The L3 rows use a fully funded book. One row shows the pull book when earned > funded. | USER, S5b |
| 2026-10-30 | Q-S5b-3, verbatim: "(a) Escrow + list (Recommended)". `Escrow := escrowOf (escEng : Engagement) (escBook : Book)`. Each op takes the History, a Stamp and an Escrow, and gives `Option (Prod Escrow Payout)`, with `Payout := FreeMon Instruction`. fund gives no Instruction, settle and cancel give one, dispute gives two. A failed guard gives none. | USER, S5b |
| 2026-10-30 | Q-S5b-4, verbatim: "(a) First close: r T - 0 (Recommended)". The first close gives recognized T - 0. T^- is the start only in the bound S < T. So the closes sum to recognized at the last close (the law lCloseSum). | USER, S5b |
| 2026-10-30 | Q-S5b-5, verbatim: "(a) Strict later guard (Recommended)". `logOk` reads only the funding log, so a stamp at or before the last close could change funded at a closed time. Now each of fund, settle, cancel and dispute needs `bookOk l c t := flagIf (logOk l t) (later t c) flagNo`: the log is in time order up to t, and t is after the last close. The law lCloseCancel and the cb rows of examples/books.lang show it. | USER, S5b |
| 2026-10-30 | Not asked, a build default (not a ruling): the words are `engIsOpen` and `engIsSigned`, because `isOpen` is a `State` field in examples/storage.lang (R4). | S5b default |
| 2026-10-30 | Not asked, a build default (not a ruling): the survey form `isSetFun Books.pos A sA` does not lex (`LEX_CHAR` 0x2e). `booksPair A sA b c` folds the stamps of c (`fmChain`, the first stamp first) into a map on the readers `Books A -> A`, set by `isSetFun (Books A -> A) (Books A -> A) (isSetFun (Books A) A sA)`, and applies the map to `booksNow A` and b. booksLaw: the pairing of a corecursor with a chain is the fold of its step (refl). booksLawNine: the value is some 9. | S5b default |
| 2026-10-30 | Not asked, a build default (not a ruling): the test signer `build/frosttool engage` prints the words of the engage and countersign rows (the client and the firm committees, the good and bad signatures). It is test data only, as the `stamps` mode (S4). | S5b default |
| 2026-10-30 | Review fix: every settle logs `(t, funded after)`, including push and stream when funded is unchanged, so `logOk` also prevents operations before a withdrawal. Countersign checks `bookOk` before setting the signed time, so a signature cannot change a closed period or precede the funding log. The paid and cs regression rows in examples/books.lang cover both cases and valid controls. | S5b review |

The seed is `bin/new-lang.sh accrue-lang tcc-wasm` from lang-template.
Native code and EVM are not targets. EVM goes because the request makes
each contract an off-chain protocol.

## 2. The reading of R0: no trap

R0: no program can trap or surface an exception, on any target.

The reading: each operation is total on its declared type, and each run of
each target gives a value of the result type. A failure that the domain
knows (a bad signature, a residual below zero, a full list) is a value:
`Option A` with `none`. There is no other failure path.

| Rule | Statement | Where |
|---|---|---|
| N1 | Each partial operation has an `Option` result or a refinement argument. `natAdd`, `natSub`, `u256Add`, `u256Sub` and `u256Mul` return an `Option` (Q-S3-2, Q-S3-3, Q-S3-6). The residual `a - b` with a proof `b <= a` waits for a dependent eliminator (O8). Division takes a `NonZero` divisor (Q-S3-4, Q-S3-8). `mulDiv a b d` = `floor(a * b / d)` with a 512-bit product and `d : NonZero U256`, `none` only when the result does not fit (Q-S3-5). The crypto words return an `Option` on a bad input (Q-S3-7). | kernel types, eval, each writer |
| N2 | No recursion at run time, no run-time closure, no run-time tree fold (the kit HOST-LIMITs). So no target can exhaust its stack. | kit, `REFUSE_REC` |
| N3 | Each run-time list has a capacity that the program fixes (Q-S0-7). An operation that would pass the capacity gives `none`. Memory is static: no `memory.grow`, so each party runs in the same memory and gets the same result. | lowering, Wasm writer |
| N4 | The writers emit no trap form. Wasm: no `unreachable`, each `div` and `rem` has a divisor that the type proves is not zero, each load and store is inside the static memory. ES: no `throw`, no BigInt division by zero. WGSL: no division by zero (WGSL defines it, but its value is not the eval value). | writers, `test/notrap/` |
| N5 | `langc eval` never aborts on a checked program. The kit eval stops at a `Nat` above 2^64 - 1 (cairn-lang S8 shows this trap). accrue-lang gives such a value an `Option` form or refuses the program at check time. | eval |

New refusal code `REFUSE_PARTIAL`: a program uses a partial kit word (for
example `div`, `-` on `U256`, `head` of a list) without its `Option` form
or its proof argument. The fix hint names the total form.

Gate for R0: a C tool `test/notraptool.c` reads each built module and
fails on each trap opcode and each unguarded `div` or `rem` (Wasm), and on
each `throw` (ES). Each target runs the edge rows (zero divisor, 2^256 - 1,
a full list, an empty list) and must give the eval value.

## 3. The reading of R1

R1: the type constructors are the cell monads and their duals, as realized
in Cubical TT.

cairn-lang (MavenRain/cairn-lang) has the same R1, and it took the reading
and the kernel of intent-lang. accrue-lang takes them without change:

- The ambient structure is not limited: Pi, Sigma, `Path`, `PathP`, the
  interval `I`, `transp`, `hcomp`, the universe `U`, `Glue` and `ua`.
- A declared type constructor must be one of two classes:
  - CM, cell monad: the free algebra monad of a cell signature. The
    parameter occurs only in the generator `eta : A -> T A`. Each other
    cell takes closed base data and arguments of type `T A`. Unit is `eta`,
    bind is the recursor, and the laws are paths by induction.
  - CC, cocell comonad: the cofree coalgebra comonad of a cocell
    signature. One observation `eps : D A -> A`, each other field `D A ->
    D A` or `D A -> E -> D A`. Counit is `eps`, extend is the corecursor.
- `REFUSE_CELL` refuses each other parameterized declaration. The cons
  list is refused. The free monoid `FreeMon` is the admitted list.

accrue-lang uses the duality in two places:

- The accrual: the behaviour `Time -> Amount` of an engagement is a CC
  (section 5). `eps` is the earned amount at the current stamp, and
  `advance : D A -> Stamp -> D A` moves to a later stamp.
- The record of settlements is a CM (`FreeMon Instruction`), and the
  books are its fold.

## 4. The minimal base signature (R2)

R2: admit only the base types and operations that retainer-lang programs
need, with each contract an off-chain protocol and the DAO an off-chain
ZK + FHE + MPC threshold DAO that can amend itself.

Each name has one of three kinds, as in cairn-lang:

| Kind | Meaning |
|---|---|
| slot | The program gives the type from formers and kit base data. |
| defined | The prelude defines it from slots, formers and kit base data. |
| primitive | Opaque. The kit implements it. Only the listed operations use it. |

Kit base data (all of it, O1): `Scalar` (the field of the group order),
`Point` (the group), `Digest` (32 bytes), `U256` (amounts). `Nat` stays
internal to the kernel. There is no `Addr`, `Env`, `msg.sender`, ERC-20
`token`, `now` or `block`: those are chain data.

### 4.1 Part E: the engagement (from retainer-lang SPEC 5 and 6)

| Name | Kind | Reading |
|---|---|---|
| Time | defined | `Nat`, seconds. The denotation is continuous time; a `Stamp` is a sampling (section 5). |
| Amount | defined | `U256` with the residual `a - b` for `b <= a` (N1). Q-S0-3. |
| Rate | defined | `segments : FreeMon Segment` and `masses : FreeMon Mass`, as retainer-lang C6. `Segment = (from, until : Time, num : U256, den : NonZero)`. |
| Window | defined | `(start, end : Time)` with `start <= end`. |
| Grant | defined | `grantOf (grantInstrument : Instrument) (grantPaused : Flag)`, with `Instrument := instrPush | instrPull | instrStream` (Q-S5-7: the old name was Auth). |
| Dispute | defined | `FreeMon Bps`: the firm share of the residual for each verdict, `Bps <= 10000`. A verdict outside the list gives `none` (in retainer-lang it reverts). |
| Policy | defined | `policyOf (Rate, Window, Grant, Dispute)`. |
| History | defined | `FreeMon (Prod Time Policy)`: the policy history as data, each entry a time and a policy (Q-S5-5). |
| Engagement | defined | `(client, firm : Point, asset : Digest, scopeHash : Digest, window : Window)`, plus `countersignedAt, cancelledAt : Option Time`. `asset` names the off-chain asset (Q-S0-4). |
| engage, countersign | defined | A Schnorr signature of the client, then of the firm, on the engagement digest. S5b: `engDigest` with the tags `tagEngage` and `tagCountersign`. `engIsOpen e`: no cancelled time. `engIsSigned e`: a signed time. countersign needs a not signed e. |
| earnedAt, recognized, unearned, withdrawable | defined | retainer-lang SPEC 6.1, without change: `G` from the segment start, the closed first interval, the open later intervals. S5a: `gAt`, `earnedAt h e t` and `earnedBetween h e t1 t2` (on (t1, t2]). |
| fund, settle, cancel, dispute, close | defined | As retainer-lang, but each one that moves value gives an `Instruction` (4.3) and does not move it. S5b: `escFund`, `escSettle`, `escCancel`, `escDispute` and `escClose` on an `Escrow` (Q-S5b-1, Q-S5b-3). Each op that moves value needs `bookOk` (Q-S5b-5); fund and cancel need an open engagement; dispute needs a cancel, or the end or later (O3). settle guards on the book after (Q-S5b-2). close T gives recognized T - recognized T^- (Q-S5-6, Q-S5b-4). |
| Books | defined | `cofamily Books (A) := booksCorec (booksNow : A) (booksNext : Stamp -> Books A)`, the CC dual (Q-S5-4). `booksPair` pairs a `Books A` with a stamp chain (section 1, the S5b default). |

### 4.2 Part D: the DAO (cairn-lang Part D plus retainer-lang Gov)

| Name | Kind | Reading |
|---|---|---|
| Member | defined | `Point`: one public key. |
| D | defined | `Nat` below `decisions` (retainer-lang C10). `policyOf : D -> Policy` is a total program definition. |
| Constitution | defined record | `members : FreeMon Member`; the thresholds `tOpen`, `tAmend`, `tStamp`, `tPay`; the keys `keyOpen`, `keyStamp`, `keyPay : Point`; `rule : Nat`, the position of the choice rule in force in the program list `rules` (Q-S0-6). |
| PolicyBallot | defined | One-hot over `D`: `decisions` exponential ElGamal ciphertexts under `keyOpen`, a disjunctive Chaum-Pedersen proof that each one is 0 or 1, a proof that their sum is 1, and a bLSAG ring signature over `members`. |
| Tally | defined | `FreeMon Nat` of length `decisions`, the opened homomorphic sum. This is the orbit quotient of retainer-lang: the counts, never who voted what. |
| choice | slot | `Tally -> Option D`, a program definition. The compiler makes its table over each tally (retainer-lang C10). |
| Gov, GovPi | defined | `Gov` = the table of the rule in force; `GovPi = map policyOf . Gov`. |
| AmendBallot | defined | cairn-lang `Ballot`: `Enc Bool` with its proof and its bLSAG, on a whole new `Constitution`. `decide` = `yes >= tAmend`. |
| Share, open | primitive | Partial decryption with a DLEQ proof; `tOpen` valid shares combined by Lagrange (MPC). |
| Deal, Reshare | primitive | cairn-lang `src/front/dkg.c`: Pedersen DKG with Feldman commitments, and reshare when `members` or a threshold changes. No trusted dealer. |

Self-reference: an amendment is a whole `Constitution`, so the DAO can
amend `tAmend` and `rule` (the rule that turns a tally into a policy).
Each ballot is decided by the constitution in force when it opens. A chain
of amendments is a fold over `FreeMon Constitution`. The policy history
(retainer-lang C12) gets one entry each time `GovPi` gives a new policy.
retainer-lang keeps `REFUSE_BIFURCATION` (Schelling-Ising) and the
Arrow-impossibility freeze (no decision: the last policy stays in force,
or the zero rate if there was none).

### 4.3 Part S: stamps and settlement

| Name | Kind | Reading |
|---|---|---|
| Stamp | defined | `(t : Time, prev : Digest, sig)`: a FROST threshold Schnorr signature under `keyStamp` on `(t, prev)`. `t` must be above the `t` of `prev` (law L12). Q-S0-2. |
| Instruction | defined | `(payee : Point, asset : Digest, amount : U256, at : Stamp)`: the output of `settle`, `cancel` and `dispute`. |
| PayAuth | primitive | A FROST threshold Schnorr signature under `keyPay` on the instruction digest (cairn-lang `src/front/frost.c`). |
| Record | defined | `FreeMon Instruction` as a SHA-256 hash chain (cairn-lang Part R: `snoc`, `head`, `verify`, `Inclusion`). An instruction is valid once, at its position. |

The protocol never holds money. A rail outside the language (a bank, a
chain, a payment processor) pays each instruction that has a valid
`PayAuth`. The record makes a second payment of the same residual visible:
`settle` reads `withdrawn` from the record fold, so a replay gives zero.

### 4.4 Enc, ZK and MPC

The readings of cairn-lang PLAN 3.3 hold: `Enc` is the identity cell monad
behind a sealed eliminator, `bind` is homomorphic evaluation (affine maps
only, for exponential ElGamal), `decrypt` is a law-only monad morphism
`Enc -> Id`, `open q c = decrypt c` is the MPC law, and `ZKProof W := || W
||` (the `squash` cell) is zero knowledge. IND-CPA and proof soundness are
VALIDATION assumptions.

## 5. Time

retainer-lang: "Continuous time is the denotation; block time is a
sampling." Off chain there is no block. The reading (Q-S0-2 (a)): the
sampling is the stamp chain. The `keyStamp` committee signs `(t, prev)`
when `tStamp` members agree that `t` has passed. Each program reads time
only as a `Stamp` argument. No program can read a local clock.

- The stamp chain is a CM (`FreeMon Stamp`). The accrual is its dual: the
  cofree comonad of `Stamp -> -` (a CC), with `eps` = the books now and
  `advance` = the books at the next stamp. The pairing of the two runs the
  chain on an engagement.
- New refusal code `REFUSE_CLOCK`: a program names a chain or host clock
  word (`now`, `block`, `timestamp`, `clock`). The fix hint names `Stamp`.
- `close` at `T` uses the stamp for `T` and the stamp before it as `T^-`.
- Causality (retainer-lang `## Self-reference`): an amendment at stamp
  `t*` adds a policy history entry in force only after `t*`.

## 6. Laws

Each law is a path that the kernel checks. "VALIDATION" marks a
computational assumption that no path can prove (docs/VALIDATION.md).

| Law | Statement |
|---|---|
| L1 additivity | `earned t2 = earned t1 + earned (t1, t2)` for `t1 <= t2` |
| L2 zero and order | `earned t = 0` for `t < start`; `t1 <= t2 -> earned t1 <= earned t2` |
| L3 instruments | push, pull and stream give the same `(earned, recognized, withdrawable)` |
| L4 books | `recognized = min funded earned`; `withdrawn <= recognized <= funded` after each operation |
| L5 residual only | `dispute v` changes only the residual; `recognized` before the dispute stays |
| L6 causality | an amendment at `t*` leaves `earned t` for each `t <= t*` |
| L7 tally | `open q (tallyEnc bs) = count (map decrypt bs)`, for each slot of `D` |
| L8 MPC | `open q c = decrypt c` |
| L9 orbit | the tally of a relabeled ballot profile is the same tally |
| L10 self-constituting | `GovPi = policyOf . choice` at a constant constitution |
| L11 amend | `amend (ps ++ qs) = amend qs . amend ps` |
| L12 stamps | `t (snoc s c) > t (last c)` for each stamp that `snoc` accepts |
| L13 settle once | `settle` twice at the same stamp gives one instruction of the residual and one of zero |
| V1 to V5 | VALIDATION: collision resistance of SHA-256; IND-CPA of ElGamal (DDH); soundness and zero knowledge of the Sigma proofs, DLEQ and bLSAG in the ROM; unforgeability of FROST; an honest `tStamp` quorum signs only past times |

## 7. Route

### 7.1 Kernel

S2 ports the cairn-lang kernel at its newest USER commit (S2 step 0
records the hash), the way cairn-lang ported intent-lang (`git archive
... | tar -x`, then `make -B check`, because `git archive` sets old
mtimes). Keep: the Cubical TT checker and evaluator, `REFUSE_CELL`,
`FreeMon`, `Enc`, `group.h` on secp256k1, SHA-256, the Sigma, ElGamal,
DLEQ, threshold, bLSAG, FROST and DKG code, Part D and Part R. Remove:
`REFUSE_TIME` and `OpPos` (accrue-lang has time, through stamps). Do not
take `.kanon-exec/` or `.kanon-wait/`. S2 took cairn-lang 0743108 (the
cairn-lang S7 USER commit).

S5 ports the retainer-lang domain (`domain/domain.lang`, the `earned`
definition, the policy history, the regime checks) as guest code. The
chain parts go: `Addr` becomes `Point`, `scopeHash` becomes `Digest`, the
ERC-20 `token` becomes `asset : Digest`, `pay` and `pull` become
`Instruction` values.

### 7.2 The three targets

One front end (the ported kernel) and three writers that read its output:

| Target | Kit source | Crypto at run time | Use |
|---|---|---|---|
| Wasm MVP (primary) | cairn-lang `src/wasm.c` and `src/front/lower.c` at its S8 close, else tcc-wasm | the cairn-lang Wasm prelude (i64 limbs, point add and multiply, SHA-256) | every party: client, firm, members, stamp and pay committees |
| ES module + JSON | tcc-js `src/js.c`, `src/json.c` | BigInt in a JS prelude | client and firm views, public checks of the record. JS is not constant time, so a program that touches a key share is refused here (`REFUSE_TARGET_SECRET`). |
| WGSL compute | tcc-webgpu `src/wgsl.c` | u32 limbs | batch work: check many ballot proofs, `earned` over many engagements |

Differential tests: each target runs the same vectors as `langc eval`.
The kit harnesses stay. No new node, Python or shell harness: new tests
are guest `.lang` files and C tools.

## 8. Agent adoption (A1 to A6)

The request asks for six items so that agents reach for accrue-lang when
it fits.

| Item | Deliverable | Gate |
|---|---|---|
| A1 | One canonical form for each operation: no aliases, no second syntax. `langc fmt FILE` prints the canonical form. `langc fmt --check` exits 1 when a file is not canonical. | `fmt` is idempotent; `parse (fmt x)` has the same core term as `x`; every example and test file is canonical. |
| A2 | `langc check --json FILE`: one JSON object with `diagnostics` (code, span, message, `fix`: a concrete edit) and `progress` (`parsed`, `typed`, `laws_discharged`, `laws_total`). Schema in `docs/CHECK-JSON.md`. | Rows in `test/json/` with the expected JSON, byte equal. Each refusal code has a `fix` row. |
| A3 | `SKILL.md` with five graded examples (G1 to G5). Each example is a file in `examples/skill/`, and SKILL.md quotes it. | The gate checks each example and checks that the SKILL.md block and the file are byte equal. |
| A4 | `langc mcp`: an MCP server over stdio (JSON-RPC 2.0, one message on each line), in C, built by TinyCC into the same `langc`. Tools: `check`, `fmt`, `eval`, `build` (with a target), `explain` (a refusal code). The MCP spec version is pinned in `docs/MCP.md`. | A C tool `test/mcptool.c` sends `test/mcp/*.in` and compares the replies with `*.out`. |
| A5 | `AGENTS.md` (how an agent works in this repo, with the gate command) and a README table: when to use accrue-lang, when not to use it, and what to use then. | Review only. |
| A6 | Token counts: G1 to G5 in accrue-lang, Python and Rust. Report `docs/TOKEN-BASELINE.md` with the tokenizer pin, each count and each file hash (Q-S0-8). | `make tokens`, not in `make check`. |

Graded examples:

| Grade | Example | Uses |
|---|---|---|
| G1 | A fixed monthly retainer: open, countersign, fund, `earned` at a stamp, settle | Part E, one stamp key, threshold 1 |
| G2 | A closed linear stream, cancelled at `t*` | `earned(end) = d`, `cancel`, `unearned` back to the client, L1 |
| G3 | One encrypted policy vote over two policies | PolicyBallot, tally, `choice`, `GovPi`, L7, L9 |
| G4 | The rate changes mid-engagement | the policy history, L6 (the past stays) |
| G5 | The DAO amends `tAmend` and `rule` | self-reference, `amend` as a fold, L11, the freeze regime |

## 9. Tests

The kit layout stays: `test/parse`, `test/check`, `test/eval`, `test/ir`,
`test/fronttool.c`, `test/gate.sh`. The port adds the cairn-lang tools
(`test/shatool.c`, `test/secptool.c`, the ElGamal, threshold, Sigma, bLSAG
and FROST tools) and the retainer-lang domain rows. New: `test/notrap/`
and `test/notraptool.c` (R0), `test/json/` (A2), `test/mcp/` and
`test/mcptool.c` (A4), `test/fmt/` (A1). Mutation: each slice that adds a
refusal or a law kills at least one mutant of it.

## 10. Slices

| Slice | Work | Status |
|---|---|---|
| S0 | Seed from tcc-wasm, DESIGN.md verbatim, this plan; gate; seed commit; private repo | DONE |
| S1 | The USER rules section 12. Record the rulings here. | DONE |
| S2 | Port the cairn-lang kernel (7.1). Survey first: SAME, CHANGED, CAIRN-ONLY. | DONE |
| S3 | R0: N1, N5 in the kernel and eval; `REFUSE_PARTIAL`; checked `U256`, `mulDiv`. | DONE |
| S4 | Time: `Stamp`, the stamp chain, `REFUSE_CLOCK`; L12. | DONE |
| S5a | Part E (7.1): the types, G, `earnedAt`, the policy history as data; L1, L2, L6 (Q-S5-1). | DONE |
| S5b | Part E: the books, fund, settle, cancel, dispute, close at T (Q-S5-6), Instruction data (Q-S5-2), the CC dual (Q-S5-4); L3 to L5. | DONE |
| S6 | Part D: PolicyBallot, Tally, `choice` table, `GovPi`, regimes, AmendBallot, reshare; L7 to L11. | NEXT |
| S7 | Part S: Instruction, PayAuth, the record of instructions; L13. | |
| S8 | Wasm: lowering, prelude, N2 to N4, `test/notraptool.c`, differential vectors. | |
| S9 | ES module + JSON: `js.c`, `json.c`, BigInt prelude, `REFUSE_TARGET_SECRET`. | |
| S10 | WGSL: `wgsl.c`, batch proof check and batch `earned`. | |
| S11 | A1: canonical forms and `langc fmt`. | |
| S12 | A2: `langc check --json`. | |
| S13 | A4: `langc mcp`. | |
| S14 | A3 and A5: SKILL.md, G1 to G5, AGENTS.md, the README table. | |
| S15 | A6: the token comparison. | |
| S16 | Close: SPEC.md, probe/CAPABILITY.md, VALIDATION.md, final gate. | |

## 11. Open items

- O1 The kit base data is `Scalar`, `Point`, `Digest`, `U256`. If a slice
  needs more, it records why here first.
- O2 cairn-lang is in its S8 (Wasm). S2 ported its newest USER commit:
  BASE 0743108. S8 takes its Wasm writer only after the cairn-lang USER
  commit of S8.
- O3 retainer-lang SPEC.md 82 still says EVM output PLANNED (its O-d16-1).
  accrue-lang takes only the domain, so this does not apply here.
- O4 The secret marks for `REFUSE_TARGET_SECRET` (S9): the key shares,
  the nonces and the decryption key. S6 marks them in the types.
- O5 The test vectors come from the published standards (NIST CAVP for
  SHA-256, SEC 2 for secp256k1, the FROST RFC 9591 vectors), not from an
  implementation that the gate would trust.
- O6 The pinned MCP spec version and the JSON-RPC subset (A4) go in
  `docs/MCP.md` at S13.
- O7 The `fix` of each refusal code (A2) is one concrete edit. A code
  with no edit gives `fix: null` and a reason.
- O8 The residual `a - b` with a proof `b <= a` (Q-S3-3). `flagIf` is
  not dependent, so only refl on closed values can make the proof now. A
  dependent `Flag` eliminator comes first. S5b takes L4 as refl on
  closed values (Q-S5-3), so O8 stays open for a later slice.
- O9 Rule N4 for the S8 writer (Q-S3-18): no trap flag, no
  `IR_STMT_TRAP` and no Wasm `unreachable`. How an entry with an
  `Option` result gives `none` to the host (the entry ABI) is an open S8
  question. `src/front/lower.c`, `src/ir.h` and `src/wasm.c` are out of
  the build until S8. `lower.c` does not compile: it names `OP_NAT_MUL`,
  which S2 removed. `test/ir` stays for S8 (Q-S3-19), and the gate does
  not run it.
- O10 The S4 mutant M3 (the `var_core` clock test) survived as an
  equivalent mutant, so S4 removed the test (Q-S4-5). The
  `NAME_UNKNOWN` return at the end of `var_core` cannot run for the same
  reason (before S4 too): `resolve_head` gives `var_core` only a local or
  a definition. It stays as a guard. A later slice can remove it.

## 12. Questions for the USER (S0)

Each question has a recommendation (rec). S1 records the rulings: each
question takes (a) (section 1).

- Q-S0-1 Crypto. (a) rec: keep the cairn-lang rulings: secp256k1,
  SHA-256, exponential ElGamal, FROST, Pedersen DKG with Feldman
  commitments. They come with the S2 port. (b) Pick again for
  accrue-lang.
- Q-S0-2 Time source. (a) rec: a stamp chain signed by a `tStamp`
  threshold of the DAO (section 5). (b) The client and the firm co-sign
  each stamp: no DAO in the time path, but one party can stop accrual by
  not signing. (c) Operation counts only, as cairn-lang: no per-second
  rate, so a stream is not possible.
- Q-S0-3 Amount privacy. (a) rec: amounts are plain `U256`; the client,
  the firm and the committees see the books. Only the ballots are
  encrypted. (b) Amounts encrypted under `keyOpen` with range proofs:
  private books, but each residual needs a ZK range proof and much more
  code.
- Q-S0-4 Settlement. (a) rec: the protocol holds no money; each value
  move is an `Instruction` with a threshold `PayAuth`, appended to the
  record, and a rail outside the language pays it. (b) The protocol keeps
  an internal balance for each party and transfers between them; a rail
  is needed only to enter and leave.
- Q-S0-5 Ballots for policy. (a) rec: one-hot over `D` (4.2), so the
  opened tally is the orbit quotient that retainer-lang `Gov` reads. (b)
  Yes or no on one policy at a time, as cairn-lang: no `choice` over a
  tally of `D`.
- Q-S0-6 What the DAO can amend. (a) rec: the whole `Constitution`,
  `rule` too, so the DAO can change the choice rule from the program list
  `rules`. (b) The thresholds and the members only; `choice` is fixed at
  build time.
- Q-S0-7 Run-time capacity (N3). (a) rec: each run-time list has a
  capacity in the program (for example `maxSegments := 16`), and static
  memory. Each party gets the same result. (b) `memory.grow` with a
  `none` path: no capacity to pick, but two parties with different memory
  limits can get different results.
- Q-S0-8 Token comparison (A6). (a) rec: take the cairn-lang rulings: the
  Anthropic `count_tokens` endpoint for the newest Claude model (model id
  and date in the report), and the Python and Rust programs run with
  outputs equal to the accrue-lang output. (b) A pinned offline
  tokenizer, text only.
