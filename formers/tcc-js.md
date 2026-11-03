# TinyCC JS realization

Paths below are relative to this host kit, or to the root of a generated
language. The template's `formers/FORMERS.md` gives the host-neutral laws.
The JS target uses the rows of `formers/tcc-wasm.md`: `src/front/check.c`
lines are rows of the builtin table, and `src/front/lower.c` lowers a
program to the IR of `src/ir.h`. `src/js.c` writes that IR as an ES module
with a BigInt prelude (`langc build PROG --js OUT.mjs`, S9). An entry with
an `Option` result returns `null` for none, else the payload as a BigInt
(Q-S9-1 (a)).

DONE means: `langc check` accepts the former, `langc eval` computes it,
and `langc build --js` writes it to an ES module when its value is
first-order at run time. Types and universes erase in the module. Proof
contents erase.

`test/gate.sh` runs the rows of each lowered example on its ES module
with `build/jstool` (`test/jstool.c`, node, Q-S9-2 (a); `js
differential`). Each row must print the same output as its Wasm row. It
also runs the 32 `test/ir` rows of `comb`, `lazy`, `spine`, `proof-trap`
and `memname` (`test/ir js`). `build/notraptool --js` checks the 33 ES
modules that the gate builds for trap forms (`notrap js`, Q-S9-3 (a),
D15). The Makefile runs the `test/wasm` rows of `hash`, `secp`, `traps`
and `edge` on the ES module, and each output must be the same as the Wasm
output (`js regressions`).

| ID | Former | Status |
|---|---|---|
| F1 | Product | DONE |
| F2 | Coproduct | DONE |
| F3 | Option | DONE |
| F4 | FreeMon | DONE. HOST-LIMIT: a run-time tree |
| F5 | Monad pure/map/bind | DONE over Option, Sum E and List |
| F6 | Algebra fold | DONE over Nat, List and domain families. HOST-LIMIT: a run-time tree |
| F7 | Algebra unfold | DONE into List with a Nat step limit |
| F8 | Filter | DONE over Option and List |
| F9 | Non-dependent Pi | DONE. HOST-LIMIT: a function value at run time |
| F10 | Dependent Pi | DONE over `Type 0` and over Nat |
| F11 | Sigma | DONE |
| F12 | Path refl/sym/trans | DONE |
| F13 | Path transp/cong | DONE |
| F14 | Universes | DONE for Type 0 and Type 1, without cumulativity |
| F15 | Generic indexed family | PARTIAL: only the domain file declares an indexed family |

The Where and Evidence columns of each row are in `formers/tcc-wasm.md`.
Each target reads the same IR from `src/front/lower.c`.

The HOST-LIMIT refusals (`REFUSE_RUNTIME_TREE`, `REFUSE_RUNTIME_CLOSURE`)
come from `src/front/lower.c`, before a target writes the IR. Thus
`build --js` refuses each program that `build` refuses. JS is not
constant time, so `build --js` also refuses a run-time `Secret` or a zk
secret op (`REFUSE_TARGET_SECRET`, Q-S9-4 (a), O4). The F4, F6, F9 and
F15 notes of `formers/tcc-wasm.md` apply to the ES module too.
