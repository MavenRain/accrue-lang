# Host capability probe: tcc-wasm

Date: 2026-11-05. Host tree: `/Users/oobi/Documents/accrue-lang` at
d3ddc7b (tree 4d78acbf), read only: `git archive` copies the tree into a
new temporary directory (probe.sh:17; probe.log:5, :10). Driver:
`build/langc`, built on 2026-11-05 by `make build/langc` in that copy
(probe.log:11-16), with tcc 0.9.28rc 2026-09-04 mob@0fb54300 (AArch64
Darwin), wasmtime 48.0.1, wasm-validate 1.0.39 and node v23.10.0
(probe.log:6-9, probe2.log:6-9). Other work loaded the machine (load
averages 15.75 to 18.30, probe.log:4, probe2.log:4). Thus each wall time
is an upper bound. A cite `probe.log:N` or `probe2.log:N` is a line of a
run log in `accrue-lang-work/s16/probe-logs/`, and a cite `probe.sh:N` is
a line of the probe script in `accrue-lang-work/s16/`. Both directories
are outside the repo.

The probe ran `zsh -f probe.sh > probe-logs/probe.log 2>&1`, then `zsh -f
probe2.sh > probe-logs/probe2.log 2>&1`, in `accrue-lang-work/s16/`. Each
log ends with the line `EXIT 0` (probe.log:343, probe2.log:48). Each step
is one `step` line of a script; to run some steps, keep the setup lines
and those `step` lines. probe.sh runs the base span, the kit steps with
`--kit` and the cost rows on a `Size` chain. probe2.sh runs
`test/check/self.lang` and `test/check/later.lang` again with the flag
that `test/check/kit.txt` gives them, as the gate does (probe2.log:34).
Each step runs below `probe/guard.py`, which stops a command above 4096 MB
resident memory or above its time limit, 120 s (probe.sh:11). Each guard
verdict is `exit`: no step reached a limit. The probe writes only below a
new temporary directory. The largest process in the probe used 429.6 MB
(probe.log:339, the build of a chain of 1000 `Size` definitions joined to
`examples/dao.lang`). The scripts, generated inputs and logs are local
artifacts, not files shipped with this repository. They are provenance
for this report, not a repository-local replay command.

To recheck the repository fixtures from the repository root, run:

```sh
make check
python3 probe/guard.py --rss-mb 4096 --timeout 120 -- build/langc check examples/dao.lang
python3 probe/guard.py --rss-mb 4096 --timeout 120 -- build/langc --kit check examples/sigma.lang
python3 probe/guard.py --rss-mb 4096 --timeout 120 -- build/langc --kit check test/check/self.lang
python3 probe/guard.py --rss-mb 4096 --timeout 120 -- build/langc --kit check test/check/later.lang
```

The last two commands intentionally exit 1 with `REFUSE_REC`
(probe2.log:35-42); the first three exit 0 (probe.log:34-41). The guard
uses macOS libproc. These commands cover the checked-in fixtures, not the
custom probe inputs or the historical cost measurements in section 6.

The base span: `langc check` refuses a program with a `Nat` or another kit
item. The line is REFUSE_KIT `NAME: Nat is a kit item outside the base
span (PLAN section 4); only the kit mode langc --kit admits it`
(probe.log:146, :153, :167; `langc explain REFUSE_KIT`, probe2.log:44).
The gate runs the kit-type programs with `--kit` when `kit.txt` lists them
(`test/gate.sh:15`, `:68`), and so do the kit steps of probe.sh and
probe2.sh. `examples/dao.lang` checks `ok` in the base span
(probe.log:38-39), and so does a chain of `Size` definitions
(probe.log:318-319, :330-331).

## Questions

Each host must answer these questions. Each answer is one numbered section
below. An answer sets the status of a former in `formers/tcc-wasm.md`.

| Question | Section | Formers |
|---|---|---|
| Which commands check a file, print a normal form, build and run? | 1 | all |
| Does the host take several files, or must the driver join them? | 2 | all |
| Which recursion does the host accept, and how does it refuse the rest? | 3 | F6, F7 |
| Which `Nat` primitives exist? Does `Nat` have an eliminator? | 4 | F6, F7 |
| How does a result leave the host? | 5 | target |
| What do check and build cost (time and resident memory) at N = 100 and N = 1000? | 6 | all |
| Can a family take a parameter? Can a constructor of such a family appear in a term? | 7 | F4, F12, F15 |
| Can a program project a Sigma? Does the kernel accept Sigma eta? | 8 | F11 |
| How does a program use equality: transport, cong, an index type? | 9 | F12, F13 |
| How does the host report an axiom? | 10 | R2 |

## 1. Commands

| Job | Command | Result |
| --- | --- | --- |
| Check | `build/langc [--kit] check FILE` | `ok` and exit 0 for `examples/dao.lang` (probe.log:38-41), or one line `langc: CODE: NAME: TEXT` and exit 1 (probe.log:141-143). `check --json` prints one JSON object (section 10) |
| Print normal form | `build/langc [--kit] eval FILE NAME [ARGS...]` | The normal form of NAME: `5` for `addPrice 2 3` and `none` for `addPrice 18446744073709551615 1` in `examples/entries.lang` (probe.log:42-51). Exit 0 for both (section 4) |
| Build | `build/langc [--kit] build FILE -o OUT`, or `--js OUT` or `--wgsl OUT` in place of `-o OUT` | One file. For `examples/dao.lang`: a wasm module of 63 bytes that `wasm-validate` accepts, a JavaScript module of 1815 bytes, or a WGSL file of 3882 bytes (probe.log:62-76) |
| Run | `build/langc [--kit] run FILE SCRIPT` | Each call of SCRIPT and its result: `examples/lists.lang` with `test/run/lists.script` gives the same text as `test/run/lists.out` (probe.log:97-100) |

- `langc` with no command prints the usage of 10 commands and exits 2:
  `check [--json]`, `eval`, `run`, `ir`, `abi [--text]`, `build` (with
  `--runtime`), `prove PROG NAME [POLL]`, `fmt [--check]`, `explain CODE`
  and `mcp` (probe.log:17-33, probe2.log:17-33). The usage gives `--kit`
  as the kit mode, the trusted test scope of the kit gate (probe.log:31).
- One file for each command: `check` with two files gives USAGE `too many
  arguments` and exit 2 (probe.log:122-138, section 2).
- One output for each build: `-o` with `--js` gives USAGE `build takes -o
  OUT or --js OUT.mjs, not both` and exit 2 (probe.log:77-93).
- `abi examples/dao.lang --text` prints no line, and the step exits 0 in
  1.839 s at 423.2 MB (probe.log:94-96). The step sends the output
  through `head -n 6` (probe.sh:39). `explain CODE` prints the text of a
  code (probe.log:308, :312; probe2.log:44). `fmt examples/sigma.lang
  --check` prints nothing and exits 0 (probe.log:315-317).
- `make build/wasmtool` builds the host tool `build/wasmtool`. With no
  argument it prints its usage and exits 2 (probe.log:101-109).
- `PIN` gives the tool versions of the last green host gate (tcc
  0.9.28rc, node v23.10.0, wasm-validate 1.0.39; `PIN:1`, :3, :4), and the
  probe tools are the same (probe.log:6, :8, :9). `test/gate.sh` and the
  Makefile do not read `PIN` or the probe.
- Not probed: `ir`, `prove`, `mcp`, `build --runtime`, and a run of a built
  module in wasmtime or node.

## 2. Several files

No. A program is one file, with no import form. A file that starts with
`import domain` (probe.sh:45-46) gives PARSE_EXPECT `expected 'def',
'axiom', 'family', 'cofamily', 'state', 'event', 'sealed' or 'law', found
a name` and exit 1 (probe.log:118-121). `langc check` with two files gives
USAGE `too many arguments` and exit 2 (probe.log:122-138). A program uses
the domain names with no import: a chain of `Size` definitions (`def d1 :
Size := szS d0`, probe.sh:93) checks `ok` (probe.log:318-319). Thus a
driver must join the parts of a program into one file. The cost rows do
this: the probe joins the `Size` chain and `examples/dao.lang` into one
file (probe.sh:96), and the build of that file exits 0 (probe.log:326-328,
:338-340; section 6).

## 3. Recursion and totality

No recursive definition checks. `def rec` (`test/check/rec.lang`) gives
REFUSE_REC `loop: def rec is refused (rule R3); use fold or unfold` and
exit 1 (probe.log:139-143). With `--kit`, a call of the definition itself
(`test/check/self.lang`) or of a later name (`test/check/later.lang`)
gives REFUSE_REC and exit 1: `spin: spin is this definition or a later
one; a definition can use only earlier ones (rule R3)` (probe2.log:35-38),
and the same line for `late` in `early` (probe2.log:39-42). In the base
span these two files stop first at REFUSE_KIT (probe.log:144-148,
:149-155; the base-span paragraph above). `langc explain REFUSE_REC` names
`def rec`, or a definition that refers to itself or to a later definition
(probe.log:311-312). Recursion is `fold` and `unfold`, as the REFUSE_REC
line says. F6 (algebra fold) is DONE over Nat, List and the domain
families, and F7 (algebra unfold) is DONE into List with a Nat step limit
(`formers/tcc-wasm.md:19-20`). The probe ran no `fold` or `unfold` of its
own.

## 4. Nat

The primitives are `natAdd` and `natSub`, which give an `Option Nat`
(`src/front/check.c:200-201`), `natDiv` and `natMod`, which take a
`NonZero Nat` divisor and give a `Nat` (:136, :216-217), the comparisons
`natEq` and `natLe`, which give a `Flag` (:202-203), and `natIsSet`
(:238). `natMul` gives NAME_UNKNOWN and exit 1 (probe.log:194-197).
`natRec` gives NAME_UNKNOWN too (probe.log:202-205): `Nat` has no
eliminator primitive, and the eliminator is the algebra `fold` (F6,
section 3). The base span refuses `Nat`: `test/check/kit-type-nat.lang`
gives REFUSE_KIT and exit 1 (probe.sh:50; probe.log:166-169). Thus the
steps below run with `--kit`.

A program cannot use the `Option` result of a primitive as a `Nat`. The
probe file `p-nat.lang` (probe.sh:51-57) uses `natSub 4 9` as a `Nat`.
With `--kit`, its check gives REFUSE_PARTIAL `under: natSub gives an
Option, not a value of type Nat: take it apart with option or bind` and
exit 1 (probe.log:170-173). The check stops at the first definition,
`under`. Thus each of the five eval steps of that file gives the same line
and exit 1 (probe.log:174-193), and the probe did not reach `top`, `over`,
`divz` or `modz`. The type of `natDiv` and of `natMod` is `Nat -> NonZero
Nat -> Nat` (`test/check/base-natdiv.lang:1`, `base-natmod.lang:1`). In
the base span these two files give REFUSE_KIT
(`test/check/expect.txt:109`, :110).

An overflow and a division by 0 give a value, not a trap. `addPrice` is
`natAdd a b` (`examples/entries.lang:2`), and `ratio` maps `natDiv a d`
over `nonZero k` (:4-5). `addPrice 2 3` is 5 and `addPrice
18446744073709551615 1` is `none` (probe.log:42-51). `ratio 7 2` is 3 and
`ratio 7 0` is `none` (probe.log:52-61). Each of these evals exits 0. The
largest value is 18446744073709551615, that is 2^64 - 1: a literal above
it gives LEX_NAT_RANGE `a Nat literal is larger than 2^64 - 1`
(probe.log:198-201). A definition cannot recurse on a `Nat` (section 3).
The base span counts with the domain family `Size`
(`domain/domain.lang:57`, probe.log:269).

## 5. How a result leaves the host

As printed text or as one file of a target. `eval` prints the normal form
(probe.log:43, :53), and `run` prints each call of a script
(probe.log:97-100). `build` writes one file: a wasm module with `-o`, a
JavaScript module with `--js`, or a WGSL file with `--wgsl`
(probe.log:62-76). One build gives one target (probe.log:78).
`wasm-validate` accepts the wasm module of `examples/dao.lang`
(probe.log:66-68). `check --json` prints the diagnostics as one JSON
object (probe.log:299-302). `abi --text` printed no line for
`examples/dao.lang` (section 1). The probe did not run a built module
(section 1).

## 6. Cost

Each file has N chained `Size` definitions (`def dI : Size := szS dJ`,
probe.sh:93). All three rows run in the base span. Check runs `langc check`
on the chain. Build joins the chain and `examples/dao.lang` into one file
and runs `langc build FILE -o OUT`. Run is `langc eval` of the last
definition `dN`, because the probe did not run a built module (section 1).
Each cell gives the wall time and the peak resident memory, the larger of
the two `probe/guard.py` peaks (probe.log:318-341).

| Step | N = 100 | N = 1000 |
| --- | --- | --- |
| Check (`langc check FILE`) | 0.027 s, 25.7 MB | 0.075 s, 30.2 MB |
| Build (the chain joined to `examples/dao.lang`, `langc build FILE -o OUT`) | 1.286 s, 423.9 MB | 1.961 s, 429.6 MB |
| Run (`langc eval FILE dN`) | 0.053 s, 29.8 MB | 0.078 s, 34.1 MB |

- The eval output is 602 and 6002 bytes by `wc -c` (probe.log:323, :335).
  The Run row runs in `zsh -c "... | wc -c"` (probe.sh:95), so its peak
  includes the shell and `wc`.
- `dao100.wasm` and `dao1000.wasm` are 63 bytes, the same as the module
  of `examples/dao.lang` alone (probe.log:65, :329, :341). The build does
  not lower the unused chain definitions.
- `examples/dao.lang` alone builds in 1.261 s at 423.4 MB
  (probe.log:62-63), and its check costs about the same, 1.425 s at
  423.5 MB (probe.log:38-40). Thus almost all of the build cost is fixed.
  At N = 1000 the chain adds about 0.7 s and 6.2 MB. Each of these times
  is an upper bound (the load averages above).

## 7. Parametric families

A program cannot declare a family: `test/check/data.lang` gives REFUSE_DATA
and exit 1 (probe.log:156-160), and a program with a family `Box`
(probe.sh:78) gives REFUSE_DATA `Box: a program cannot declare a family
(rule R1); only domain/domain.lang can` (probe.log:230-233). In the domain
file, `Trunc1` (`domain/domain.lang:43`), `Trunc0` (:49) and `Bag` (:101)
take a type parameter, `Stream` (:64) and `Books` (:731) are cofamilies,
and `Pos`, `Word`, `ChoiceBag` and `OrbitN` take a `Size` index, with the
type `Size -> Type 0` (:919-954; probe.log:262-298). The domain file also
declares the accrue families, for example `Stamp`, `Instruction`, `Escrow`
and `PayAuth` (probe.log:278, :286, :288, :291). The constructor `trunc1`
takes its type as an explicit argument: with the type left out
(probe.sh:72) it gives TYPE_ARITY `trunc1 needs 2 arguments, found 1`
(probe.log:218-221), and with the type (probe.sh:74) it checks `ok`
(probe.log:222-225). A constructor of an index family appears in a term:
`def p : Pos (szS sz0) := posFirst sz0` (probe.sh:76) checks `ok`
(probe.log:226-229). `examples/trunc.lang` and `examples/quot.lang` check
`ok` (probe.log:242-249). `FreeMon` (`domain/domain.lang:113`,
probe.log:274) is F4, DONE with the HOST-LIMIT a run-time tree
(`formers/tcc-wasm.md:17`). Thus F15 stays PARTIAL: only the domain file
declares an indexed family (`formers/tcc-wasm.md:28`). Probe files: the
step files of probe.sh:72-79 and `examples/trunc.lang`.

## 8. Sigma projection and eta

The projections check: the projection step (probe.sh:70-71) checks `ok` in
the base span (probe.log:214-217), and `examples/sigma.lang` checks `ok`
with `--kit` (probe.log:34-37). A Sigma pattern does not parse: a `fun`
with a Sigma pattern (probe.sh:68) gives PARSE_EXPECT `expected ':', found
')'` (probe.log:210-213). The kernel accepts Sigma eta: the eta step
(probe.sh:66) checks `ok` with `--kit` (probe.log:206-209). F11 (Sigma) is
DONE (`formers/tcc-wasm.md:24`).

## 9. Equality

A program writes equality as `Path A x y`, with `refl` and `cong` (F12 and
F13, both DONE, `formers/tcc-wasm.md:25-26`). `transport`, `J` and `trans`
are definitions in `domain/domain.lang:7`, :13 and :21 (probe.log:263-265).
A program that uses `transport` (probe.sh:80) checks `ok`
(probe.log:234-237), and so does a program that uses `trans` (probe.sh:82;
probe.log:238-241). `S1` (`domain/domain.lang:38`) is a higher inductive
family with the path constructor `loop (i : I)` (probe.log:266).
`examples/paths.lang`, `examples/equality.lang` and `examples/circle.lang`
check `ok` (probe.log:250-261); the circle check takes 0.127 s at 199.2 MB
(probe.log:260). `trunc1` takes its type as an argument (section 7), so
one family serves each index type.

## 10. Axiom detection

No command lists the axioms of a file (the usage, probe.log:18-31).
`langc check FILE` prints only `ok` for a file with no axiom
(probe.log:38-39). `langc check --json FILE` prints one JSON object. For a
one-definition axiom-free probe input (probe.sh:86), it prints
`{"diagnostics":[],"progress":{"parsed":1,"typed":1,"laws_discharged":0,"laws_total":0}}`
and exits 0 (probe.log:299-302). For `test/check/axiom.lang`, it prints one
diagnostic REFUSE_AXIOM `magic: an axiom has no proof (rule R2)` with the
span 1:1 to 1:18, and exits 1 (probe.log:303-306). A plain `check` of that
file gives REFUSE_AXIOM and exit 1 (probe.log:161-165). `langc explain
REFUSE_AXIOM` prints the text of the code (probe.log:307-308). Thus a
program with an axiom does not check, and R2 holds at the check.

## Consequences for M0

1. A program is one file. The domain names need no import (section 2).
2. No `def rec`, and no call of the definition itself or of a later one:
   recursion is `fold` and `unfold` (section 3, R3).
3. A program with a kit type (`Nat`) runs with `langc --kit`, as the gate
   does when `test/check/kit.txt` or `examples/kit.txt` lists it. The base
   span refuses it with REFUSE_KIT (the base-span paragraph, section 4).
4. A `Nat` is at most 2^64 - 1. `natAdd` and `natSub` give an `Option
   Nat`, and `natDiv` and `natMod` take a `NonZero Nat` divisor. A program
   that uses an `Option` result as a `Nat` does not check (REFUSE_PARTIAL).
   An overflow and a division by 0 give `none` and exit 0, with no trap.
   Thus a driver reads `none` as a value (section 4).
5. One build writes one target: `-o` (wasm), `--js` or `--wgsl` (sections
   1 and 5).
6. A program cannot declare a family, so F15 stays PARTIAL (section 7, R1).
7. A build of `examples/dao.lang` costs 1.3 s to 2.0 s and 423 MB to
   430 MB, and almost all of it is fixed. The unused definitions add no
   byte to the module (section 6).
8. An axiom stops the check with REFUSE_AXIOM and exit 1 (section 10, R2).
