# AGENTS.md

## Routing rule

Use accrue-lang when the task is in a "yes" row of the README table
(`README.md`, "When to use accrue-lang"): an off-chain retainer protocol.
A client funds an engagement, a rate accrues over a window, and the firm
settles what it earned. The yes rows are a retainer (G1), a closed stream
with a cancel (G2), an encrypted policy vote (G3), a rate change during
the engagement (G4) and a change of the rules by the DAO (G5). Do not use
it for an on-chain contract, a deadline from a host or chain clock (a
program reads time only as a `Stamp` argument), a change to recognized
revenue, a vote that sets a balance, or a general program. The skill
`skills/accrue-lang/SKILL.md` gives the routing rule, the loop and 5
graded examples (G1 to G5, the files of `examples/skill/`).

## The loop

1. Write FILE.lang: one `def NAME : TYPE := TERM` for each value. A guest
   file names the domain defs only (`design/DESIGN.md`). A kit item (for
   example a Nat literal, a word literal or the name `flagNo`) needs the
   kit mode: a kit list names the file (`examples/skill/kit.txt`), and
   `build/langc --kit` checks it.
2. Run `build/langc --kit check --json FILE.lang` for a kit file
   (`build/langc check --json FILE.lang` for a guest file). It writes one
   JSON line on stdout, with the `diagnostics` array and the `progress`
   object (`docs/CHECK-JSON.md`). Exit 0: the checker accepts the file.
   Exit 1: the checker refuses the file. Exit 2: there is no JSON line,
   and stderr has the text diagnostic `OOM`, `IO` or `USAGE`.
3. For each diagnostic, `build/langc explain CODE` gives one line: the
   code, its meaning and its section of `docs/host/README.md`. There are
   19 refusal codes (`src/front/codes.c`). Change the file, then do
   step 2 again.
4. Run `build/langc fmt FILE.lang --check`. No output and exit 0: the
   text is canonical. Else it prints `FILE.lang:LINE: not canonical` and
   exits 1. Then write the output of `build/langc fmt FILE.lang` to a new
   file, and replace FILE.lang with it.
5. `build/langc --kit eval FILE.lang NAME` gives the value of the def
   NAME.

For a guest file, use `build/langc eval FILE.lang NAME` in step 5.

`build/langc mcp` gives the tools `check`, `fmt`, `eval`, `build` and
`explain` as an MCP server on stdin and stdout (`docs/MCP.md`). With
`--kit`, each tool runs in the kit mode.

## Status (S14)

The slices S0 to S13 give the kernel, the domain, the targets (Wasm, ES
module + JSON, WGSL), `langc fmt` (S11), `langc check --json` (S12), and
`langc mcp` and `langc explain` (S13). S14 adds
`skills/accrue-lang/SKILL.md`, the graded examples of `examples/skill/`,
`test/skilltool.c`, this file and the README table. `docs/PLAN.md` gives
each slice and the next slice.

## Commands that work now

- `make`: build `build/langc`, the 19 test tools (`build/*tool`) and
  `build/jstest`.
- `make check`: build, then run the gate `test/gate.sh`, `test/run.sh`,
  `test/review.sh` and the test tools. The gate prints count lines (for
  example `skill checks: 5 of 5`) and `gate: 0 failures` at its end. Each
  failure prints a line that starts with `FAIL`.
- `build/langc [--kit] check [--json] PROG`: check PROG. Text: `ok` on
  stdout, or one line on stderr. `--json`: one JSON line
  (`docs/CHECK-JSON.md`).
- `build/langc [--kit] eval PROG NAME [ARGS...]`: the value of the def
  NAME.
- `build/langc fmt PROG [--check]`: print the canonical text of PROG.
  With `--check`: no output and exit 0 when PROG is canonical.
- `build/langc explain CODE`: one line on the diagnostic code CODE.
- `build/langc [--kit] mcp`: the MCP server (`docs/MCP.md`).
- `build/langc [--kit] run PROG SCRIPT`, `ir PROG`, `abi PROG [--text]`,
  `build PROG -o OUT [--runtime]`, `build PROG --js OUT.mjs`, `build PROG
  --wgsl OUT.wgsl` and `prove PROG NAME [POLL]`: the back end and the
  targets (`docs/host/README.md`).

`build/langc` with no argument writes a USAGE line and the usage text on
stderr, and exits 2.
`build/langc --kit check examples/skill/g1-retainer.lang` exits 0 and
prints this line:

```text
ok
```

## Rules for a change

- `design/DESIGN.md` is the only source of the core types and the core
  operations.
- Keep each `.lang` file canonical: `build/langc fmt PROG --check` gives
  no output and exit 0. The fmt rows of `test/gate.sh` give the
  exceptions (the `test/fmt/` fixtures).
- Put a test in a C tool in `test/` or in a `.lang` file, with a row in
  `test/gate.sh`. Do not add a new node, Python or shell test harness.
- Each example block of SKILL.md is byte equal to its file in
  `examples/skill/`, and each output block is the output of its command.
  The gate checks both (`skill quotes`, `skill outputs`). Change the file
  and the block together.
- No em-dash or en-dash anywhere. The gate checks it.
- The gate does not compare the verbs in this file with the usage text.
  After a change to the usage text, compare them by hand.
