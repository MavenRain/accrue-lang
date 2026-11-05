---
name: accrue-lang
description: Write, check and repair accrue-lang programs (a Cubical TT language for off-chain retainer protocols, governed by a threshold DAO) with the langc checker. Use it when the task is an off-chain retainer protocol (a client funds an engagement, a rate accrues over a window, and the firm settles what it earned).
---

# accrue-lang

## Routing rule

Use accrue-lang when the task is an off-chain retainer protocol: a client
funds an engagement, a rate accrues over a window, and the firm settles
what it earned. The examples are a retainer (G1), a closed stream with a
cancel (G2), an encrypted policy vote (G3), a rate change during the
engagement (G4) and a change of the rules by the DAO (G5). A threshold DAO
governs the policy (rate, window, authorization, dispute) off chain: it
encrypts its ballots (FHE), proves each step (ZK) and opens only by
threshold (MPC). Each program reads time only as a `Stamp` argument. No
program can read a local clock, and no program can trap. The README table
(README.md, "When to use accrue-lang") gives the yes and no rows.

## The loop

1. Write FILE.lang: one `def NAME : TYPE := TERM` for each value. A guest
   file names the domain defs only (design/DESIGN.md). A kit item (for
   example a Nat literal, a word literal or the name `flagNo`) needs the
   kit mode: a kit list names the file, and `langc --kit` checks it. Each
   example below is a kit file (examples/skill/kit.txt).
2. Run `build/langc --kit check --json FILE.lang` for a kit file
   (`build/langc check --json FILE.lang` for a guest file). It writes one
   JSON line on stdout, with the `diagnostics` array and the `progress`
   object (docs/CHECK-JSON.md). Exit 0: the checker accepts the file. Exit
   1: the checker refuses the file. Exit 2: there is no JSON line, and
   stderr has the text diagnostic `OOM`, `IO` or `USAGE`.
3. For each diagnostic, `build/langc explain CODE` gives one line: the
   code, its meaning and its section of docs/host/README.md. Change the
   file, then do step 2 again.
4. Run `build/langc fmt FILE.lang --check`. No output and exit 0: the text
   is canonical. Else it prints `FILE.lang:LINE: not canonical` and exits
   1. Then write the output of `build/langc fmt FILE.lang` to a new file,
   and replace FILE.lang with it.
5. Run `build/langc --kit eval FILE.lang NAME` (`build/langc eval
   FILE.lang NAME` for a guest file) for the value of the def NAME. An
   Option of a base kind prints as its payload (`20u`, `1`) or as `none`.
   An Option of a record prints as `some (...)`. An Enc prints as `encEta
   (Option Nat) (some 2)`. A U256 value ends in `u`.

`build/langc mcp` gives the same verbs to an agent as a Model Context
Protocol server on stdin and stdout (docs/MCP.md). Its 5 tools are
`check`, `fmt`, `eval`, `build` and `explain`. With `--kit`, each tool
runs in the kit mode.

## Verbs

`build/langc` with no argument writes a USAGE line and the usage text on
stderr, and exits with code 2:

```text
langc: USAGE: -: expected a command and a program path
usage: langc [--kit] check [--json] PROG
       langc [--kit] eval PROG NAME [ARGS...]
       langc [--kit] run PROG SCRIPT
       langc [--kit] ir PROG
       langc [--kit] abi PROG [--text]
       langc [--kit] build PROG -o OUT [--runtime]
       langc [--kit] build PROG --js OUT.mjs
       langc [--kit] build PROG --wgsl OUT.wgsl
       langc [--kit] prove PROG NAME [POLL]
       langc fmt PROG [--check]
       langc explain CODE
       langc [--kit] mcp
--kit: the kit mode, the trusted test scope of the kit gate
```

- `check [--json] PROG`: check PROG. Text: `ok` on stdout, or one line on
  stderr.
- `eval PROG NAME [ARGS...]`: the value of the def NAME.
- `fmt PROG [--check]`: print the canonical text of PROG. With `--check`,
  no output and exit 0 when PROG is canonical.
- `explain CODE`: one line on the diagnostic code CODE.
- `mcp`: the MCP server (docs/MCP.md).
- `run`, `ir`, `abi`, `build` and `prove`: the back end and the targets
  (docs/host/README.md). This skill does not use them.

## Examples

The examples go from easy to hard. Run each command from the repository
root. The block below each command is its stdout and its stderr. The gate
(test/skilltool.c) runs each command and compares the output and the exit
code with the text.

### G1

A retainer. `egGood` opens the engagement with the client signature, and
`csGood` adds the firm countersignature at 20. `csFundCtl` funds 200 at
30, countersigns at 40 and reads the recognized value at 50. It is a kit
file (examples/skill/kit.txt), so each command uses `--kit`.

File: examples/skill/g1-retainer.lang

```lang
-- PLAN row 529 (G1): a retainer. egGood opens the engagement with the client
-- signature and csGood adds the firm countersignature at 20. csFundCtl funds
-- 200 at 30, countersigns at 40 and reads the recognized value at 50.
def rB : Rate := pair (fmEta Segment (segmentOf 0 50 2u 1u)) (fmEta Mass (massOf 0 40u))

def dB : Dispute := fmMul U256 (fmEta U256 2500u) (fmEta U256 5000u)

def histOf : Instrument -> History :=
  fun (i : Instrument) => fmEta (Prod Time Policy) (pair 0 (policyOf rB (windowOf 0 100) (grantOf i flagNo) dB))

def hPush : History := histOf instrPush

def sAt : Time -> Stamp := fun (t : Time) => stampOf t 0u 0u 0u

def Step : Type 0 := Option (Prod Escrow Payout)

def bkRec : History -> Time -> Escrow -> Option U256 :=
  fun (h : History) (t : Time) (x : Escrow) => elim (fun (u : Escrow) => Option U256) (fun (e : Engagement) (b : Book) => elim (fun (u : Book) => Option U256) (fun (l : Funding) (w : U256) (c : Option Time) => recognizedAt h e l t) b) x

def cK : Point := 0xf2bb893b2497c632d4b2e1bcc9f3565efe400130e944ea3a4ba689a46220baa2u

def fK : Point := 0xfa043432a49afe1c8e4ad857ee8935e7cae984e9f655fbd12117d48849d5727du

def engR : U256 := 0xae00a0168cdcf5811726c150336492d5be4ebb5857965ddb7acde38ffca7a33cu

def engZ : U256 := 0x2c8cc28029354ca5e4f7d87c6807bd594a4bb1ce4de3fb875bbfcf9dbb033828u

def ctrR : U256 := 0x4c496fbafa6f2c546472f4b21bceb13b71945c5b6e6339783a0db63be6b38cfu

def ctrZ : U256 := 0x64e219dee5e8683ee2fb3f4ff4bb43f5a29b34a0393e883c6b88327e5f1c55cbu

def eS : Engagement := engagementOf cK fK 3u 4u (windowOf 10 60) none none

def xS : Escrow := escrowOf eS (bookOf (fmUnit (Prod Time U256)) 0u none)

def egGood : Option Escrow := engage cK fK 3u 4u (windowOf 10 60) (authOf engR engZ)

def csGood : Option Escrow := countersign (sAt 20) (authOf ctrR ctrZ) xS

def csLateFund : Step := escFund (sAt 30) 200u xS

def csFundAt : Time -> Option U256 :=
  fun (t : Time) => bind csLateFund (fun (p : Prod Escrow Payout) => bind (countersign (sAt t) (authOf ctrR ctrZ) (first p)) (bkRec hPush 50))

def csFundCtl : Option U256 := csFundAt 40
```

`build/langc --kit check examples/skill/g1-retainer.lang` gives:

```text
ok
```

The exit code is 0.

`build/langc --kit eval examples/skill/g1-retainer.lang egGood` gives:

```text
some (escrowOf (engagementOf 109791056892004066315013292490115010174505158101236297434859555777157806471842u 113085639790218053198109643608074705563682068750669178417462628566113730261629u 3u 4u (windowOf 10 60) none none) (bookOf (fmUnit (Prod Nat U256)) 0u none))
```

The exit code is 0.

`build/langc --kit eval examples/skill/g1-retainer.lang csGood` gives:

```text
some (escrowOf (engagementOf 109791056892004066315013292490115010174505158101236297434859555777157806471842u 113085639790218053198109643608074705563682068750669178417462628566113730261629u 3u 4u (windowOf 10 60) (some 20) none) (bookOf (fmUnit (Prod Nat U256)) 0u none))
```

The exit code is 0.

`build/langc --kit eval examples/skill/g1-retainer.lang csFundCtl` gives:

```text
20u
```

The exit code is 0.

### G2

A closed linear stream and a cancel. `hL` streams 1000 from 10 to 17, so
the earned value at the end is the deposit. `xC` cancels the push book at
30, and `payCancel` pays the unearned 120 back to the client. It is a kit
file (examples/skill/kit.txt), so each command uses `--kit`.

File: examples/skill/g2-cancel.lang

```lang
-- PLAN row 530 (G2): a closed linear stream and a cancel. hL streams 1000
-- from 10 to 17, so earned at the end is the deposit (l2Stream). xC cancels
-- the push book at 30 and payCancel pays the unearned 120 back to the client.
def grantOn : Grant := grantOf instrStream flagNo

def eL : Engagement := engagementOf 1u 2u 3u 4u (windowOf 10 17) (some 10) none

def hL : History :=
  fmEta (Prod Time Policy) (pair 0 (policyOf (pair (fmEta Segment (segmentOf 0 7 1000u 7u)) (fmUnit Mass)) (windowOf 0 100) grantOn (fmUnit U256)))

def l2Stream : Path (Option U256) (earnedAt hL eL 17) (some 1000u) := refl

def eStream12 : Option U256 := earnedAt hL eL 12

def eStream17 : Option U256 := earnedAt hL eL 17

def rB : Rate := pair (fmEta Segment (segmentOf 0 50 2u 1u)) (fmEta Mass (massOf 0 40u))

def dB : Dispute := fmMul U256 (fmEta U256 2500u) (fmEta U256 5000u)

def histOf : Instrument -> History :=
  fun (i : Instrument) => fmEta (Prod Time Policy) (pair 0 (policyOf rB (windowOf 0 100) (grantOf i flagNo) dB))

def hPush : History := histOf instrPush

def eB : Engagement := engagementOf 1u 2u 3u 4u (windowOf 10 60) (some 10) none

def xE : Escrow := escrowOf eB (bookOf (fmUnit (Prod Time U256)) 0u none)

def sAt : Time -> Stamp := fun (t : Time) => stampOf t 0u 0u 0u

def Step : Type 0 := Option (Prod Escrow Payout)

def andThen : Step -> (Escrow -> Step) -> Step :=
  fun (o : Step) (f : Escrow -> Step) => bind o (fun (p : Prod Escrow Payout) => f (first p))

def payOf : Step -> Option Payout :=
  fun (o : Step) => bind o (fun (p : Prod Escrow Payout) => some (second p))

def xF : Step := escFund (sAt 10) 200u xE

def runOf : History -> Step := fun (h : History) => andThen xF (escSettle h (sAt 20) 30u)

def xS1 : Step := runOf hPush

def xC : Step := andThen xS1 (escCancel hPush (sAt 30))

def payCancel : Option Payout := payOf xC
```

`build/langc --kit check examples/skill/g2-cancel.lang` gives:

```text
ok
```

The exit code is 0.

`build/langc --kit eval examples/skill/g2-cancel.lang eStream12` gives:

```text
285u
```

The exit code is 0.

`build/langc --kit eval examples/skill/g2-cancel.lang eStream17` gives:

```text
1000u
```

The exit code is 0.

`build/langc --kit eval examples/skill/g2-cancel.lang payCancel` gives:

```text
some (fmEta Instruction (instructionOf 1u 3u 120u (stampOf 30 0u 0u 0u)))
```

The exit code is 0.

### G3

An encrypted policy vote. `pbA`, `pbB` and `pbC` are policy ballots over
the decisions 0, 1 and 2. The rows open a tally and give the choice. It is
a kit file (examples/skill/kit.txt), so each command uses `--kit`.

File: examples/skill/g3-vote.lang

```lang
-- PLAN row 531 (G3): an encrypted policy vote. pbA, pbB and pbC are policy
-- ballots over the decisions 0, 1 and 2. L7 opens a tally, L9 shows that the
-- order of the ballots has no effect, and L10 gives the policy of the choice.
def hot1 : FreeMon Verdict :=
  fmMul Verdict (fmMul Verdict (fmEta Verdict flagNo) (fmEta Verdict flagYes)) (fmEta Verdict flagNo)

def hot2 : FreeMon Verdict :=
  fmMul Verdict (fmMul Verdict (fmEta Verdict flagNo) (fmEta Verdict flagNo)) (fmEta Verdict flagYes)

def pbA : PolicyBallot := mkPolicyBallot hot1 refl 11u

def pbB : PolicyBallot := mkPolicyBallot hot1 refl 12u

def pbC : PolicyBallot := mkPolicyBallot hot2 refl 13u

def pbBox3 : PolicyBox :=
  fmMul PolicyBallot (fmMul PolicyBallot (fmEta PolicyBallot pbA) (fmEta PolicyBallot pbB)) (fmEta PolicyBallot pbC)

def l7Slot1 : (q : Quorum) -> Path (Option Count) (open (Option Count) q (tallyAt 1 pbBox3)) (countD 1 (pbPlain pbBox3)) :=
  fun (q : Quorum) => refl

def tally1 : Enc (Option Count) := tallyAt 1 pbBox3

def decs3 : FreeMon Nat := fmMul Nat (fmMul Nat (fmEta Nat 0) (fmEta Nat 1)) (fmEta Nat 2)

def q22 : Quorum := quorum (szS (szS sz0)) (szS (szS sz0))

def tBox : PolicyTally := openTally q22 decs3 pbBox3

def mkBox3 : PolicyBallot -> PolicyBallot -> PolicyBallot -> PolicyBox :=
  fun (x : PolicyBallot) (y : PolicyBallot) (z : PolicyBallot) => fmMul PolicyBallot (fmMul PolicyBallot (fmEta PolicyBallot x) (fmEta PolicyBallot y)) (fmEta PolicyBallot z)

def l9ACB : Path (FreeMon (Enc (Option Count))) (tallyAll decs3 (mkBox3 pbA pbC pbB)) (tallyAll decs3 pbBox3) :=
  refl

def boxPlur : Option Nat := chooseD 0 tBox

def boxMaj : Option Nat := chooseD 1 tBox

def l10Plur : (q : Quorum) -> (pa : Nat -> Policy) -> Path (Option Policy) (govPi pa 3 0 (openTally q decs3 pbBox3)) (some (pa 1)) :=
  fun (q : Quorum) (pa : Nat -> Policy) => refl
```

`build/langc --kit check examples/skill/g3-vote.lang` gives:

```text
ok
```

The exit code is 0.

`build/langc --kit eval examples/skill/g3-vote.lang tally1` gives:

```text
encEta (Option Nat) (some 2)
```

The exit code is 0.

`build/langc --kit eval examples/skill/g3-vote.lang boxPlur` gives:

```text
1
```

The exit code is 0.

`build/langc --kit eval examples/skill/g3-vote.lang boxMaj` gives:

```text
1
```

The exit code is 0.

### G4

The rate changes during the engagement. `hA` holds the policy `pA` at 0
and `pB` at 30, and `hC` adds `pC` at 45. The earned value at each t <= 45
stays the same, and after 45 `pC` has an effect. It is a kit file
(examples/skill/kit.txt), so each command uses `--kit`.

File: examples/skill/g4-rate.lang

```lang
-- PLAN row 532 (G4): the rate changes during the engagement. hA holds pA at
-- 0 and pB at 30, and hC adds pC at 45. L6: earned at each t <= 45 stays the
-- same, and after 45 pC has an effect.
def rA : Rate :=
  pair (fmMul Segment (fmEta Segment (segmentOf 0 20 3u 2u)) (fmEta Segment (segmentOf 20 40 1u 1u))) (fmMul Mass (fmEta Mass (massOf 0 50u)) (fmEta Mass (massOf 25 7u)))

def rB : Rate := pair (fmEta Segment (segmentOf 0 50 2u 1u)) (fmEta Mass (massOf 30 9u))

def grantOn : Grant := grantOf instrStream flagNo

def pA : Policy := policyOf rA (windowOf 0 100) grantOn (fmUnit U256)

def pB : Policy := policyOf rB (windowOf 0 100) grantOn (fmUnit U256)

def pC : Policy :=
  policyOf (pair (fmEta Segment (segmentOf 0 50 5u 1u)) (fmUnit Mass)) (windowOf 0 100) grantOn (fmUnit U256)

def eA : Engagement := engagementOf 1u 2u 3u 4u (windowOf 10 60) (some 10) none

def hA : History :=
  fmMul (Prod Time Policy) (fmEta (Prod Time Policy) (pair 0 pA)) (fmEta (Prod Time Policy) (pair 30 pB))

def hC : History := fmMul (Prod Time Policy) hA (fmEta (Prod Time Policy) (pair 45 pC))

def l6Start : Path (Option U256) (earnedAt hC eA 10) (earnedAt hA eA 10) := refl

def l6Edge : Path (Option U256) (earnedAt hC eA 30) (earnedAt hA eA 30) := refl

def l6Before : Path (Option U256) (earnedAt hC eA 44) (earnedAt hA eA 44) := refl

def l6At : Path (Option U256) (earnedAt hC eA 45) (earnedAt hA eA 45) := refl

def l6After : Path Verdict (u256Eq (option 0u (fun (x : U256) => x) (earnedAt hC eA 50)) (option 0u (fun (x : U256) => x) (earnedAt hA eA 50))) flagNo :=
  refl

def ea37 : Option U256 := earnedAt hA eA 37

def cvAt : Option U256 := earnedAt hC eA 45

def cvAfterNew : Option U256 := earnedAt hC eA 50
```

`build/langc --kit check examples/skill/g4-rate.lang` gives:

```text
ok
```

The exit code is 0.

`build/langc --kit eval examples/skill/g4-rate.lang ea37` gives:

```text
94u
```

The exit code is 0.

`build/langc --kit eval examples/skill/g4-rate.lang cvAt` gives:

```text
119u
```

The exit code is 0.

`build/langc --kit eval examples/skill/g4-rate.lang cvAfterNew` gives:

```text
144u
```

The exit code is 0.

### G5

The DAO amends `tAmend` and the rule. `sLow` lowers `tAmend` to 1, so a
yes count of 1 then enacts `pMaj` (the majority rule). `regAILong` gives
one of the two regimes. It is a kit file (examples/skill/kit.txt), so each
command uses `--kit`.

File: examples/skill/g5-amend.lang

```lang
-- PLAN row 533 (G5): the DAO amends tAmend and the rule. sLow lowers tAmend
-- to 1, so a yes count of 1 then enacts pMaj (the majority rule). L11:
-- amendAll is a fold. regAILong and regADLong give the two regimes.
def crew : FreeMon Point :=
  fmMul Point (fmMul Point (fmEta Point 1u) (fmEta Point 2u)) (fmEta Point 3u)

def s0 : Polity := makePolity crew 2 2 2 1u 2u 3 4 5u 6u 0

def pMaj : Proposal := makePolity crew 2 2 2 1u 2u 3 4 5u 6u 1

def pAmend1 : Proposal := makePolity crew 2 1 2 1u 2u 3 4 5u 6u 0

def mkT3 : Option Count -> Option Count -> Option Count -> PolicyTally :=
  fun (a : Option Count) (b : Option Count) (c : Option Count) => fmMul (Option Count) (fmMul (Option Count) (fmEta (Option Count) a) (fmEta (Option Count) b)) (fmEta (Option Count) c)

def tSplit : PolicyTally := mkT3 (some 3) (some 2) (some 2)

def sMaj : Polity := stepD s0 (pair pMaj 2)

def sLow : Polity := stepD s0 (pair pAmend1 2)

def sLowMaj : Polity := stepD sLow (pair pMaj 1)

def sLowAmend : Nat := tAmend sLow

def sLowMajRule : Nat := rulePos sLowMaj

def govSplit0 : Option Nat := govC s0 3 tSplit

def govSplitMaj : Option Nat := govC sMaj 3 tSplit

def l11Amend : (ps : FreeMon Decided) -> (qs : FreeMon Decided) -> (s : Polity) -> Path Polity (amendAll (fmMul Decided ps qs) s) (amendAll qs (amendAll ps s)) :=
  fun (ps : FreeMon Decided) (qs : FreeMon Decided) (s : Polity) => refl

def tZero : PolicyTally := mkT3 (some 0) (some 0) (some 0)

def tLong : PolicyTally := fmMul (Option Count) tZero (fmEta (Option Count) (some 3))

def regAILong : RegimeD := regimeD 3 0 (fmEta PolicyTally tLong)

def regADLong : RegimeD := regimeD 4 0 (fmEta PolicyTally tLong)
```

`build/langc --kit check examples/skill/g5-amend.lang` gives:

```text
ok
```

The exit code is 0.

`build/langc --kit eval examples/skill/g5-amend.lang sLowAmend` gives:

```text
1
```

The exit code is 0.

`build/langc --kit eval examples/skill/g5-amend.lang govSplitMaj` gives:

```text
none
```

The exit code is 0.

`build/langc --kit eval examples/skill/g5-amend.lang regAILong` gives:

```text
regimeAI
```

The exit code is 0.
