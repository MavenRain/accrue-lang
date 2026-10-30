# The domain

`domain/domain.lang` holds the domain of accrue-lang: the families and the
definitions that a program can use. At build time, `tcc -run
gen/embed.c` copies the file into `build/domain.c`. Thus the domain is
part of `build/langc`. `langc` loads the domain first, then the kit
prelude, then the program.

## The rules

- Only this file can declare a family (rule R1). A `family`, a
  `cofamily` or a `sealed family` in a program gives `REFUSE_DATA`.
- Only this file can declare a `law def` (rule O7). A `law def` in a
  program gives `REFUSE_DATA`.
- Each family, constructor, field and definition name in this file
  becomes a core name (rule R4). A program cannot use a core name as the
  name of a new definition (`REFUSE_NAME`).
- The kit prelude in `src/front/front.c` adds the kit families, for
  example `Env` and `Out`. A guest program cannot declare them
  (`REFUSE_KIT`).
- A program and `domain/domain.lang` cannot use a partial kit name (for example
  `div` or `sub`), or the `Option` result of a checked kit word where a
  plain value is wanted (`REFUSE_PARTIAL`, Q-S3-1, Q-S3-21). Take the
  `Option` apart with `option` or `bind`.
- A program and `domain/domain.lang` cannot declare, bind or use a clock
  word (`now`, `block`, `timestamp` or `clock`): `REFUSE_CLOCK`, Q-S4-1.
  Take the time as a `Stamp` argument.

## Declaration forms

The file uses these forms.

- `family NAME (param : T)* := ctor (field : T)* | ...`. A constructor
  takes the parameters first: `fmUnit A`, `fmMul A x y`.
- An indexed family gives the index of each constructor:
  `family Pos : Size -> Type 0 := posFirst (firstSize : Size) : Pos (szS firstSize) | ...`.
- A path constructor takes interval variables `(i : I)` and a boundary
  in brackets: `loop (i : I) [ ~i -> base, i -> base ]`.
- `cofamily NAME (param : T)* := corec (destructor : T)*`. For example,
  `Stream` has the destructors `sHead` and `sTail`.
- `sealed family NAME ...` and `law def NAME : T := term`: see the guest
  rules below.
- `def NAME : T := term`.
- Line comments start with `--`.

## What the file holds

The file has these sections, in this order. A line comment at the start
of each section gives its rule or ruling.

| Section | Names |
|---|---|
| The Kan library | `transport`, `subst`, `J`, `JRefl` (all by `transp`), `trans` (by `hcomp`) |
| Propositions and sets | `isProp`, `isSet`, `isSetFun` (when `B` is a set, `A -> B` is a set) |
| Higher inductive families | `S1` (the circle), `Trunc1` (the propositional truncation), `Trunc0` (the set truncation) |
| Sizes and streams | `Size`, the cofamily `Stream` with `sExtend` and `sDuplicate`, the parity quotient `Parity` with `sizeFlip` and `sizeEven` |
| The empty family and the bag | `Empty`, `Bag` (the free commutative monoid) |
| The free monoid | `FreeMon`, `fmCons`, `fmIsSet`, `fmFold`, `fmBind`, `fmMap`, `endoThen`, `fmChain`, `fmRun`, `len` |
| Part R, the record | `Digest`, `Entry`, `Record`, `snoc`, `tagGenesis`, `tagAppend`, `H3`, `chainFrom`, `head`, `opSucc`, `Verdict`, `verdictYes`, `foundAt`, `verify`, `prefix`, `Inclusion`, `recTake`, `recDrop`, `inclusion`, `verifyAt`, `Auth`, `authOf`, `authR`, `authZ`, `authOk`, `append` |
| S4, the stamp chain | `Time`, `Stamp`, `stampOf`, `stampT`, `stampPrev`, `stampR`, `stampZ`, `Stamps`, `tagStamp`, `tagStampGenesis`, `stampStep`, `stampHead`, `lastT`, `later`, `stampOk`, `stampSnoc` |
| S5a, Part E | `Segment`, `segmentOf`, `segFrom`, `segUntil`, `segNum`, `segDen`, `Mass`, `massOf`, `massAt`, `massAmount`, `Rate`, `Window`, `windowOf`, `winStart`, `winEnd`, `Instrument`, `instrPush`, `instrPull`, `instrStream`, `Grant`, `grantOf`, `grantInstrument`, `grantPaused`, `Dispute`, `Policy`, `policyOf`, `polRate`, `polWindow`, `polGrant`, `polDispute`, `Engagement`, `engagementOf`, `engClient`, `engFirm`, `engAsset`, `engScope`, `engWindow`, `engSigned`, `engCancelled`, `History`, `u256Min`, `natMax`, `natMin`, `Acc`, `accIsSet`, `accZero`, `accAdd`, `accOut`, `segTerm`, `gAt`, `optMin`, `optMax`, `accrue`, `entryTerm`, `EarnRun`, `earnedFrom`, `earnedAt`, `earnedBetween` |
| The encrypted values | the sealed family `Enc`, `encBind`, `encMap`, `encOp2`, the law defs `decrypt` and `observe` |
| Quorum and proofs | `Quorum`, the law def `open`, `ZKProof`, the sealed family `Transcript`, the law def `verified` |
| Part D, the DAO | `Point`, `Count`, `verdictNo`, `isSetProd`, `encEtaOf`, `encIsSet`, `Polity`, `makePolity`, `members`, `tAppend`, `tAmend`, `tOpen`, `keyOpen`, `keyAppend`, `polityIsSet`, `Proposal`, `Bit`, `SealedBallot`, `mkBallot`, `ballotVote`, `linkTag`, `Box`, `tagUsed`, `castBallot`, `tallyBox`, `countVotes`, `Decided`, `decideYes`, `enact`, `stepD`, `amendAll` |
| Positions, words and orbits | `Pos`, `Word`, `sizePred`, `consP`, `swapFront`, `actW`, `ChoiceBag`, `OrbitN` |
| The charter | `Charter` (an inductive-inductive family) |
| Equivalences | `isContr`, `fiber`, `isEquiv`, `Equiv`, `idEquiv`, `isoCap`, `isoPath`, `isoToEquiv`, `ua`, `uaArrow` |

## Guest programs and the domain

- A guest program uses `FreeMon` for a list. The cons list `List` is kit
  only (the host ABI). A guest `List` gives `REFUSE_CELL`.
- A guest program cannot name the kit types `Nat`, `U256` and `Flag`
  (`REFUSE_KIT`). The domain gives guest names for the values that a
  guest needs: `Digest` and `Entry` for a digest, `Count` and `opSucc`
  for a position or a count, `Verdict` and `verdictYes` for a result.
- A guest elim, fold or projection of `Enc`, or a use of a law def,
  outside a `Trunc1` argument makes the guest definition law-only. A
  law-only definition must have a law type (a `Path` or `Trunc1`
  codomain), else `REFUSE_LAW`.
- `examples/record.lang` uses Part R in a guest program. It holds the
  laws L1 to L5 and the defs of the record rows in
  `test/eval/expect.txt`.
- `examples/append.lang` holds the laws L10 and L11 (a good and a bad
  Auth under `keyAppend`), and `examples/outlog.lang` is the run row of
  the host append (O20).
- `examples/stamps.lang` holds the law L12 (S4): a good first and second
  stamp go at the end of the chain, and a bad signature, an old or equal
  time, a wrong prev or another key gives none.
- `examples/earned.lang` holds the laws L1 and L2 (S5a): earned at t2 is
  earned at t1 plus earned on (t1, t2], earned is 0 before the start, and
  earned does not go down. It also holds the cuts of an entry: each bound,
  a pause, no signed time, a bad Segment and a sum past the U256 maximum.
- `examples/causality.lang` holds the law L6 (S5a): an amendment at t*
  does not change earned at a t <= t*.

## How to change the domain

1. Edit `domain/domain.lang`. Keep each new name apart from the core
   names.
2. Change the examples and the expect files that use a changed name.
3. Run `make check`. The build makes `build/domain.c` again when
   `domain/domain.lang` changes.
