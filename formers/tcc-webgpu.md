# TinyCC WebGPU realization

Paths below are relative to this host kit, or to the root of a generated
language. The template's `formers/FORMERS.md` gives the host-neutral laws.
The WGSL target uses the rows of `formers/tcc-wasm.md`, as
`formers/tcc-js.md` does: `src/front/check.c` lines are rows of the
builtin table, and `src/front/lower.c` lowers a program to the IR of
`src/ir.h`. `src/wgsl.c` writes that IR as a WGSL compute module (`langc
build PROG --wgsl OUT.wgsl`, S10, Q-S10-1 (a); `wgsl_write` at
`src/wgsl.c:1123`, called from `src/main.c:227`). Each module gets the
runtime of `RUNTIME` (`src/wgsl.c:81`). A module that calls a u256, hash
or curve operation also gets `LIMB_PART`, `SHA_PART` or `EC_PART`
(`src/wgsl.c:199`, `src/wgsl.c:323`, `src/wgsl.c:406`), from the need
mask of its IR call nodes (`src/wgsl.c:1137`).

DONE means: `langc check` accepts the former, `langc eval` computes it,
and `langc build --wgsl` writes it to a WGSL module when its value is
first-order at run time. Types and universes erase in the module. Proof
contents erase. The writer has no trap form (rule N4 in `docs/PLAN.md`,
S10 P2). A divisor of 0 reads as 1, and the status u32 of each out row
is always 0.

`make gpu-check` runs `sh test/gate.sh gpu` outside the sandbox, because
deno needs a GPU adapter. `make check` does not run it (Q-S10-2 (a)). It
writes the WGSL module of each lowered example (`wgsl: 59 modules
written`) and runs the rows of each lowered example on its module with
`build/wgsltool` (`test/wgsltool.c`, deno with WebGPU; `wgsl
differential`). Each row must print the same output as its Wasm row. It
also runs the 32 `test/ir` rows of `comb`, `lazy`, `spine`, `proof-trap`
and `memname` (`test/ir wgsl`). The output of the module of each
`test/wasm` file (`hash`, `secp`, `traps`, `edge`) must be the RESULT
text of its `.txt` file (`wgsl regressions`). `build/notraptool` checks
the 63 WGSL modules that the gpu gate writes (`notrap wgsl`). The rows of
`test/wgsl/batch.txt`, `zk.txt` and `earned.txt` run on the modules of
their `.lang` files (`wgsl proofs`, `wgsl chain`, `wgsl zk`, `wgsl
earned`), and `wgsl batch` runs one `test/wasm` row 192 and 1024 times.

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
`build --wgsl` refuses each program that `build` refuses. As `build --js`
does, it also refuses a run-time `Secret` or a zk secret op
(`REFUSE_TARGET_SECRET`). The F4, F6, F9 and F15 notes of
`formers/tcc-wasm.md` apply to the WGSL module too.
The Metal backend does not compile 6 entries in 2 modules
(`test/wgsl/internal.txt`; `docs/PLAN.md` O12). No Evidence file of the
F rows is in that list.
