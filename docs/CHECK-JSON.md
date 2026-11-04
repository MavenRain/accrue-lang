# `langc check --json`

`langc check --json` checks a program as `langc check` does. It writes one
JSON line on stdout. A tool reads this line in place of `ok` or the text
diagnostic on stderr. S12 adds it (PLAN row A2, rulings Q-S12-1 to
Q-S12-7). It has the form of cairn-lang S12.

## Usage

```sh
build/langc check --json PROG
build/langc --kit check --json PROG
```

| Exit | stdout | stderr |
|---|---|---|
| 0 | the JSON line, with an empty diagnostic list | nothing |
| 1 | the JSON line, with one diagnostic: a failure of the lexer, the parser or the check | nothing |
| 2 | nothing | the text diagnostic `OOM`, `IO` or `USAGE` |

## The line

The line is one JSON object and a newline. The keys come in this order:

```
{"diagnostics":[DIAG],"progress":{"parsed":P,"typed":T,"laws_discharged":D,"laws_total":L}}
```

When `langc` accepts the program, `diagnostics` is empty. Else it holds
one object, the first failure. An example (`test/json/REFUSE_SIG.json`):

```json
{"code":"REFUSE_SIG","span":{"file":"test/json/REFUSE_SIG.lang","line":1,"col":1,"end_line":1,"end_col":31},"message":"Outcome: the slot Outcome must have the type Type 0","fix":{"span":{"file":"test/json/REFUSE_SIG.lang","line":1,"col":15,"end_line":1,"end_col":21},"text":"Type 0"}}
```

- `code`: the diagnostic code. The `REFUSE_` codes are in
  `docs/host/README.md` (section Source language).
- `span`: the file and the source text that fails. The file is the path
  as given on the command line. Lines and columns start at 1, one byte
  is one column, and the end column is exclusive. A lexer or a parser
  failure gets the span of its token. A check failure gets the span of
  the declaration that fails. When the diagnostic has no line (line 0),
  `span` is `null`.
- `message`: `DEF: MSG`. DEF is the name of the declaration that fails,
  or `-` when there is no declaration. MSG is the text of the diagnostic.
  `langc check` writes the same text after `langc: CODE: ` on stderr.
- `fix`: `null`, or one edit: put `fix.text` in place of the text of
  `fix.span`. The edit removes this failure. Another failure can come
  next. These diagnostics have a fix:
  - `LEX_CHAR` at its 3 sites in `src/front/lexer.c` (a letter after a
    hex literal, a letter after a Nat literal and an unexpected byte):
    delete the one byte.
  - `REFUSE_CELL` for the name `List` in a guest program: put `FreeMon`
    in place of `List`. The other cons-list names have no fix.
  - `REFUSE_SIG` for a slot that has the wrong type: put the slot type in
    place of the type of the declaration (`Type 0` for a type slot). When
    the declaration has no type, the fix is `null`.
  - `REFUSE_STATE` for a second state declaration: delete the
    declaration.

Each string is JSON. `"` becomes `\"`, `\` becomes `\\`, a newline
becomes `\n` and a tab becomes `\t`. Each other byte below 0x20 becomes
`\u00XX`. An invalid UTF-8 byte becomes `\ufffd`. Valid UTF-8 stays as it
is. The writers are `json_string_n` and `json_string` in `src/json.c`.
The ABI writer `json_char` does not change.

## Progress

| Key | What it counts |
|---|---|
| `parsed` | the program decls that parse (a block counts each sort) |
| `typed` | the program decls that check before the first failure |
| `laws_discharged` | the typed program defs that are laws only (`law_only`) |
| `laws_total` | `laws_discharged`, plus the program defs that did not check and that a syntax test of rule O7 marks as laws (`LawScan` in `src/front/check.c`) |

Only program decls count. The decls of `domain/domain.lang`, the kit
prelude and the decls that the kernel splices in (Orbit, the DAO prelude
and the ZK surface, `src/front/front.c`) do not count. After a `LEX_` or
a `PARSE_` failure, the 4 counts are 0.

LawScan reads the syntax only. It marks a def as a law when the def is a
`law def`, or when the def uses a law-only name: a law def outside the
program, a typed program def with `law_only`, an earlier untyped program
def that LawScan marked, or a field of a sealed family. An `elim` or a
`fold` counts when the def also names a sealed family. When the failing
def gives `REFUSE_LAW`, it is a law. LawScan has these limits:

- It does no type inference. Thus it does not see an `elim` or a `fold`
  of a sealed value when the def does not name the family.
- An `elim` or a `fold` of another value counts when the def names a
  sealed family.
- It does not see the normal form of a type.
- A program `law def` is a law for LawScan, but the check refuses it
  (`REFUSE_DATA`, rule O7).

## FIXFILE

`build/fronttool fix PROG FIXFILE` applies one fix to PROG and writes the
result on stdout. FIXFILE has this form:

- line 1: `LINE COL END_LINE END_COL`, the span of the fix. Lines and
  columns start at 1, one byte is one column, and the end is exclusive.
  In a file of N lines that ends in a newline, line N + 1 column 1 is
  the end of the file.
- the remaining text of the file: the new text, less one final newline.

The new text can be empty or end in a space. Thus `.gitattributes` turns
off the git whitespace check for `test/json/*.fix`.

An example, `test/json/REFUSE_SIG.fix`:

```
1 15 1 21
Type 0
```

The row `test/json/REFUSE_SIG.lang` is `def Outcome : Type 1 := Type 0`.
The fix puts `Type 0` in place of `Type 1` (line 1, columns 15 to 20).

## The test rows

The `json rows` block of `test/gate.sh` runs each `test/json/*.lang` file
as a row:

- `CODE.lang`: a row for the code CODE. `ok.lang` and `kit.lang` are
  accepted rows. `langc` refuses each other row, and its first code is
  CODE.
- `CODE.TAG.lang`: one more row for CODE. The code is the file name up to
  the first dot. In `TYPE_MISMATCH.after.lang`, a program law comes after
  the failing decl, thus `laws_total` is 1. In
  `TYPE_MISMATCH.splice.lang`, the program decls of `examples/dao3.lang`
  and `law def outcomeSet` come after the failing decl, thus the kernel
  splices the DAO prelude after the failing decl. `laws_total` is 1,
  because the spliced decls do not count.
- `X.json`: the expected line of the row X. `langc check --json` must give
  these bytes and write nothing on stderr.
- `X.fix`: the FIXFILE of each refused row X. After `fronttool fix`,
  `langc check --json` must exit 0 or give a first code other than CODE.
- The rows `kit`, `REFUSE_HISTORY_WRITE`, `REFUSE_MAP` and `REFUSE_STATE`
  run with `--kit`. In guest mode they give `REFUSE_KIT`.
- Each `REFUSE_` code of `src/front/check.c` must have a row `CODE.lang`.
  There are 14 codes: `REFUSE_AXIOM`, `REFUSE_CELL`, `REFUSE_CLOCK`,
  `REFUSE_DATA`, `REFUSE_EVENT`, `REFUSE_HISTORY_WRITE`, `REFUSE_KIT`,
  `REFUSE_LAW`, `REFUSE_MAP`, `REFUSE_NAME`, `REFUSE_PARTIAL`,
  `REFUSE_REC`, `REFUSE_SIG` and `REFUSE_STATE`.
- The `fmt` block of `test/gate.sh` reads each `test/*/*.lang` file, thus
  each row. Each row must be `langc fmt` canonical (`build/langc fmt
  X.lang | cmp - X.lang`). `langc fmt` refuses `LEX_CHAR.lang` and
  `PARSE_EXPECT.lang`. Both are listed in `test/fmt/refused.txt`, so the
  `fmt` block checks that they are refused.

To add a row X:

1. Write `test/json/X.lang`, and make sure that it is canonical.
2. From the repo root, run `build/langc [--kit] check --json
   test/json/X.lang > test/json/X.json`. The span file is the path as
   given, thus use this relative path.
3. Read the line. Count the progress values by hand.
4. For a refused row, write `test/json/X.fix`.

At S12 the gate prints `json rows: 21 passed`, `json refusal codes: 14 of
14` and `json fixes: 19 passed`.
