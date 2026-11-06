# accrue-lang specification (draft)

Status: M0 to M2 DONE, M3 PARTIAL (section 10). `accrue-lang` is a working
name.

`docs/PLAN.md` holds the rulings and the slices. `docs/VALIDATION.md`
holds the evidence.

## 1. Purpose

accrue-lang is a language for one retainer DAO. A client funds an
engagement. The firm earns a rate over time against the engagement: the
client, the firm, the `scopeHash` of the engagement letter, a window and a
policy (`design/DESIGN.md`, Semantic domain). The DAO decides a policy,
not a balance: the rate, the window, the authorization and the dispute
rule (Stance). The books give the funded, recognized, unearned and
withdrawable amounts at a time (Books). The admitted operations are fund,
settle, cancel, dispute and amend (Admitted operations).

A program declares the values of the engagements, the books and the DAO,
and the laws that relate them. A law is a term of a `Path` type. The
compiler `langc` checks the program, its types and its laws, and then
writes the target.

The host is TinyCC (`PIN`). The targets are Wasm MVP (the tcc-wasm kit,
the primary target), an ES module with JSON (the tcc-js kit) and WGSL
compute (the tcc-webgpu kit). Each build writes one target
(`probe/CAPABILITY.md` section 1). Native code is not a target.

No target has a trap form (R0, `docs/PLAN.md` section 2). A partial kit
word gives an `Option` value or takes a `NonZero` divisor, and an overflow
or a divide by 0 gives `none` (`probe/CAPABILITY.md` section 4). A bare
use of a partial word gives `REFUSE_PARTIAL`. A program has no clock: a
time comes in as a `Stamp` argument, and a clock word gives `REFUSE_CLOCK`
(`docs/PLAN.md` section 5).

Its type formers are F1 to F15 of `formers/FORMERS.md`. Its core data types
and core operations are only the types and operations of
`design/DESIGN.md` (the design).

The kernel comes from cairn-lang: S2 ports the cairn-lang kernel at
0743108 (`docs/PLAN.md` section 7). The hash is SHA-256 and the group is
secp256k1 (O5). The guest `Enc`, `ZKProof` and ballot types are an ideal
model, as in cairn-lang (sections 5 and 6).

The tools for agents are in `docs/PLAN.md` section 8 (A1 to A6):

- `langc fmt` prints the one canonical form of a program (S11).
- `langc check --json` gives the diagnostics and the progress counts
  (S12).
- `langc mcp` serves agents (S13). `docs/MCP.md` gives the pinned MCP
  revision `2025-11-25` and the JSON-RPC subset (O6).
- The `skills` directory holds `SKILL.md` and the graded examples G1 to
  G5. `AGENTS.md` and the README table say when to use the language
  (S14).
- S15 records the token baseline for G1 to G5 (A6).

## 2. Programs

A program is one file, with no `import` (`probe/CAPABILITY.md` section 2).
It is a sequence of definitions:

```
def NAME : TYPE := TERM
```

A definition can use the core names, the prelude names and the
definitions before it (`probe/CAPABILITY.md` section 3). Thus a program
has no general recursion. Recursion comes only from `fold` and `unfold`
(F6, F7). The domain needs no fixed first definition: a size is a term of
the domain family `Size` (`domain/domain.lang:57`).

The compiler refuses these host forms in a program (`formers/tcc-wasm.md`).
`test/check/expect.txt` gives the code of each row in `test/check/` (203
rows). The number after each code is its count of rows.

- A `family` declaration gives `REFUSE_DATA` (3).
- An `axiom` gives `REFUSE_AXIOM` (1).
- A use of the definition itself or of a later one gives `REFUSE_REC` (3).
- A core name, a domain name, a reserved name or a name that is already
  defined gives `REFUSE_NAME` (7).

The domain adds these refusals:

- A bare use of a partial kit word gives `REFUSE_PARTIAL` (6). The check
  has two sites (Q-S3-1 (a), `docs/PLAN.md:189`). Site 2 covers the
  checked words of the `BUILTINS` table (Q-S3-21 (a), :209).
- A clock word (`now`, `block`, `timestamp` or `clock`) gives
  `REFUSE_CLOCK` (4; Q-S4-1 (a), :210).
- A kit type, a kit operation or a kit literal outside the kit gives
  `REFUSE_KIT` (22).
- The kit cons list `List` in a guest program gives `REFUSE_CELL` (1). A
  guest program uses `FreeMon`.
- A state that is not the one state of the design gives `REFUSE_STATE`
  (16).
- A history write that is not the append gives `REFUSE_HISTORY_WRITE`
  (11).
- A bad event field gives `REFUSE_EVENT` (3).
- A map key or a map value that is not admitted gives `REFUSE_MAP` (2).
- A term that leaks a secret gives `REFUSE_LAW` (11).
- A signature that the design does not give gives `REFUSE_SIG` (12).

The lowering adds `REFUSE_RUNTIME_TREE` and `REFUSE_RUNTIME_CLOSURE` (the
kit HOST-LIMITs, `docs/PLAN.md:357`), `REFUSE_LOWER`,
`REFUSE_TARGET_SECRET` (O4) and `REFUSE_CAPACITY` (the static memory of a
Wasm module, :271). The compiler writes the target; a program cannot.

## 3. Type formers

The type formers are F1 to F15. `formers/tcc-wasm.md` gives the status of
each former on the primary target, Wasm MVP (the rows
`formers/tcc-wasm.md:14` to :28), with the `src/front/check.c` and
`src/front/lower.c` lines. `formers/tcc-js.md` and `formers/tcc-webgpu.md`
give the other 2 targets. Rule R1 admits only cell monads (CM) and their
duals (CC) as type constructors (`docs/PLAN.md` section 3). Each other
parameterized declaration gets `REFUSE_CELL` (`docs/PLAN.md:389`). F1, F2,
F3, F11, F12 and F13 are DONE with no deviation. The table gives each
other former.

| ID | Status on tcc-wasm | Effect on this language | Open item |
|---|---|---|---|
| F4 | DONE as `FreeMon`. HOST-LIMIT: a tree at run time (`formers/tcc-wasm.md:17`) | `FreeMon` (`domain/domain.lang:113`) is the list of the language. A run-time fold over a tree gives `REFUSE_RUNTIME_TREE`. | None |
| F5 | DONE over `Option`, `Sum E` and `List` (`formers/tcc-wasm.md:18`) | A guest uses `FreeMon`, not `List`: a cons-list name gives `REFUSE_CELL`. The domain gives `fmMap` and `fmBind` on `FreeMon` (section 4). | None |
| F6 | DONE over `Nat`, `List` and the domain families. HOST-LIMIT: a tree at run time (`formers/tcc-wasm.md:19`) | A run-time fold over a tree gives `REFUSE_RUNTIME_TREE`. `len` (`domain/domain.lang:153`) is a domain fold. | None |
| F7 | DONE into `List`, with a `Nat` step limit (`formers/tcc-wasm.md:20`) | A guest cannot name `List` (`REFUSE_CELL`). The guest corecursion is the `extend` of the cocell comonad `Stream` (`domain/domain.lang:64`). | None |
| F8 | DONE over `Option` and `List` (`formers/tcc-wasm.md:21`) | A guest uses `filter` on `Option`. The `List` form needs a cons-list name (`REFUSE_CELL`). | None |
| F9 | DONE. HOST-LIMIT: a function value at run time (`formers/tcc-wasm.md:22`) | A function value that must exist at run time gives `REFUSE_RUNTIME_CLOSURE`. | None |
| F10 | DONE over `Type 0` and over `Nat` (`formers/tcc-wasm.md:23`) | A dependent function takes a type in `Type 0` or a `Nat`. | None |
| F14 | DONE for `Type 0` and `Type 1`, without cumulativity (`formers/tcc-wasm.md:27`) | A term of `Type 0` is not also a term of `Type 1`. | None |
| F15 | PARTIAL: only the domain file declares an indexed family (`formers/tcc-wasm.md:28`) | A `family` in a program gives `REFUSE_DATA`. An entry with a `Prod U256 U256` result is not in the abi, so it has no WGSL row. | O14 |

R0 changes the kit words, not the formers. `natAdd` and `natSub` give
`Option Nat` (`src/front/check.c:200-201`). `natDiv` and `natMod` take a
`NonZero Nat` divisor (`src/front/check.c:216-217`). A bare use gives
`REFUSE_PARTIAL` (`probe/CAPABILITY.md` section 4). The kit HOST-LIMITs
make sure that no target can exhaust its stack (N2, `docs/PLAN.md:357`).

The open item ids are the ids of `docs/PLAN.md` section 11, and section 9
uses the same ids.

## 4. Structures

- **Monad**: `pure`, `map` and `bind` on `Option`, `Sum E` and `List` (F5,
  `formers/tcc-wasm.md:18`). A guest uses `FreeMon`, not `List`: a
  cons-list name gives `REFUSE_CELL`. The domain gives `fmBind` and
  `fmMap` on `FreeMon` (`domain/domain.lang:132`, :135), and the bind is
  the fold `fmFold` (:129). The domain also gives `encBind`, `encMap` and
  `encOp2` on `Enc` (:232, :235, :238).
- **Algebra**: `fold` on `Nat`, `List` and each domain family (F6,
  `formers/tcc-wasm.md:19`), and `fmFold` on `FreeMon`. `unfold` runs into
  `List` with a `Nat` step limit (F7, :20), but a guest cannot name
  `List`. The guest corecursion is the cocell comonad `Stream`
  (`domain/domain.lang:64`), with `sExtend` (:68) and `sDuplicate` (:71).
- **Filterable**: `filter` on `Option` and `List` (F8,
  `formers/tcc-wasm.md:21`). A guest uses the `Option` form.

The logs of the design are `FreeMon` values, so the `FreeMon` monad and
`fmFold` act on each of them: the stamp chain `Stamps`
(`domain/domain.lang:381`), the policy history `History` (:430), the
funding log `Funding` (:511), the payout `Payout` (:517) and the decision
log `DecLog` (:900). The host gives the carriers `Nat`, `Option`, `Sum E`
and `List`, and a guest cannot name `List`. The carriers that the host
does not give are the families of `domain/domain.lang` (section 5 gives
the core ones) and the cofamily `Stream`. A program cannot add a family
(`REFUSE_DATA`, F15).

## 5. Core types

The table gives the types of `design/DESIGN.md`, the parts of `Policy`,
and the books and governance types that the operations of section 6 use.
No row is probe-forced: each definition follows the design, and no fact of
`probe/CAPABILITY.md` forces it.

| Type | Meaning (design section) | Definition |
|---|---|---|
| `Time` | The time of the model. The design takes the nonnegative reals, and block time is a sampling of them (Semantic domain) | `Nat` (`domain/domain.lang:377`) |
| `Stamp` | 1 sample of block time: a time, the digest of the previous stamp and a signature (Semantic domain) | `family Stamp := stampOf (stampT : Time) (stampPrev : Digest) (stampR : U256) (stampZ : U256)` (`domain/domain.lang:379`). `Stamps` is `FreeMon Stamp` (:381) |
| Amount | The codomain of settlement: a cancellative commutative monoid (A, 0, +), so that the residual is defined (Semantic domain) | `U256` with `0u`. `u256Add` and `u256Sub` give an `Option U256` (`domain/domain.lang:610`, :547). The domain has no Amount definition |
| `Rate` | The rate that the policy emits: 1 measure over time for the 3 instruments (One measure, three instruments) | `Prod (FreeMon Segment) (FreeMon Mass)` (`domain/domain.lang:416`). A `Segment` (:412) is a constant rate, `segNum` over `segDen`, from `segFrom` to `segUntil`: it gives a stream and a piecewise rate. A `Mass` (:414) is an amount `massAmount` at the time `massAt`: it gives the Dirac comb of a scheduled transfer |
| `Window` | The window of the engagement: its start and its end (Semantic domain) | `family Window := windowOf (winStart : Time) (winEnd : Time)` (`domain/domain.lang:418`) |
| Auth (`Grant`) | The authorization part of the policy: the instrument that moves the residual (push, pull or stream), and a pause (Semantic domain, One measure, three instruments) | `family Grant := grantOf (grantInstrument : Instrument) (grantPaused : Flag)` (`domain/domain.lang:422`). `Instrument` (:420) has the constructors `instrPush`, `instrPull` and `instrStream`. The domain `Auth` (:356) is a different family: a signature `authOf (authR : U256) (authZ : U256)` |
| `Dispute` | The dispute policy: the map that a dispute applies to the residual only (Semantic domain, Books) | `FreeMon U256` (`domain/domain.lang:424`). `escDispute` reads the share of a verdict from it, in basis points (:629) |
| `Policy` | What governance decides: a rate, a window, an authorization and a dispute policy, not a balance (Semantic domain) | `family Policy := policyOf (polRate : Rate) (polWindow : Window) (polGrant : Grant) (polDispute : Dispute)` (`domain/domain.lang:426`) |
| `Engagement` | The signed scope that the rate is earned against: the client, the firm, the `scopeHash` of the engagement letter and the window (Semantic domain) | `family Engagement := engagementOf (engClient : Point) (engFirm : Point) (engAsset : Digest) (engScope : Digest) (engWindow : Window) (engSigned : Option Time) (engCancelled : Option Time)` (`domain/domain.lang:428`). `engScope` is the `scopeHash`. The policy is not a field: `History` gives it |
| `History` | The policy in force at each time, which Book L reads off Gov L (Semantic domain, Self-reference) | `FreeMon (Prod Time Policy)` (`domain/domain.lang:430`) |
| `Book` | The books of 1 engagement: the funded log, the withdrawn amount and the last period close (Books) | `family Book := bookOf (bookLog : Funding) (bookWithdrawn : U256) (bookClosed : Option Time)` (`domain/domain.lang:513`), with `Funding` = `FreeMon (Prod Time U256)` (:511) |
| `Escrow` | The custody of 1 engagement: the engagement and its book (Books, Admitted operations) | `family Escrow := escrowOf (escEng : Engagement) (escBook : Book)` (`domain/domain.lang:515`) |
| `Payout` | The transfers out of custody (Books, Admitted operations) | `FreeMon Instruction` (`domain/domain.lang:517`), with `family Instruction := instructionOf (ixPayee : Point) (ixAsset : Digest) (ixAmount : U256) (ixAt : Stamp)` (:509) |
| `Books A` | A behavior over the stamps: a value now, and the next behavior after each stamp. The design acts on a behavior from time to amount (Stance) | `cofamily Books (A : Type 0) := booksCorec (booksNow : A) (booksNext : Stamp -> Books A)` (`domain/domain.lang:731`) |
| Decision | A decision of the choice rule F. policyOf maps each decision to a policy (Semantic domain) | `Nat` below the number n of decisions. `govD` refuses a decision d when n <= d (`docs/PLAN.md:442`). The policy map is the argument `pa : Nat -> Policy` of `govPi` (`domain/domain.lang:882`) |
| `PolicyTally` | The table that the rule in force reads: the tally of each decision (Self-reference) | `FreeMon (Option Count)` (`domain/domain.lang:850`) |
| `PolicyBallot` | 1 one-hot ballot: 1 ideal encrypted verdict for each decision, a truncated witness that the verdicts are one-hot, and a link tag (Stance, `docs/PLAN.md:237`) | `Sigma (cs : FreeMon (Enc Verdict)) (Prod (ZKProof (HotOk cs)) Digest)` (`domain/domain.lang:806`). `PolicyBox` is `FreeMon PolicyBallot` (:815) |
| `DecLog` | The decision log: the time of each change of the decision. It holds the frozen past (Self-reference) | `FreeMon (Prod Time Nat)` (`domain/domain.lang:900`) |
| `RegimeD` | The regime of a run of tallies: Arrow-Debreu or Arrow-impossibility (Self-reference) | The constructors `regimeAD` and `regimeAI` (`domain/domain.lang:911`) |

2 probe facts limit the rows. A `Nat` is at most 2^64 - 1
(`probe/CAPABILITY.md:287`), so a `Time` is too. A program cannot declare a
family (:294), so each family of the table is a domain family.

## 6. Core operations

The operations are defs of `domain/domain.lang`, and `design/DESIGN.md`
gives their meaning. The design names map to domain names: earned is
`earnedAt`, recognized is `recognizedAt`, unearned is `unearnedAt`,
withdrawable is `withdrawableAt`, fund is `escFund`, settle is
`escSettle`, cancel is `escCancel` and dispute is `escDispute`. Gov is
`govD` and Gov_pi is `govPi` (`docs/PLAN.md:447`). The design rateAt has
no def of its own: `earnedFrom` (`domain/domain.lang:492`) folds the
`History` entries and adds 1 term (`entryTerm`) for each entry, and
`accrue` (:474) gives the integral of 1 rate.

A time difference uses `natSub`, and an amount sum or difference uses
`u256Add` or `u256Sub`. Each gives an `Option` (`probe/CAPABILITY.md:287`,
`domain/domain.lang:475`), so the measures give an `Option U256`.

| Operation | Type | Meaning (design section) |
|---|---|---|
| `accrue` | `Rate -> Time -> Time -> Time -> Verdict -> Option U256` (`domain/domain.lang:474`) | The integral of 1 rate from lo to hi, through the cumulative mass `gAt`. The times of the rate count from s, and `entryTerm` gives the start of the engagement window as s. An empty interval gives `some 0u` (One measure, three instruments) |
| `earnedAt` | `History -> Engagement -> Time -> Option U256` (`domain/domain.lang:495`) | earned(L, E, t): the integral of the rate over the engagement up to t. It is `earnedFrom` with no lower bound (One measure, three instruments) |
| `earnedBetween` | `History -> Engagement -> Time -> Time -> Option U256` (`domain/domain.lang:498`) | earned(t1, t2): `earnedFrom` with the lower bound t1. The design uses it for earned(t2) = earned(t1) + earned(t1, t2) (Admitted operations) |
| `fundedAt` | `Funding -> Time -> U256` (`domain/domain.lang:528`) | funded(t): the amount of the last log entry at or before t, else `0u` (Books) |
| `recognizedAt` | `History -> Engagement -> Funding -> Time -> Option U256` (`domain/domain.lang:543`) | recognized(t) = min(funded(t), earned(t)) (Books) |
| `unearnedAt` | `History -> Engagement -> Funding -> Time -> Option U256` (`domain/domain.lang:546`) | unearned(t) = funded(t) - recognized(t) (Books) |
| `withdrawableAt` | `History -> Escrow -> Time -> Option U256` (`domain/domain.lang:549`) | withdrawable(t) = recognized(t) - withdrawn(t), with withdrawn the `bookWithdrawn` field (Books) |
| `engage` | `Point -> Point -> Digest -> Digest -> Window -> Auth -> Option Escrow` (`domain/domain.lang:715`) | Open an engagement with an empty book when the client signature on its digest holds (Semantic domain) |
| `countersign` | `Stamp -> Auth -> Escrow -> Option Escrow` (`domain/domain.lang:718`) | Set `engSigned` to the stamp time when the firm signature holds. A signed engagement refuses a second countersign (Semantic domain) |
| `escFund` | `Stamp -> U256 -> Escrow -> Option (Prod Escrow Payout)` (`domain/domain.lang:609`) | Fund: add the amount to funded at the stamp time, with an empty payout. It does not touch the history or the earned amount (Admitted operations) |
| `escSettle` | `History -> Stamp -> U256 -> Escrow -> Option (Prod Escrow Payout)` (`domain/domain.lang:616`) | Settle: add v to withdrawn when the sum is at most recognized, and pay v to the firm. Under a pull grant (`pullAt`), v also adds to funded first (Admitted operations) |
| `escSettleR` | `History -> Stamp -> Escrow -> PayLog -> Option (Prod Escrow Payout)` (`domain/domain.lang:696`) | Settle from a record of instructions: take withdrawn from `settledIn` of the `PayLog`, then call `escSettle` (Admitted operations) |
| `escCancel` | `History -> Stamp -> Escrow -> Option (Prod Escrow Payout)` (`domain/domain.lang:621`) | Cancel at t*: mark the engagement cancelled, take unearned(t*) out of funded and pay it to the client. The firm keeps the recognized amount (Admitted operations) |
| `escDispute` | `History -> Stamp -> Nat -> Escrow -> Option (Prod Escrow Payout)` (`domain/domain.lang:628`) | Dispute with verdict v: split unearned(t) by the share of v in the dispute policy. The firm gets the share and the client gets the rest. It acts on the residual only (Admitted operations) |
| `escClose` | `History -> Stamp -> Time -> Escrow -> Option (Prod Escrow U256)` (`domain/domain.lang:636`) | Period close at T: give recognized(T) minus recognized at the last close (0 before the first close), and set `bookClosed` to T (Books) |
| `chooseD` | `Nat -> PolicyTally -> Option Nat` (`domain/domain.lang:876`) | The choice rule F at the rule position r: 0 is plurality and 1 is majority. Another position gives none (Semantic domain) |
| `govD` | `Nat -> Nat -> PolicyTally -> Option Nat` (`domain/domain.lang:879`) | Gov L: the decision of the rule in force on a tally. A decision d with n <= d gives none (Stance, Self-reference) |
| `govPi` | `(Nat -> Policy) -> Nat -> Nat -> PolicyTally -> Option Policy` (`domain/domain.lang:882`) | Gov_pi L = policyOf after Gov L: the policy of the decision, through the policy map pa (Self-reference) |
| `govC` | `Polity -> Nat -> PolicyTally -> Option Nat` (`domain/domain.lang:886`) | `govD` at the rule position of the constitution (`rulePos`). An amendment can change that rule (:289) (Self-reference) |
| `govStep` | `Nat -> Nat -> Time -> PolicyTally -> DecLog -> DecLog` (`domain/domain.lang:905`) | Add (t, d) to the decision log when the decision d is not the last one. With no decision the log does not change: accrual freezes at the last decision (Self-reference) |
| `regimeD` | `Nat -> Nat -> FreeMon PolicyTally -> RegimeD` (`domain/domain.lang:913`) | `regimeAD` (Arrow-Debreu) when each tally has a decision, else `regimeAI` (Arrow-impossibility) (Self-reference) |
| `castPolicy` | `Nat -> PolicyBallot -> PolicyBox -> Option PolicyBox` (`domain/domain.lang:820`) | Add 1 policy ballot unless its link tag is used, when it has 1 vote for each of the n decisions (Stance) |
| `amendAll` | `FreeMon Decided -> Polity -> Polity` (`domain/domain.lang:788`) | amend = Gov on the constitution: it folds the accepted decisions in order (:297) (Stance) |

Some defs support these operations, but the design does not name them.
`stampSnoc` (`domain/domain.lang:402`) adds a stamp when `stampOk` holds,
and `later` (:396) compares 2 times. `paySnoc` (:676) adds a pay entry
when `payOk` holds and the entry is not in the log (`verify`). `append`
(:361) is the record operation of the kit.

`booksLaw` (`domain/domain.lang:745`) is a `Path` with the proof `refl`:
`booksPair` of the behavior `booksLast` on the chain `booksChain` equals
the fold `fmChain` of `booksStep` from none on that chain.

## 7. Target and instance encoding

The compiler has 3 writers: the Wasm writer `src/wasm.c` (S8), the ES
module and JSON writers `src/js.c` and `src/json.c` (S9), and the WGSL
writer `src/wgsl.c` (S10) (`docs/PLAN.md:619` to :621).
`src/front/lower.c` lowers a program to the IR of `src/ir.h` (:661).

- **Wasm (S8)**: the writer has no trap flag, no `IR_STMT_TRAP` and no
  Wasm `unreachable` (`docs/PLAN.md:659`). An `Option` entry returns the
  address of its payload, and 0 is none (:660). Each list walk and each
  entry heap has a static bound, else `REFUSE_CAPACITY` (:69). The memory
  has min = max = P + 1 pages: P is the least power of two that holds the
  heap, at most 16384, and page P is a guard page. Each load and store
  address is masked with P * 65536 - 1 (:70 to :72). The prelude heap
  bytes are hand constants, and alloc has no check and no `memory.grow`
  (:288).
- **ES module and JSON (S9)**: an entry with an `Option` result returns
  `null` for none, else the payload as a BigInt (`Nat`, `Flag` or `U256`)
  (`docs/PLAN.md:289`). The secret marks are the key shares, the nonces
  and the decryption key (`Secret`, `KeyShare`, `Nonce`, `DecKey`). They
  are the marks for `REFUSE_TARGET_SECRET` (:638 to :640).
  `test/jstool.c` runs each ES module with `node --input-type=module`
  through popen, and a `trap` line is a FAIL (:290).
- **WGSL (S10)**: the writer gives a batch proof check and a batch
  `earned` (`docs/PLAN.md:621`). The Metal backend does not compile 6
  entries: `rand`, `randWith`, `slot` and `slotWith` of
  `examples/onehot.lang`, and `dleq` and `dleqWith` of
  `examples/threshold.lang`. `test/wgsl/internal.txt` lists them (O12,
  :672 to :677). `zkVerify` does not lower, because the IR has no
  hash-to-point, `lift_x` or square root (O13, :681). Instead
  `test/wgsl/zk.lang` checks a guest bLSAG ring of 2 keys (:683). An entry
  with a `Prod U256 U256` result has no WGSL row (O14, :689).

| Type | Encoding |
|---|---|
| An `Option` entry result | Wasm: the payload address, and 0 is none (`docs/PLAN.md:660`). ES module: `null` for none, else a BigInt (:289) |
| A list value | A static length bound, else `REFUSE_CAPACITY` (`docs/PLAN.md:269`) |
| `KeyShare`, `Nonce`, `DecKey` | The secret marks of `REFUSE_TARGET_SECRET` (`docs/PLAN.md:638`) |
| A `Prod U256 U256` entry result | Not in the abi, so no WGSL row (`docs/PLAN.md:689`) |
| A type, a universe, an index, the content of a proof | Erased on Wasm (`formers/tcc-wasm.md:10`, :52) |

A proof erases: `ZKProof W` is `Trunc1 W` (`domain/domain.lang:260`), and
the content of a proof erases on Wasm (`formers/tcc-wasm.md:10`). A
`ZKProof` value holds a witness, not a serialized cryptographic proof.

The tests run each module: `test/wasmtool.c` (Wasm), `test/jstool.c` (ES
module) and `test/wgsltool.c` (WGSL). S8 added `test/notraptool.c` and
differential vectors (`docs/PLAN.md:619`).

## 8. Host and target

The host is TinyCC: `tcc -v` gives 0.9.28rc 2026-09-04 (mob@0fb54300,
AArch64 Darwin). The repo has a `PIN` file (7 lines). It records the `tcc`
version and path, `node` v23.10.0, and `wasm-validate` and `wasm-objdump`
1.0.39. The gate uses `node`, `wasm-validate` and `wasm-objdump` from
`/opt/homebrew/bin`. `PIN` also records the kit host check `make -C
hosts/tcc-wasm check` GREEN on 2026-10-08.

The targets are Wasm, an ES module with JSON, and WGSL (section 7). One
build writes one target: `-o` (Wasm), `--js` or `--wgsl`
(`probe/CAPABILITY.md:292`). `probe/CAPABILITY.md` (S16) holds the host
probe in 10 sections. Its consequences (:279) set the design:

- A program is 1 file, and the domain names need no import (:281).
- Recursion is `fold` and `unfold`, with no `def rec` (:282).
- A program with a kit type such as `Nat` runs with `langc --kit` (:284).
  The base span refuses it with `REFUSE_KIT`.
- A `Nat` is at most 2^64 - 1. An overflow and a division by 0 give none
  and exit 0, with no trap (:287 to :290).
- A program cannot declare a family, so F15 stays PARTIAL (:294).
- A build of `examples/dao.lang` costs 1.3 s to 2.0 s and 423 MB to 430
  MB, and an unused definition adds no byte to the module (:295 to :297).
- An axiom stops the check with `REFUSE_AXIOM` and exit 1 (:298).

The gate is `make check`. It uses the tools of section 7 and the host
tools of `PIN`. `docs/VALIDATION.md` records its run.

## 9. Open items

The open items are the O rows of `docs/PLAN.md` section 11
(`docs/PLAN.md:629` to :700, O1 to O16). Each row gives its slice and its
ruling. 5 rows are CLOSED: O2, O4, O6, O7 and O9. The rows below stay
open, each for a later plan (Q-S16-5 (a)).

- O1. The kit base data is `Scalar`, `Point`, `Digest` and `U256`. A slice
  that needs more data records why first (`docs/PLAN.md:631`).
- O3. retainer-lang SPEC.md still says that EVM output is PLANNED.
  accrue-lang takes only the domain, so this does not apply here (:636).
- O5. The test vectors come from the published standards: NIST CAVP for
  SHA-256, SEC 2 for secp256k1 and the FROST RFC 9591 vectors (:646).
- O8. The residual `a - b` with a proof `b <= a` (Q-S3-3) needs a
  dependent `Flag` eliminator first. S5b takes L4 as refl on closed
  values (Q-S5-3) (:655).
- O10. The `NAME_UNKNOWN` return at the end of `var_core` cannot run. It
  stays as a guard (:664).
- O11. `govStep` takes the time t as given, so the time order of the
  decision log is the order of the caller. A time guard is for a later
  plan (:669).
- O12. The Metal backend does not compile 6 entries of
  `examples/onehot.lang` and `examples/threshold.lang`.
  `test/wgsl/internal.txt` lists them (:672).
- O13. `zkVerify` does not lower: the IR has no hash-to-point, `lift_x` or
  square root. S10 checks a guest bLSAG ring of 2 keys on WGSL instead
  (:681).
- O14. An entry with a `Prod U256 U256` result is not in the abi, so it
  has no WGSL row (:689).
- O15. The S12 mutant M5 of `json_string` in `src/json.c` survived
  (Q-S12-8) (:693).
- O16. The cofamily caller of `checker_reset` in `src/front/check.c` was
  not compared with cairn-lang (:698).

## 10. Milestones

The plan maps its slices to the kit milestones by content (`docs/PLAN.md`
section 10). The agent slices S11 to S15 (A1 to A6) have their own row.

| Milestone | Slices | Content | Commits (date) | Status |
|---|---|---|---|---|
| M0 | S0 to S3 | Host probe (`probe/CAPABILITY.md`); the seed and the gate; the S1 rulings; the port of the cairn-lang kernel at 0743108; R0 (`REFUSE_PARTIAL`, checked `U256`, `mulDiv`); the core types construct and check; refusal list; examples and tests | 92349c9, 423687f, d6e84d3 (2026-10-27); dbf4e11 (2026-10-29) | DONE (S16 fills the probe file) |
| M1 | S4 to S7 | Core operations: time as a `Stamp` argument (`REFUSE_CLOCK`, the stamp chain, L12); Part E (the accrual data, `earnedAt`, the escrow books and operations); Part D (the policy ballots, the governance decisions, the constitution record, the secret marks, the host rows); Part S (the authenticated payment records, the residual settlement) | c3ce5ba (2026-10-29); 84ce518, f701461, b4167d9, dbb592e (2026-10-30); 7338754, d1017f1 (2026-10-31) | DONE |
| M2 | S8 to S10 | Targets: Wasm with static memory and no trap forms, the ES module with the JSON ABI, WGSL compute with the batch proof check and the batch `earned` | 20852a1 (2026-11-01), 025f73d (2026-11-02), d87d674 (2026-11-03) | DONE |
| A | S11 to S15 | Agent work: `langc fmt` (A1), `langc check --json` (A2), `langc mcp` (A4), SKILL.md (A3, A5), the token baseline (A6) | f8a02d7, 02964ec (2026-11-03); 2c804b7 (2026-11-04); 238ca53, d3ddc7b (2026-11-05) | DONE |
| M3 | none | Hardening: speed, limits, a checked certificate for the output | none | PARTIAL: the refusal codes, `REFUSE_CAPACITY` and the mutant rows give the limits; no speed work, no checked certificate |
| Close | S16 | SPEC.md, `probe/CAPABILITY.md`, `docs/VALIDATION.md`, the final gate | this commit | DONE |

Status 2026-11-06 (S16 complete): M0 to M2, the agent work and the S16
close are DONE, and M3 is PARTIAL. The plan is CLOSED.
