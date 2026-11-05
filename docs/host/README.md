# TinyCC Wasm host kit

This kit compiles a small typed language to a WebAssembly MVP module.
One C99 program, `build/langc`, holds the parser, the checker, the
evaluator, the lowering to a first-order IR and the Wasm writer.
TinyCC builds it.

## Shared files

This kit and `hosts/tcc-evm` share these files. `TCC_SHARED` in the root
`Makefile` lists them:

- the source: the front end (`src/front/`), `src/ir.h`, `src/target.h`
  and `src/main.c`
- the domain and its embed tool: `domain/` and `gen/`
- the examples: `examples/`
- the shared tests: `test/parse/`, `test/check/`, `test/eval/`, `test/ir/`,
  `test/grid.awk`, `test/review.sh` and `test/fronttool.c`
- `.gitignore`

`make check` at the template root compares each shared entry in the two
kits with `diff -r`. It fails when a shared file is different in the two
kits or is missing from one kit. When you change a shared file, make the
same change in `hosts/tcc-evm`.

These files are specific to this kit:

- the target files: `src/wasm.c`, `src/js.c`, `src/json.c` and `src/wgsl.c`
- the target test files: `test/wasmtool.c`, `test/jstool.c`, `test/wgsltool.c` and `test/notraptool.c`
- the build and gate files: `Makefile`, `test/gate.sh` and `PIN`
- the docs: `README.md`, `FORMERS.md` and `docs/CAPABILITY.md`

## Build

From the template root:

```sh
make -C hosts/tcc-wasm build
make -C hosts/tcc-wasm check
make -C hosts/tcc-wasm check-clang
```

`build` runs `tcc -run gen/embed.c` to copy `domain/domain.lang` into
`build/domain.c`. Then it builds `build/langc` with
`tcc -std=c99 -Wall -Werror`. The domain is part of the executable.
`check-clang` compiles the same sources with
`cc -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only`.
`check` does both steps, then runs `test/gate.sh`.
The gate needs `node`, `wasm-validate` and `wasm-objdump` on the `PATH`.
`PIN` records the tool versions of the last GREEN gate.
The build and the gate need no network access.

## Source language

A program is a sequence of `def NAME : TYPE := TERM`. A line comment
starts with `--`. Lambdas are `fun (x : A) => t`. There is no text type.

Core names: types `Nat Flag Prod Sum Option List Eq`, values
`flagYes flagNo pair first second inl inr either none some option nil cons
pure map bind fold unfold filter pack witness payload refl symm trans
transport cong`, Nat operations `natAdd natSub natMul natEq natLe`, and the
Flag eliminator `flagIf b yes no`. `Type 0 : Type 1` is the universe
chain. `Sigma (x : A) B` and `(x : A) * B` are dependent pairs.
`FORMERS.md` gives the status of each former.

The compiler enforces these rules:

| Rule | Code | The compiler refuses |
|---|---|---|
| R0 | `REFUSE_PARTIAL` | a partial name (for example `div`, `mod` or `sub`), the `Option` result of a checked kit word where a plain value is wanted, or the literal 0 where a `NonZero` is wanted |
| R1 | `REFUSE_DATA` | a `family` declaration in a program |
| R2 | `REFUSE_AXIOM` | an `axiom` declaration |
| R3 | `REFUSE_REC` | `def rec`, or a definition that refers to itself or to a later definition |
| Time | `REFUSE_CLOCK` | a clock word (`now`, `block`, `timestamp`, `clock`) as a name or a use; the message says to take the time as a `Stamp` argument (PLAN section 5) |
| R4 | `REFUSE_NAME` | a core name, a domain name, a reserved name or a repeated name as a new definition |
| R1, O7 | `REFUSE_CELL` | a type constructor that is not one cell signature or one cocell signature, a cons-list name in place of a cell monad, or a sealed family that is not a cell monad |
| - | `REFUSE_EVENT` | an event with more fields than the entry parameter limit, an event field that is not an ABI word type (`Nat`, `Flag`, `U256`, `Addr`), or a repeated event field |
| D-9 | `REFUSE_HISTORY_WRITE` | a history field `h` that is not `(h t)`, `ADD (h t) e` or the variable of `bind (ADD (h t) e)`, with `ADD` as `u256Add` or `natAdd`, a history constructor without all of its arguments, or a definition with `State` in its type that refers to a name that the history rule does not check |
| - | `REFUSE_KIT` | a kit item outside the base span (PLAN section 4); only the kit mode `langc --kit` admits it |
| O7 | `REFUSE_LAW` | a law-only name (`decrypt`, `observe`, an elim, a fold or a projection of a sealed family) in a definition whose type is not a law (a `Path` or `Trunc1` codomain) |
| - | `REFUSE_MAP` | a `Map` key type that is not `Nat`, `U256` or `Addr`, or a `Map` value type that is not `Nat`, `Flag`, `U256` or `Addr` |
| - | `REFUSE_SIG` | a name of the design signature (PLAN section 3): a slot with the wrong type, a reserved name that no slice gives yet, or a defined or primitive name that the kernel gives |
| - | `REFUSE_STATE` | a second state, a state type with a name other than `State`, a state with parameters, an index telescope or not one constructor, a state field that is not first-order, or a history field that is not `Nat` or `U256` |

## CLI

```sh
build/langc check PROG
build/langc eval PROG NAME [ARGS...]
build/langc ir PROG
build/langc abi PROG [--text]
build/langc build PROG -o OUT [--runtime]
build/langc build PROG --js OUT.mjs
build/langc build PROG --wgsl OUT.wgsl
build/langc check --json PROG
build/langc fmt PROG [--check]
build/langc explain CODE
build/langc mcp
```

- `check` prints `ok`, or refuses the program.
- `eval` prints the normal form of `NAME`. Each argument is a decimal
  number. For an Option result, `eval` prints the payload of `some`, or
  `none` (S3, Q-S3-9).
- `ir` prints the first-order IR. It has one `func` for each entry.
- `abi` prints the JSON ABI of the entries (`src/json.c`, Q-S9-6 (a)):
  `{"entries":[{"name":N,"params":[K,...],"result":R}]}`. A kind K is
  `nat`, `flag` or `u256`, and R is K or `option K` (Q-S9-7 (a)).
  `--text` prints the text form, one line for each entry.
- `build` writes the Wasm module to `OUT`. On this target, `--runtime`
  gives the same bytes. `--js OUT.mjs` writes an ES module with a BigInt
  prelude instead (`src/js.c`, S9). On ES, a run-time `Secret` or a zk
  secret op gives `REFUSE_TARGET_SECRET` (Q-S9-4 (a)).
- `build PROG --wgsl OUT.wgsl` writes a WGSL compute module instead
  (`src/wgsl.c`, S10). Each entry is a compute entry with the bindings
  `NAME_in` (0), `NAME_out` (1) and `heap` (2). Invocation idx reads row
  idx of `NAME_in` and writes row idx of `NAME_out`: a status u32 (always
  0), then the result. A `Nat` or `Flag` is 2 u32 and a `U256` is 8 u32,
  low first. An `Option` result has one more u32 after the status: 0 for
  none, 1 for some. `--wgsl` takes the Wasm heap limit
  (`REFUSE_CAPACITY`). As `--js` does, it refuses a run-time `Secret` or
  a zk secret op (`REFUSE_TARGET_SECRET`). `build` takes one of `-o OUT`,
  `--js OUT.mjs` and `--wgsl OUT.wgsl`.
- `check --json` prints one JSON object with `diagnostics` and
  `progress` (`docs/CHECK-JSON.md`, S12).
- `fmt` prints the canonical form of `PROG` (S11). With `--check`, a
  program that is not canonical gives exit 1.
- `explain CODE` prints the text of a refusal code in the table above
  (`src/front/codes.c`, Q-S13-4 (a)).
- `mcp` runs the MCP server on stdin and stdout (`docs/MCP.md`, S13).

The exit status is 0 for success, 1 for a refused program and 2 for a
usage or IO error. A diagnostic on stderr has the form
`langc: CODE: DEF: message`.

## Entries

An entry is a top-level program definition whose declared type is `Nat`,
`Flag`, or a first-order function from `Nat` and `Flag` arguments to a
`Nat` or `Flag` result. `build` exports each entry by its name. Each
parameter and the result is an `i64`. Other definitions are not exported.

`Nat` is an unsigned 64-bit integer. `natAdd` gives `none` on an
overflow past 2^64 - 1, and `natSub` gives `none` when the second
argument is larger than the first. `Flag` is 0 or 1.

## Compilation

Compilation is partial evaluation. The compiler normalizes the body of
each entry with the parameters as free variables. Types and universes erase.
Proof contents erase. A lambda stays only as the direct function argument of
`fold`, `unfold`, `map`, `bind`, `filter`, `either` or `option`. The
lowering puts its body into a loop or a branch. The lowering refuses
programs with four HOST-LIMIT codes (see `docs/CAPABILITY.md`) and one
O4 code:

- `REFUSE_RUNTIME_CLOSURE`: a function value must exist at run time.
- `REFUSE_RUNTIME_TREE`: a fold walks a tree that exists only at run time.
- `REFUSE_CAPACITY`: a list walk, an unfold limit or the heap of a loop has
  no static bound (S8 P3).
- `REFUSE_LOWER`: a construct does not lower.
- `REFUSE_TARGET_SECRET`: on the ES or WGSL target, the operation touches a
  run-time `Secret` or a zk secret op (O4).

## No trap form

A partial operation gives an `Option`: `none` where it has no result.
`langc eval` prints `none`, and a `langc run` step reverts on `none`
(Q-S3-9). The Wasm writer (`src/wasm.c`, `src/front/lower.c`,
`src/ir.h`) is out of the build until S8, and S8 replaces it. Rule N4
in `docs/PLAN.md`: a writer emits no trap form. S8 rules how an entry
with an `Option` result gives `none` to the host (Q-S3-18).

## Wasm layout

The S8 writer sets the Wasm layout (Q-S3-18).

## Domain slot

`domain/domain.lang` holds the families and the operations of the domain.
Only this file can declare a `family` (rule R1). `domain/README.md` tells
what the domain holds and how to change it.

## Gate

`make -C hosts/tcc-wasm check` prints these lines when it is GREEN:

| Line | What the gate does |
|---|---|
| `examples: 10 checked, 0 failed` | `langc check` accepts each `examples/*.lang` file |
| `parse refusals: 9 checked` | each `test/parse` file fails with the code in `test/parse/expect.txt`; one file has 1100 nested parentheses |
| `check refusals: 19 checked` | each `test/check` mutant fails with the code in `test/check/expect.txt`; the `TYPE_REFL` message shows both normal forms |
| `evals: 32 checked` | `langc eval` gives each result in `test/eval/expect.txt` |
| `skill checks: 5 of 5` | for each `examples/skill/X.lang`, `langc check` (`langc --kit check` when `X.lang` is a line of `examples/skill/kit.txt`) gives `ok`, and `langc fmt examples/skill/X.lang --check` exits 0 with no output |
| `skill quotes: 5 of 5` | `build/skilltool` finds that the program block of each example G1 to G5 of `skills/accrue-lang/SKILL.md` is byte equal to the file that its `File:` line names |
| `skill outputs: 20 of 20` | `build/skilltool` runs each `langc` row of the 5 examples (`CMD 2>&1`): the output equals the text block line for line, and the exit code is the number of its `The exit code is N.` sentence |
| `skill negatives: 3 of 3` | `build/skilltool test/skill/X.md 1` exits 1 with the FAIL line of the change on each of 3 fixtures: the G1 section with a changed program byte, with no text block and with a wrong exit code |
| `gate: 0 failures` | the sum of all failures |

The `ir`, `wasm` and `differential` lines come with the S8 writer.
`make gpu-check` runs `sh test/gate.sh gpu` outside the sandbox, because
deno needs a GPU adapter. `make check` does not run it (Q-S10-2 (a)). It
prints the `wgsl` lines, for example
`wgsl differential: 166 rows, 0 mismatches, 0 files with no WGSL module`
and `wgsl known internal: 2 modules, 6 entries`.

The gate also checks the usage and IO exit codes, five `eval` refusals,
the eval cases of `test/ir/spine.lang` in `Option` form (`eval_spines`
in `test/runtool.c`), and the absence of em-dashes and en-dashes
in the kit. `test/ir` stays for S8 (Q-S3-19). The gate does not run its
files. `perl bin/doc-check.pl hosts/tcc-wasm` must report 0
problems.

## Known limits

- A `fold` over a bare `nil` cannot infer its type (`TYPE_INFER`). Use a
  definition with a declared `List` type.
- With no expected type, `either`, `map` and `cong` take the result type
  of the function at a fresh variable. A dependent result needs a
  declared type.
- A definition with a `List`, `Prod` or other non-scalar result is not an
  entry. `ir` and `abi` skip it.
- `langc eval` stops after 20000000 steps (`EVAL_FUEL`). The Wasm module
  has no step limit, so a large loop count can run on Wasm and stop in
  `eval`. The grid uses large values only for entries with no loop.
- The Metal backend does not compile 6 WGSL entries (O12 in
  `docs/PLAN.md`). `test/wgsl/internal.txt` lists them, and the gpu gate
  accepts a listed entry only when it gives `GPUInternalError`.
- The arena of `langc` stops at 1 GiB. The Wasm memory has a static size (min = max, Q-S8-16): P + 1
  pages, where P is the least power of two pages that holds the heap of each entry, at most 16384 pages
  (1 GiB). An entry above that refuses with REFUSE_CAPACITY (HOST-LIMIT). Each load and store address is
  masked with P * 65536 - 1, and page P is a guard page (Q-S8-18 (a)), so no load or store traps.
- Source files are at most 1 MiB. `Nat` literals are at most
  18446744073709551615.
