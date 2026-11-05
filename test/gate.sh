#!/bin/sh
# The kit gate. `make check` runs it from the kit directory after the build.
set -eu
fail=0
count=0
# `sh test/gate.sh gpu` adds the WebGPU rows (accrue S10 P3, make gpu-check).
gpu=${1:-}
# kit LIST FILE prints --kit when FILE is a line of LIST. A row whose text uses a
# kit name or a Nat or word literal is a kit row (orbit-lang S3); the others run
# in guest mode.
kit() {
  if [ -f "$1" ] && rg -q -x -F "$2" "$1"; then echo --kit; fi
}
for prog in examples/*.lang; do
  out=$(build/langc $(kit examples/kit.txt "${prog#examples/}") check "$prog" 2>&1) || true
  count=$((count + 1))
  if [ "$out" != "ok" ]; then
    echo "FAIL $prog: $out"
    fail=$((fail + 1))
  fi
done
echo "examples: $count checked, $fail failed"

tmp=${TMPDIR:-/tmp}/langc-gate.$$
mkdir -p "$tmp"
awk 'BEGIN { printf "def deep : Nat := " } { } END {
  for (i = 0; i < 1100; i++) printf "("; printf "1"; for (i = 0; i < 1100; i++) printf ")"; print "" }' </dev/null >"$tmp/deep.lang"

# refusals DIR LABEL: each line of DIR/expect.txt is `FILE CODE`. `langc check`
# must exit 1 and print `langc: CODE: ...` on stderr.
refusals() {
  checked=0
  while read -r file code; do
    path="$1/$file"
    [ "$file" = "deep.lang" ] && path="$tmp/deep.lang"
    status=0
    build/langc $(kit "$1/kit.txt" "$file") check "$path" 2>"$tmp/err" >/dev/null || status=$?
    first=$(head -n 1 "$tmp/err")
    checked=$((checked + 1))
    case "$status:$first" in
      "1:langc: $code: "*) ;;
      *) echo "FAIL $file: want exit 1 and $code, got exit $status: $first"; fail=$((fail + 1)) ;;
    esac
  done <"$1/expect.txt"
  echo "$2 refusals: $checked checked"
}
refusals test/parse parse
refusals test/check check

# A REFUSE_KIT row refuses a kit item in guest mode only: `langc --kit check`
# must print ok.
kit_only=0
while read -r file code; do
  [ "$code" = "REFUSE_KIT" ] || continue
  status=0
  out=$(build/langc --kit check "test/check/$file" 2>&1) || status=$?
  kit_only=$((kit_only + 1))
  if [ "$status:$out" != "0:ok" ]; then
    echo "FAIL $file: want ok under --kit, got exit $status: $out"
    fail=$((fail + 1))
  fi
done <test/check/expect.txt
echo "kit-only refusals: $kit_only checked"

# A REFUSE_CLOCK refusal names Stamp (S4, Q-S4-1 (a)), in kit and guest mode.
clock_hint=0
for file in clock-now.lang clock-clock.lang; do
  build/langc $(kit test/check/kit.txt "$file") check "test/check/$file" 2>"$tmp/err" >/dev/null || true
  clock_hint=$((clock_hint + 1))
  if ! rg -q -F 'no program can read a chain or host clock; take the time as a Stamp argument' "$tmp/err"; then
    echo "FAIL $file: the message does not name Stamp"
    fail=$((fail + 1))
  fi
done
echo "clock hint: $clock_hint checked"

# A refl refusal shows both normal forms.
build/langc $(kit test/check/kit.txt bad-refl.lang) check test/check/bad-refl.lang 2>"$tmp/err" >/dev/null || true
if ! rg -q -F 'the path at i1 is some 1201, but the type needs some 1200' "$tmp/err"; then
  echo "FAIL bad-refl.lang: the message does not show both normal forms"
  fail=$((fail + 1))
fi

# Each line of test/eval/expect.txt is `FILE NAME [ARGS] => OUTPUT`. `langc eval`
# must exit 0 and print OUTPUT.
evals=0
while IFS= read -r line; do
  want=${line#* => }
  set -- ${line%% => *}
  file=$1
  name=$2
  shift 2
  status=0
  got=$(build/langc $(kit examples/kit.txt "$file.lang") eval "examples/$file.lang" "$name" "$@" 2>"$tmp/err") || status=$?
  first=$(head -n 1 "$tmp/err")
  evals=$((evals + 1))
  case "$status|$got|$first" in
    "0|$want|"*) ;;
    *) echo "FAIL eval $file $name: want $want, got exit $status: $got $first"; fail=$((fail + 1)) ;;
  esac
done <test/eval/expect.txt
echo "evals: $evals checked"

# eval_refused STATUS CODE NAME [ARGS]: `langc --kit eval examples/entries.lang
# NAME ARGS` must exit STATUS and print `langc: CODE: NAME: ...` on stderr.
eval_refused() {
  want_status=$1
  code=$2
  shift 2
  status=0
  build/langc --kit eval examples/entries.lang "$@" 2>"$tmp/err" >/dev/null || status=$?
  case "$status:$(head -n 1 "$tmp/err")" in
    "$want_status:langc: $code: $1: "*) ;;
    *) echo "FAIL eval $*: want exit $want_status and $code, got exit $status"; fail=$((fail + 1)) ;;
  esac
}
eval_refused 2 EVAL_ARGS addPrice 1
eval_refused 2 EVAL_ARGS addPrice x 1
eval_refused 2 EVAL_ARGS addPrice 18446744073709551616 1
eval_refused 2 EVAL_ARGS pick 2 4 9
eval_refused 1 EVAL_ENTRY nope

# Each line of test/prove/expect.txt is `FILE NAME [POLL] => WORDS`. `langc prove`
# must exit 0 and print the words of WORDS, one per line (S8 B2c).
proves=0
while IFS= read -r line; do
  want=${line#* => }
  set -- ${line%% => *}
  file=$1
  name=$2
  shift 2
  status=0
  build/langc $(kit examples/kit.txt "$file.lang") prove "examples/$file.lang" "$name" "$@" >"$tmp/out" 2>"$tmp/err" || status=$?
  got=$(tr '\n' ' ' <"$tmp/out")
  got=${got% }
  proves=$((proves + 1))
  case "$status|$got" in
    "0|$want") ;;
    *) echo "FAIL prove $file $name: want $want, got exit $status: $got"; fail=$((fail + 1)) ;;
  esac
done <test/prove/expect.txt
echo "proves: $proves checked"

# prove_refused STATUS CODE FILE NAME [POLL]: `langc prove examples/FILE.lang
# NAME POLL` must exit STATUS and print `langc: CODE: NAME: ...` on stderr.
prove_refused() {
  want_status=$1
  code=$2
  file=$3
  shift 3
  status=0
  build/langc prove "examples/$file.lang" "$@" 2>"$tmp/err" >/dev/null || status=$?
  case "$status:$(head -n 1 "$tmp/err")" in
    "$want_status:langc: $code: $1: "*) ;;
    *) echo "FAIL prove $file $*: want exit $want_status and $code, got exit $status"; fail=$((fail + 1)) ;;
  esac
}
prove_refused 1 PROVE_BALLOT dao w0
prove_refused 2 EVAL_ARGS dao b0 x

# The target (S8, Q-S8-1 (a), D9). `langc build examples/dao.lang -o OUT` must exit 0,
# write nothing on stdout and write a valid module. build_refused FLAGS CODE
# FILE: `langc FLAGS build FILE -o OUT` must exit 1, print `langc: CODE: ...` on
# stderr and write nothing on stdout.
status=0
build/langc $(kit examples/kit.txt dao.lang) build examples/dao.lang -o "$tmp/dao.wasm" >"$tmp/out" 2>"$tmp/err" || status=$?
case "$status:$(wc -c <"$tmp/out" | tr -d ' ')" in
  0:0) wasm-validate "$tmp/dao.wasm" 2>"$tmp/err" || { echo "FAIL build dao: the module is not valid: $(head -n 1 "$tmp/err")"; fail=$((fail + 1)); } ;;
  *) echo "FAIL build dao: want exit 0 and no stdout, got exit $status: $(head -n 1 "$tmp/err")"; fail=$((fail + 1)) ;;
esac
builds=0
build_refused() {
  flags=$1
  code=$2
  file=$3
  status=0
  build/langc $flags build "$file" -o "$tmp/refused.wasm" >"$tmp/out" 2>"$tmp/err" || status=$?
  builds=$((builds + 1))
  case "$status:$(head -n 1 "$tmp/err"):$(wc -c <"$tmp/out" | tr -d ' ')" in
    "1:langc: $code: "*":0") ;;
    *) echo "FAIL build $file: want exit 1 and $code, got exit $status: $(head -n 1 "$tmp/err")"; fail=$((fail + 1)) ;;
  esac
}
build_refused --kit REFUSE_RUNTIME_CLOSURE test/ir/closure.lang
build_refused --kit REFUSE_RUNTIME_TREE test/ir/tree.lang
build_refused --kit REFUSE_LOWER examples/zk.lang
build_refused --kit REFUSE_LOWER test/ir/transp.lang
build_refused "" REFUSE_KIT test/ir/comb.lang
build_refused --kit REFUSE_CAPACITY test/ir/capacity.lang
# lower.c lowers test/ir/heapcap.lang. The refusal comes from the Wasm
# emitter (src/wasm.c, program_pages): the heap of mapUp needs more than
# 16384 pages (S8 P3c).
build_refused --kit REFUSE_CAPACITY test/ir/heapcap.lang
# test/ir/heapif.lang (Q-S8-20 (a), Q-S8-21 (a)): the over-cap heap is in
# one arm of a flagIf (the other arm needs 0), from a loop with a literal
# count. A heap_max that takes the smaller arm, or a heap_mul that gives
# a + b, builds the file.
build_refused --kit REFUSE_CAPACITY test/ir/heapif.lang
# S9 P4 (Q-S9-4 (a), Q-S9-5 (a), PLAN O4): a Secret value or a zk secret
# operation at run time is refused on the ES target (`build --js`) and, from
# accrue S10 P1 (Q-S10-4), on the WGSL target (`build --wgsl`) only. The
# Wasm build of the same file gives a module (D8), or REFUSE_LOWER for the zk
# operation.
build_refused_js() {
  flags=$1
  code=$2
  file=$3
  status=0
  build/langc $flags build "$file" --js "$tmp/refused.mjs" >"$tmp/out" 2>"$tmp/err" || status=$?
  builds=$((builds + 1))
  case "$status:$(head -n 1 "$tmp/err"):$(wc -c <"$tmp/out" | tr -d ' ')" in
    "1:langc: $code: "*":0") ;;
    *) echo "FAIL build --js $file: want exit 1 and $code, got exit $status: $(head -n 1 "$tmp/err")"; fail=$((fail + 1)) ;;
  esac
}
for f in test/ir/secret-param.lang test/ir/secret-result.lang test/ir/secret-carrier.lang test/ir/secret-const.lang; do
  build_refused_js --kit REFUSE_TARGET_SECRET "$f"
  build/langc --kit build "$f" -o "$tmp/secret.wasm" 2>"$tmp/err" ||
    { echo "FAIL build $f: the Wasm build must give a module (D8): $(head -n 1 "$tmp/err")"; fail=$((fail + 1)); }
done
build_refused_js --kit REFUSE_TARGET_SECRET test/ir/secret-key.lang
# accrue S10 P1 (Q-S10-4): the WGSL target refuses the same five files.
build_refused_wgsl() {
  flags=$1
  code=$2
  file=$3
  status=0
  build/langc $flags build "$file" --wgsl "$tmp/refused.wgsl" >"$tmp/out" 2>"$tmp/err" || status=$?
  builds=$((builds + 1))
  case "$status:$(head -n 1 "$tmp/err"):$(wc -c <"$tmp/out" | tr -d ' ')" in
    "1:langc: $code: "*":0") ;;
    *) echo "FAIL build --wgsl $file: want exit 1 and $code, got exit $status: $(head -n 1 "$tmp/err")"; fail=$((fail + 1)) ;;
  esac
}
for f in test/ir/secret-param.lang test/ir/secret-result.lang test/ir/secret-carrier.lang test/ir/secret-const.lang test/ir/secret-key.lang; do
  build_refused_wgsl --kit REFUSE_TARGET_SECRET "$f"
done
build_refused --kit REFUSE_LOWER test/ir/secret-key.lang
echo "build refusals: $builds checked"

# The IR (S8 P1): each example lowers, with one `func` line for each `abi
# --text` line, and its module is valid (`wasm-validate`, `wasm-objdump -x`).
# An example that does not lower must refuse REFUSE_LOWER (or REFUSE_CAPACITY
# (S8 P3): its heap has no static bound) with a HOST-LIMIT
# detail (a host proof check, as examples/zk.lang; the secp256k1 ops
# lower).
irs=0
host_limits=0
wasms=0
wgsls=0
wgsl_secret=0
mkdir -p "$tmp/ex"
for f in examples/*.lang; do
  k=$(kit examples/kit.txt "${f#examples/}")
  status=0
  build/langc $k ir "$f" >"$tmp/ir.txt" 2>"$tmp/ir.err" || status=$?
  case "$status:$(head -n 1 "$tmp/ir.err")" in
    "1:langc: REFUSE_LOWER: "*"(HOST-LIMIT)"*) host_limits=$((host_limits + 1)); continue ;;
    "1:langc: REFUSE_CAPACITY: "*"(HOST-LIMIT)"*) host_limits=$((host_limits + 1)); continue ;;
    0:) ;;
    *) echo "FAIL ir $f: exit $status: $(head -n 1 "$tmp/ir.err")"; fail=$((fail + 1)); continue ;;
  esac
  funcs=$(rg -c '^func ' "$tmp/ir.txt" || echo 0)
  entries=$(build/langc $k abi "$f" --text | rg -c '.' || echo 0)
  [ "$funcs" -eq "$entries" ] || { echo "FAIL ir $f: $funcs funcs for $entries entries"; fail=$((fail + 1)); continue; }
  irs=$((irs + 1))
  w="$tmp/ex/$(basename "$f" .lang).wasm"
  if ! build/langc $k build "$f" -o "$w" 2>"$tmp/w.err" || ! wasm-validate "$w" 2>>"$tmp/w.err" ||
    ! wasm-objdump -x "$w" >/dev/null 2>>"$tmp/w.err"; then
    echo "FAIL wasm $f: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
    continue
  fi
  wasms=$((wasms + 1))
  # accrue S10 P1 (from cairn-lang S10): the module builds with --wgsl too: exit
  # 0, no stdout, a module file. A file with a Secret is refused (Q-S10-4).
  g="$tmp/ex/$(basename "$f" .lang).wgsl"
  status=0
  build/langc $k build "$f" --wgsl "$g" >"$tmp/out" 2>"$tmp/g.err" || status=$?
  case "$status:$(head -n 1 "$tmp/g.err")" in
    "1:langc: REFUSE_TARGET_SECRET: "*) wgsl_secret=$((wgsl_secret + 1)); continue ;;
  esac
  if [ "$status:$(wc -c <"$tmp/out" | tr -d ' ')" != 0:0 ] || [ ! -s "$g" ]; then
    echo "FAIL wgsl $f: want exit 0, no stdout and a module, got exit $status: $(head -n 1 "$tmp/g.err")"
    fail=$((fail + 1))
    continue
  fi
  wgsls=$((wgsls + 1))
done
echo "ir: $irs lowered, $host_limits refused (HOST-LIMIT)"
echo "wasm: $wasms modules valid"
echo "wgsl: $wgsls modules written, $wgsl_secret refused (REFUSE_TARGET_SECRET)"
# accrue S10 P1: the --wgsl usage rows of src/main.c: build takes one output,
# and --wgsl needs a path.
wgsl_refused=0
wgsl_usage() {
  msg=$1
  shift
  status=0
  build/langc --kit build test/ir/comb.lang "$@" >"$tmp/out" 2>"$tmp/err" || status=$?
  wgsl_refused=$((wgsl_refused + 1))
  case "$status:$(head -n 1 "$tmp/err"):$(wc -c <"$tmp/out" | tr -d ' ')" in
    "2:langc: USAGE: "*"$msg:0") ;;
    *) echo "FAIL build $*: want exit 2 and USAGE, got exit $status: $(head -n 1 "$tmp/err")"; fail=$((fail + 1)) ;;
  esac
}
wgsl_usage "build takes one of -o OUT, --js OUT.mjs and --wgsl OUT.wgsl" --wgsl "$tmp/a.wgsl" --js "$tmp/a.mjs"
wgsl_usage "build takes one of -o OUT, --js OUT.mjs and --wgsl OUT.wgsl" -o "$tmp/a.wasm" --wgsl "$tmp/a.wgsl"
wgsl_usage "build needs -o OUT, --js OUT.mjs or --wgsl OUT.wgsl"
wgsl_usage "unknown build argument '--wgsl'" --wgsl
echo "wgsl refusals: $wgsl_refused checked"

# The Wasm differential (S8 P1): each test/eval/expect.txt row whose NAME is an
# entry of FILE (a line of its `abi --text`) runs on the module in
# build/wasmtool and must print the row OUTPUT (R0: no row is `trap`). Each FILE
# builds once and runs once, with its rows on stdin.
mkdir -p "$tmp/wd"
while IFS= read -r line; do
  want=${line#* => }
  set -- ${line%% => *}
  file=$1
  shift
  if [ ! -f "$tmp/wd/$file.abi" ]; then
    k=$(kit examples/kit.txt "$file.lang")
    : >"$tmp/wd/$file.abi"
    build/langc $k build "examples/$file.lang" -o "$tmp/wd/$file.wasm" 2>/dev/null &&
      build/langc $k abi "examples/$file.lang" --text >"$tmp/wd/$file.abi" 2>/dev/null || :
  fi
  rg -q "^$1 \(" "$tmp/wd/$file.abi" || continue
  echo "$*" >>"$tmp/wd/$file.in"
  echo "$want" >>"$tmp/wd/$file.want"
done <test/eval/expect.txt
rows=0
mismatches=0
for in_file in "$tmp"/wd/*.in; do
  file=$(basename "$in_file" .in)
  if ! build/wasmtool "$tmp/wd/$file.wasm" "$tmp/wd/$file.abi" <"$in_file" >"$tmp/wd/$file.got" 2>"$tmp/w.err"; then
    echo "FAIL wasmtool $file: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
    continue
  fi
  rows=$((rows + $(rg -c '.' "$in_file")))
  paste -d '|' "$in_file" "$tmp/wd/$file.want" "$tmp/wd/$file.got" | awk -F '|' '$2 != $3' >"$tmp/bad.txt"
  bad=$(rg -c '.' "$tmp/bad.txt" || echo 0)
  mismatches=$((mismatches + bad))
  [ "$bad" -eq 0 ] || { echo "FAIL wasm differential $file: $bad mismatches, first $(head -n 1 "$tmp/bad.txt")"; fail=$((fail + 1)); }
done
echo "wasm differential: $rows rows, $mismatches mismatches"
# The JS differential (S9 P2, Q-S9-2 (a)): the same rows run on the ES module
# of `langc build FILE --js` in build/jstool (under node) and must print the
# same OUTPUT. R0: a throw is a `bad:` line, so it is a mismatch.
js_rows=0
js_bad=0
for in_file in "$tmp"/wd/*.in; do
  file=$(basename "$in_file" .in)
  k=$(kit examples/kit.txt "$file.lang")
  if ! build/langc $k build "examples/$file.lang" --js "$tmp/wd/$file.mjs" 2>"$tmp/w.err" ||
    ! build/jstool "$tmp/wd/$file.mjs" "$tmp/wd/$file.abi" <"$in_file" >"$tmp/wd/$file.js" 2>>"$tmp/w.err"; then
    echo "FAIL jstool $file: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
    continue
  fi
  js_rows=$((js_rows + $(rg -c '.' "$in_file")))
  paste -d '|' "$in_file" "$tmp/wd/$file.want" "$tmp/wd/$file.js" | awk -F '|' '$2 != $3' >"$tmp/bad.txt"
  bad=$(rg -c '.' "$tmp/bad.txt" || echo 0)
  js_bad=$((js_bad + bad))
  [ "$bad" -eq 0 ] || { echo "FAIL js differential $file: $bad mismatches, first $(head -n 1 "$tmp/bad.txt")"; fail=$((fail + 1)); }
done
[ "$js_rows" -eq "$rows" ] || { echo "FAIL js differential: $js_rows rows for $rows wasm rows"; fail=$((fail + 1)); }
echo "js differential: $js_rows rows, $js_bad mismatches"
# D10: a Flag argument is x != 0, so the Wasm pick 2 4 9 gives 4 (langc eval
# refuses it with EVAL_ARGS above). No differential row builds the entries
# module. The ratio def lowers after S8 P4, but the square def of
# examples/entries.lang refuses (REFUSE_CAPACITY: the heap of a loop with a
# run-time count, HOST-LIMIT), so the row builds the pick def alone.
k=$(kit examples/kit.txt entries.lang)
# The canonical layout can put a def on more than one line: take the def line and the indented lines after it.
awk '/^def pick /{p=1; print; next} p && /^  /{print; next} {p=0}' examples/entries.lang >"$tmp/pick.lang"
build/langc $k build "$tmp/pick.lang" -o "$tmp/pick.wasm" 2>/dev/null &&
  build/langc $k abi "$tmp/pick.lang" --text >"$tmp/pick.abi" 2>/dev/null || :
got=$(echo 'pick 2 4 9' | build/wasmtool "$tmp/pick.wasm" "$tmp/pick.abi" 2>&1) || true
[ "$got" = "4" ] || { echo "FAIL wasm pick 2 4 9: want 4 (D10: a Flag argument is x != 0), got $got"; fail=$((fail + 1)); }
build/langc $k build "$tmp/pick.lang" --js "$tmp/pick.mjs" 2>/dev/null || :
got=$(echo 'pick 2 4 9' | build/jstool "$tmp/pick.mjs" "$tmp/pick.abi" 2>&1) || true
[ "$got" = "4" ] || { echo "FAIL js pick 2 4 9: want 4 (D10: a Flag argument is x != 0), got $got"; fail=$((fail + 1)); }

# The test/ir modules (S8 P1): each builds in --kit mode to a valid
# module. Each row `FILE NAME ARGS` below runs on the module of test/ir/FILE.lang
# in build/wasmtool and must print the `langc --kit eval` output
# (R0: no row is `trap`).
mkdir -p "$tmp/ir"
ir_mods=0
for m in comb lazy spine proof-trap memname; do
  if ! build/langc --kit build "test/ir/$m.lang" -o "$tmp/ir/$m.wasm" 2>"$tmp/w.err" ||
    ! wasm-validate "$tmp/ir/$m.wasm" 2>>"$tmp/w.err" ||
    ! build/langc --kit abi "test/ir/$m.lang" --text >"$tmp/ir/$m.abi" 2>>"$tmp/w.err"; then
    echo "FAIL test/ir wasm $m: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
    continue
  fi
  ir_mods=$((ir_mods + 1))
  : >"$tmp/ir/$m.in"
  : >"$tmp/ir/$m.want"
done
while IFS= read -r line; do
  set -- $line
  m=$1
  shift
  [ -f "$tmp/ir/$m.in" ] || continue
  want=$(build/langc --kit eval "test/ir/$m.lang" "$@" 2>/dev/null) || :
  [ -n "$want" ] || { echo "FAIL test/ir eval $line: no output"; fail=$((fail + 1)); continue; }
  echo "$*" >>"$tmp/ir/$m.in"
  echo "$want" >>"$tmp/ir/$m.want"
done <<'EOF'
comb snocChain 0
comb snocChain 1
comb snocChain 3
comb snocRun 0
comb snocRun 1
comb snocRun 3
comb consChain 0
comb consChain 1
comb consChain 3
comb consRun 0
comb consRun 1
comb consRun 3
comb mixChain 0
comb mixChain 1
comb mixChain 3
comb mixRun 0
comb mixRun 1
comb mixRun 3
comb snocLen 0
comb snocLen 1
comb snocLen 3
lazy lazy 3
spine trapHead 3
spine trapSum 3
spine filterTrap 3
spine bindTrap 3
proof-trap proofMove 0
proof-trap proofMove 18446744073709551615
proof-trap proofSymm 18446744073709551615
proof-trap proofTrans 18446744073709551615
proof-trap proofCong 18446744073709551615
memname memory
EOF
ir_rows=0
ir_bad=0
for m in comb lazy spine proof-trap memname; do
  [ -f "$tmp/ir/$m.in" ] || continue
  if ! build/wasmtool "$tmp/ir/$m.wasm" "$tmp/ir/$m.abi" <"$tmp/ir/$m.in" >"$tmp/ir/$m.got" 2>"$tmp/w.err"; then
    echo "FAIL wasmtool test/ir/$m.lang: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
    continue
  fi
  ir_rows=$((ir_rows + $(rg -c '.' "$tmp/ir/$m.in" || echo 0)))
  paste -d '|' "$tmp/ir/$m.in" "$tmp/ir/$m.want" "$tmp/ir/$m.got" | awk -F '|' '$2 != $3' >"$tmp/bad.txt"
  bad=$(rg -c '.' "$tmp/bad.txt" || echo 0)
  ir_bad=$((ir_bad + bad))
  [ "$bad" -eq 0 ] || { echo "FAIL test/ir wasm $m: $bad mismatches, first $(head -n 1 "$tmp/bad.txt")"; fail=$((fail + 1)); }
done
echo "test/ir wasm: $ir_mods modules valid, $ir_rows rows, $ir_bad mismatches"
# The test/ir rows on the JS module (S9 P2): build/jstool must print the same
# `langc --kit eval` output (R0: no row is `trap`).
ir_js_rows=0
ir_js_bad=0
for m in comb lazy spine proof-trap memname; do
  [ -f "$tmp/ir/$m.in" ] || continue
  if ! build/langc --kit build "test/ir/$m.lang" --js "$tmp/ir/$m.mjs" 2>"$tmp/w.err" ||
    ! build/jstool "$tmp/ir/$m.mjs" "$tmp/ir/$m.abi" <"$tmp/ir/$m.in" >"$tmp/ir/$m.js" 2>>"$tmp/w.err"; then
    echo "FAIL jstool test/ir/$m.lang: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
    continue
  fi
  ir_js_rows=$((ir_js_rows + $(rg -c '.' "$tmp/ir/$m.in" || echo 0)))
  paste -d '|' "$tmp/ir/$m.in" "$tmp/ir/$m.want" "$tmp/ir/$m.js" | awk -F '|' '$2 != $3' >"$tmp/bad.txt"
  bad=$(rg -c '.' "$tmp/bad.txt" || echo 0)
  ir_js_bad=$((ir_js_bad + bad))
  [ "$bad" -eq 0 ] || { echo "FAIL test/ir js $m: $bad mismatches, first $(head -n 1 "$tmp/bad.txt")"; fail=$((fail + 1)); }
done
[ "$ir_js_rows" -eq "$ir_rows" ] || { echo "FAIL test/ir js: $ir_js_rows rows for $ir_rows wasm rows"; fail=$((fail + 1)); }
echo "test/ir js: $ir_js_rows rows, $ir_js_bad mismatches"

# S9 P5 (Q-S9-6 (a), Q-S9-7 (a)): the JSON abi is the default form of
# `langc abi` (src/json.c, the rows of cairn-lang 6b73467 test/gate.sh
# 409-419). An Option result has the word option before its kind, as in
# --text. edge.lang and append.lang add the Option results and entries with
# no params.
json_rows=0
while IFS='|' read -r json_prog json_want; do
  json_got=$(build/langc --kit abi "$json_prog" 2>&1) || json_got="exit $?"
  [ "$json_got" = "$json_want" ] || { echo "FAIL json abi: build/langc --kit abi $json_prog is not the JSON abi line"; fail=$((fail + 1)); }
  json_rows=$((json_rows + 1))
done <<'EOF'
test/wasm/secp.lang|{"entries":[{"name":"addX","params":["u256","u256","u256","u256"],"result":"u256"},{"name":"addY","params":["u256","u256","u256","u256"],"result":"u256"},{"name":"mulX","params":["u256","u256","u256"],"result":"u256"},{"name":"mulY","params":["u256","u256","u256"],"result":"u256"},{"name":"sigOk","params":["u256","u256","u256","u256","u256"],"result":"flag"}]}
test/wasm/edge.lang|{"entries":[{"name":"mul","params":["u256","u256"],"result":"option u256"},{"name":"ratioW","params":["u256","u256","u256"],"result":"option u256"},{"name":"addN","params":["nat","nat"],"result":"option nat"},{"name":"nz","params":["nat"],"result":"option nat"},{"name":"nzW","params":["u256"],"result":"option u256"},{"name":"empty","params":["nat"],"result":"nat"},{"name":"full","params":["nat"],"result":"nat"}]}
examples/append.lang|{"entries":[{"name":"kA","params":[],"result":"u256"},{"name":"kO","params":[],"result":"u256"},{"name":"lenAfter","params":[],"result":"option flag"},{"name":"headAfter","params":[],"result":"option flag"}]}
EOF
json_rc=0
build/langc --kit abi test/wasm/secp.lang --bad > /dev/null 2> "$tmp/json-abi.err" || json_rc=$?
json_msg=$(head -n 1 "$tmp/json-abi.err")
case "$json_rc $json_msg" in
  "2 "*USAGE*"abi takes only --text"*) ;;
  *) echo "FAIL json abi: --bad gave exit $json_rc, line 1: $json_msg"; fail=$((fail + 1)) ;;
esac
echo "json abi: $((json_rows + 1)) checked"

# json rows: begin
# S12 A2 (the rows of cairn-lang 98d18ef test/gate.sh). Each
# test/json/CODE.lang is a row: `langc check --json` writes
# test/json/CODE.json on stdout and nothing on stderr. It exits 0 for ok and
# kit, else it exits 1 and the first code is CODE. The rows of json_kit run
# with --kit. Each REFUSE_ code of src/front/check.c (14) has a row. Each row
# other than ok and kit has test/json/CODE.fix: `fronttool fix` applies it,
# and `langc check --json` of the result exits 0 or gives a first code other
# than CODE.
# A row CODE.TAG.lang is one more row for CODE (the code is the file name up
# to the first dot); its .json and .fix go by the name CODE.TAG.
json_kit() {
  case $1 in
    kit | REFUSE_HISTORY_WRITE | REFUSE_MAP | REFUSE_STATE) echo --kit ;;
  esac
}
jrows=0
for f in test/json/*.lang; do
  stem=$(basename "$f" .lang)
  code=${stem%%.*}
  case $code in
    ok | kit) want=0 head='{"diagnostics":[],' ;;
    *)
      want=1 head="{\"diagnostics\":[{\"code\":\"$code\","
      [ -f "test/json/$stem.fix" ] || { echo "FAIL json $f: test/json/$stem.fix is missing"; fail=$((fail + 1)); }
      ;;
  esac
  status=0
  build/langc $(json_kit "$code") check --json "$f" >"$tmp/json.out" 2>"$tmp/json.err" || status=$?
  first=0
  case $(cat "$tmp/json.out") in
    "$head"*) first=1 ;;
  esac
  if [ "$status" -eq "$want" ] && [ "$first" -eq 1 ] && [ ! -s "$tmp/json.err" ] && cmp -s "${f%.lang}.json" "$tmp/json.out"; then
    jrows=$((jrows + 1))
  else
    echo "FAIL json $f: want exit $want and ${f%.lang}.json, got exit $status: $(head -c 80 "$tmp/json.out")"
    fail=$((fail + 1))
  fi
done
jcodes=0
jwant=0
for code in $(rg -o --no-filename 'REFUSE_[A-Z_]+' src/front/check.c | sort -u); do
  jwant=$((jwant + 1))
  if [ -f "test/json/$code.lang" ]; then
    jcodes=$((jcodes + 1))
  else
    echo "FAIL json: no row test/json/$code.lang for the check code $code"
    fail=$((fail + 1))
  fi
done
[ "$jwant" -gt 0 ] || { echo "FAIL json: no REFUSE_ code in src/front/check.c"; fail=$((fail + 1)); }
jfixes=0
for x in test/json/*.fix; do
  stem=$(basename "$x" .fix)
  code=${stem%%.*}
  if ! build/fronttool fix "test/json/$stem.lang" "$x" >"$tmp/fixed.lang" 2>"$tmp/json.err"; then
    echo "FAIL json fix $x: fronttool fix fails: $(head -n 1 "$tmp/json.err")"
    fail=$((fail + 1))
    continue
  fi
  status=0
  build/langc $(json_kit "$code") check --json "$tmp/fixed.lang" >"$tmp/json.out" 2>"$tmp/json.err" || status=$?
  same=0
  case $(cat "$tmp/json.out") in
    "{\"diagnostics\":[{\"code\":\"$code\","*) same=1 ;;
  esac
  if [ "$status" -eq 0 ] || { [ "$status" -eq 1 ] && [ "$same" -eq 0 ]; }; then
    jfixes=$((jfixes + 1))
  else
    echo "FAIL json fix $x: the fixed file gives exit $status: $(head -c 80 "$tmp/json.out")"
    fail=$((fail + 1))
  fi
done
echo "json rows: $jrows passed"
echo "json refusal codes: $jcodes of $jwant"
echo "json fixes: $jfixes passed"
# json rows: end

# mcp rows: begin
# S13 A4 (Q-S13-2 to Q-S13-5; the rows of cairn-lang 60768e8 test/gate.sh
# L785-854). Each test/mcp/NAME.in is a row: build/mcptool runs `langc mcp`
# (`langc --kit mcp` when NAME starts with kit-) with NAME.in on stdin. The
# stdout equals test/mcp/NAME.out byte for byte, the stderr is empty and the
# exit is 0. The rows call each of the 5 tools. The REFUSE_ codes of src
# (src/front/codes.c aside) are the rows of src/front/codes.c (19), and
# `langc explain CODE` writes one line `CODE: ...` and nothing on stderr for
# each. build/mcptool exits 1 on each of 3 bad inputs: a changed .out, a
# missing .out and no rows (its FAIL lines go to $tmp, not to the log).
# accrue adds 2 checks. The build row writes build/mcp-dao.{main,runtime,js,wgsl}
# from examples/dao.lang: each file equals the file of `langc build` with
# -o, -o --runtime, --js or --wgsl (a target swap gives the same reply text,
# so only the files show it). A line of 1048577 bytes (MCP_LINE_MAX_BYTES of
# src/mcp.h, plus 1) gets the -32700 reply with id null, and the ping on the
# next line gets its reply.
status=0
build/mcptool test/mcp/*.in || status=$?
[ "$status" -eq 0 ] || { echo "FAIL mcp: build/mcptool exit $status"; fail=$((fail + 1)); }
mkdir -p "$tmp/neg"
cp test/mcp/explain.in test/mcp/explain.out "$tmp/neg/"
printf x >>"$tmp/neg/explain.out"
neg=0
status=0
build/mcptool "$tmp/neg/explain.in" >"$tmp/neg.out" 2>&1 || status=$?
if [ "$status" -eq 1 ]; then
  neg=$((neg + 1))
else
  echo "FAIL mcp negative changed .out: exit $status"
  fail=$((fail + 1))
fi
cp test/mcp/explain.out "$tmp/neg/explain.out"
cp test/mcp/explain.in "$tmp/neg/kit-explain.in"
status=0
build/mcptool "$tmp/neg/explain.in" "$tmp/neg/kit-explain.in" >"$tmp/neg.out" 2>&1 || status=$?
if [ "$status" -eq 1 ]; then
  neg=$((neg + 1))
else
  echo "FAIL mcp negative missing .out: exit $status"
  fail=$((fail + 1))
fi
status=0
build/mcptool >"$tmp/neg.out" 2>&1 || status=$?
if [ "$status" -eq 1 ]; then
  neg=$((neg + 1))
else
  echo "FAIL mcp negative no rows: exit $status"
  fail=$((fail + 1))
fi
echo "mcp negatives: $neg of 3"
tools=0
for t in check fmt eval build explain; do
  if rg -qF -- "\"method\":\"tools/call\",\"params\":{\"name\":\"$t\"" test/mcp/*.in; then
    tools=$((tools + 1))
  else
    echo "FAIL mcp tools: no row of test/mcp calls $t"
    fail=$((fail + 1))
  fi
done
echo "mcp tools: $tools of 5"
mkdir -p "$tmp/mcpb"
mbuilds=0
for t in main runtime js wgsl; do
  status=0
  case $t in
    main) build/langc build examples/dao.lang -o "$tmp/mcpb/dao.$t" >"$tmp/mcpb.out" 2>&1 || status=$? ;;
    runtime) build/langc build examples/dao.lang -o "$tmp/mcpb/dao.$t" --runtime >"$tmp/mcpb.out" 2>&1 || status=$? ;;
    js) build/langc build examples/dao.lang --js "$tmp/mcpb/dao.$t" >"$tmp/mcpb.out" 2>&1 || status=$? ;;
    wgsl) build/langc build examples/dao.lang --wgsl "$tmp/mcpb/dao.$t" >"$tmp/mcpb.out" 2>&1 || status=$? ;;
  esac
  if [ "$status" -eq 0 ] && cmp -s "build/mcp-dao.$t" "$tmp/mcpb/dao.$t"; then
    mbuilds=$((mbuilds + 1))
  else
    echo "FAIL mcp build $t: langc build exit $status, or build/mcp-dao.$t is not the file of the CLI"
    fail=$((fail + 1))
  fi
done
echo "mcp builds: $mbuilds of 4"
mpre='{"jsonrpc":"2.0","id":8,"method":"ping","params":{"pad":"'
{
  printf '%s' "$mpre"
  head -c $((1048577 - ${#mpre} - 3)) /dev/zero | tr '\0' a
  printf '"}}\n{"jsonrpc":"2.0","id":9,"method":"ping"}\n'
} >"$tmp/long.in"
printf '%s\n' '{"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error"}}' \
  '{"jsonrpc":"2.0","id":9,"result":{}}' >"$tmp/long.want"
mlong=0
mlen=$(($(head -n 1 "$tmp/long.in" | wc -c)))
status=0
build/langc mcp <"$tmp/long.in" >"$tmp/long.out" 2>"$tmp/long.err" || status=$?
if [ "$mlen" -eq 1048578 ] && [ "$status" -eq 0 ] && cmp -s "$tmp/long.want" "$tmp/long.out" && [ ! -s "$tmp/long.err" ]; then
  mlong=$((mlong + 1))
else
  echo "FAIL mcp long line: line of $mlen bytes, exit $status, or the reply is not $tmp/long.want"
  fail=$((fail + 1))
fi
echo "mcp long line: $mlong of 1"
rg -o --no-filename 'REFUSE_[A-Z0-9_]+' src -g '!codes.c' | sort -u >"$tmp/codes.src"
awk -F '"' '$1 == "  {" && $2 ~ /^REFUSE_/ {print $2}' src/front/codes.c | sort -u >"$tmp/codes.rows"
cmp -s "$tmp/codes.src" "$tmp/codes.rows" || { echo "FAIL explain codes: the REFUSE_ codes of src are not the rows of src/front/codes.c"; fail=$((fail + 1)); }
codes=0
for code in $(cat "$tmp/codes.src"); do
  status=0
  build/langc explain "$code" >"$tmp/explain.out" 2>"$tmp/explain.err" </dev/null || status=$?
  lines=$(($(wc -l <"$tmp/explain.out")))
  if [ "$status" -eq 0 ] && [ "$lines" -eq 1 ] && rg -q "^$code: " "$tmp/explain.out" && [ ! -s "$tmp/explain.err" ]; then
    codes=$((codes + 1))
  else
    echo "FAIL explain $code: exit $status, $lines lines"
    fail=$((fail + 1))
  fi
done
echo "explain codes: $codes of $(($(wc -l <"$tmp/codes.src")))"
# mcp rows: end
# skill rows: begin
# The skill rows (S14, Q-S14-3 (a)). Each examples/skill/*.lang checks to ok
# (`langc --kit check` when it is a line of examples/skill/kit.txt) and
# `langc fmt PROG --check` accepts it with no output. build/skilltool compares the
# program block of each example of skills/accrue-lang/SKILL.md with its file
# and runs each row: the output equals the text block and the exit code
# equals the sentence. build/skilltool exits 1 with the FAIL line of the
# change on each of the 3 fixtures of test/skill/ (the G1 section with a
# changed program byte, no text block and a wrong exit code); their lines go
# to $tmp, not to the log.
checked=0
for prog in examples/skill/*.lang; do
  out=$(build/langc $(kit examples/skill/kit.txt "${prog#examples/skill/}") check "$prog" 2>&1) || true
  status=0
  build/langc fmt "$prog" --check >"$tmp/skill.fmt" 2>&1 || status=$?
  if [ "$out" = "ok" ] && [ "$status" -eq 0 ] && [ ! -s "$tmp/skill.fmt" ]; then
    checked=$((checked + 1))
  else
    echo "FAIL skill check $prog: $out; fmt --check exit $status"
    fail=$((fail + 1))
  fi
done
echo "skill checks: $checked of 5"
status=0
build/skilltool || status=$?
[ "$status" -eq 0 ] || { echo "FAIL skill: build/skilltool exit $status"; fail=$((fail + 1)); }
# skill_negative NAME LINE: build/skilltool test/skill/NAME.md 1 exits 1 and
# writes the line LINE.
skill_negative() {
  status=0
  build/skilltool "test/skill/$1.md" 1 >"$tmp/skill.out" 2>&1 || status=$?
  if [ "$status" -eq 1 ] && rg -q -x -F -- "$2" "$tmp/skill.out"; then
    neg=$((neg + 1))
  else
    echo "FAIL skill negative $1: exit $status"
    fail=$((fail + 1))
  fi
}
neg=0
skill_negative changed-quote "FAIL: skill: G1: the program block is not examples/skill/g1-retainer.lang"
skill_negative no-text "FAIL: skill: G1: the row at line 58 has no command, text block or exit sentence"
skill_negative bad-exit "FAIL: skill: G1: \`build/langc --kit check examples/skill/g1-retainer.lang\` exits 0, the sentence gives '1.'"
echo "skill negatives: $neg of 3"
# skill rows: end

# P5 (Q-S8-4 (a), Q-S8-18 (a)): no module has a trap form. build/notraptool
# runs its self-test (test/notrap/expect.txt), then it checks each module that
# the gate builds: examples, test/ir and test/wasm.
build/notraptool --self-test >"$tmp/notrap.out" 2>&1 || :
if cmp -s test/notrap/expect.txt "$tmp/notrap.out"; then
  echo "notrap regressions: $(rg -c '.' test/notrap/expect.txt) checked"
else
  echo "FAIL notrap regressions: the self-test output is not test/notrap/expect.txt"
  fail=$((fail + 1))
fi
mkdir -p "$tmp/tw"
for m in hash secp traps edge; do
  build/langc --kit build "test/wasm/$m.lang" -o "$tmp/tw/$m.wasm" 2>"$tmp/w.err" ||
    { echo "FAIL notrap test/wasm $m: $(head -n 1 "$tmp/w.err")"; fail=$((fail + 1)); }
done
status=0
build/notraptool "$tmp"/ex/*.wasm "$tmp"/ir/*.wasm "$tmp"/tw/*.wasm >"$tmp/notrap.out" 2>&1 || status=$?
rg '^FAIL' "$tmp/notrap.out" | head -n 5
[ "$status" -eq 0 ] || { echo "FAIL notrap: exit $status"; fail=$((fail + 1)); }
tail -n 1 "$tmp/notrap.out"

# S9 P3 (Q-S9-3 (a)): no ES module has a trap form. build/notraptool --js
# checks each .mjs that the gate builds: examples, pick, test/ir and test/wasm.
for m in hash secp traps edge; do
  build/langc --kit build "test/wasm/$m.lang" --js "$tmp/tw/$m.mjs" 2>"$tmp/w.err" ||
    { echo "FAIL notrap js test/wasm $m: $(head -n 1 "$tmp/w.err")"; fail=$((fail + 1)); }
done
status=0
build/notraptool --js "$tmp"/wd/*.mjs "$tmp/pick.mjs" "$tmp"/ir/*.mjs "$tmp"/tw/*.mjs >"$tmp/notrap.out" 2>&1 || status=$?
rg '^FAIL' "$tmp/notrap.out" | head -n 5
[ "$status" -eq 0 ] || { echo "FAIL notrap js: exit $status"; fail=$((fail + 1)); }
tail -n 1 "$tmp/notrap.out"

# accrue S10 P1: each test/wasm module builds with --wgsl too (exit 0, no
# stdout, a module file).
tw_wgsls=0
for m in hash secp traps edge; do
  status=0
  build/langc --kit build "test/wasm/$m.lang" --wgsl "$tmp/tw/$m.wgsl" >"$tmp/out" 2>"$tmp/g.err" || status=$?
  if [ "$status:$(wc -c <"$tmp/out" | tr -d ' ')" != 0:0 ] || [ ! -s "$tmp/tw/$m.wgsl" ]; then
    echo "FAIL wgsl test/wasm $m: want exit 0, no stdout and a module, got exit $status: $(head -n 1 "$tmp/g.err")"
    fail=$((fail + 1))
    continue
  fi
  tw_wgsls=$((tw_wgsls + 1))
done
echo "wgsl test/wasm: $tw_wgsls modules written"
# accrue S10 P2 (N4): no WGSL module has a trap form. build/notraptool --wgsl
# checks each .wgsl that the gate builds: examples and test/wasm.
status=0
build/notraptool --wgsl "$tmp"/ex/*.wgsl "$tmp"/tw/*.wgsl >"$tmp/notrap.out" 2>&1 || status=$?
rg '^FAIL' "$tmp/notrap.out" | head -n 5
[ "$status" -eq 0 ] || { echo "FAIL notrap wgsl: exit $status"; fail=$((fail + 1)); }
tail -n 1 "$tmp/notrap.out"

# The differential (S9 B2) in the guest. A row of test/contract/diff.txt is
# `PROG START GUEST PERMUTED BALLOTS NAME... => OUTCOME SELF`: `langc eval
# examples/PROG.lang` of GUEST (the canonical config) and of PERMUTED (a config
# outside the act orbit) must exit 0 and print the Outcome constructor of index
# OUTCOME. The rows are read on fd 3, so langc cannot read them.
# ctor_index TEXT prints the constructor index of TEXT: each leading `inr (` adds
# 1, then `inl unit` adds 0 and `inr unit` adds 1. Other text prints -.
ctor_index() {
  _text=$1
  _index=0
  while :; do
    case $_text in
      "inr ("*) _text=${_text#"inr ("}; _index=$((_index + 1)) ;;
      *) break ;;
    esac
  done
  case ${_text%%")"*} in
    "inl unit") echo "$_index" ;;
    "inr unit") echo $((_index + 1)) ;;
    *) echo - ;;
  esac
}
diffs=0
while read -r line <&3; do
  case $line in --*|'') continue ;; esac
  set -- ${line%% => *}
  want=${line#* => }
  for name in "$3" "$4"; do
    status=0
    got=$(build/langc $(kit examples/kit.txt "$1.lang") eval "examples/$1.lang" "$name" 2>"$tmp/err") || status=$?
    diffs=$((diffs + 1))
    if [ "$status" -ne 0 ] || [ "$(ctor_index "$got")" != "${want%% *}" ]; then
      echo "FAIL differential $1 $name: want outcome ${want%% *}, got exit $status: $got $(head -n 1 "$tmp/err")"
      fail=$((fail + 1))
    fi
  done
done 3<test/contract/diff.txt
echo "differential: $diffs guest evals checked"

# fmt (S11). Each repo program that `langc fmt` accepts is canonical (fmt F
# equals F byte for byte) and keeps its AST (the fronttool dump of F equals
# the dump of fmt F). fmt refuses exactly the files in test/fmt/refused.txt.
# Each test/fmt/X.lang (not piname.lang) is not canonical by design: the
# canonical line skips it, it formats to X.canon, and `langc fmt --check`
# accepts each X.canon and refuses test/fmt/check.lang at line 3. The
# witness row: test/fmt/piname.lang is test/fmt/lambda.canon with one Pi
# binder name changed, and its dump must not equal the lambda.canon dump.
same_dump() {
  build/fronttool dump "$1" >"$tmp/d1.txt" 2>&1 && build/fronttool dump "$2" >"$tmp/d2.txt" 2>&1 &&
    cmp -s "$tmp/d1.txt" "$tmp/d2.txt"
}
canon=0
trips=0
refused=0
for f in domain/domain.lang examples/*.lang test/*/*.lang; do
  status=0
  build/langc fmt "$f" >"$tmp/fmt.out" 2>"$tmp/fmt.err" || status=$?
  listed=0
  rg -qxF -- "$f" test/fmt/refused.txt && listed=1
  if [ "$status" -ne 0 ]; then
    refused=$((refused + 1))
    [ "$listed" -eq 1 ] || { echo "FAIL fmt $f: refused, exit $status: $(head -n 1 "$tmp/fmt.err")"; fail=$((fail + 1)); }
    continue
  fi
  [ "$listed" -eq 0 ] || { echo "FAIL fmt $f: accepted, but test/fmt/refused.txt lists it"; fail=$((fail + 1)); }
  case $f in
    test/fmt/*) ;;
    *)
      if cmp -s "$f" "$tmp/fmt.out"; then
        canon=$((canon + 1))
      else
        echo "FAIL fmt $f: not canonical"
        fail=$((fail + 1))
      fi
      ;;
  esac
  if same_dump "$f" "$tmp/fmt.out"; then
    trips=$((trips + 1))
  else
    echo "FAIL fmt round trip $f: the AST dump changes after fmt"
    fail=$((fail + 1))
  fi
done
want=$(($(wc -l <test/fmt/refused.txt)))
[ "$refused" -eq "$want" ] || { echo "FAIL fmt: $refused files refused, test/fmt/refused.txt lists $want"; fail=$((fail + 1)); }
echo "fmt canonical: $canon passed"
echo "fmt round trip: $trips passed"
rows=0
checks=0
for f in test/fmt/*.lang; do
  [ "$f" = test/fmt/piname.lang ] && continue
  c=${f%.lang}.canon
  status=0
  build/langc fmt "$f" >"$tmp/fmt.out" 2>"$tmp/fmt.err" || status=$?
  if [ "$status" -eq 0 ] && cmp -s "$c" "$tmp/fmt.out"; then
    rows=$((rows + 1))
  else
    echo "FAIL fmt row $f: the output is not $c, exit $status"
    fail=$((fail + 1))
  fi
  status=0
  build/langc fmt "$c" --check >"$tmp/fmt.out" 2>"$tmp/fmt.err" || status=$?
  if [ "$status" -eq 0 ] && [ ! -s "$tmp/fmt.out" ] && [ ! -s "$tmp/fmt.err" ]; then
    checks=$((checks + 1))
  else
    echo "FAIL fmt --check $c: want exit 0 and no output, got exit $status"
    fail=$((fail + 1))
  fi
done
printf 'test/fmt/check.lang:3: not canonical\n' >"$tmp/want.err"
status=0
build/langc fmt test/fmt/check.lang --check >"$tmp/fmt.out" 2>"$tmp/fmt.err" || status=$?
if [ "$status" -eq 1 ] && [ ! -s "$tmp/fmt.out" ] && cmp -s "$tmp/want.err" "$tmp/fmt.err"; then
  checks=$((checks + 1))
else
  echo "FAIL fmt --check test/fmt/check.lang: want exit 1 and line 3, got exit $status: $(head -n 1 "$tmp/fmt.err")"
  fail=$((fail + 1))
fi
if same_dump test/fmt/lambda.canon test/fmt/piname.lang; then
  echo "FAIL fmt witness: the dump of test/fmt/piname.lang equals the dump of test/fmt/lambda.canon"
  fail=$((fail + 1))
else
  rows=$((rows + 1))
fi
echo "fmt rows: $rows passed"
echo "fmt check: $checks passed"

status=0
build/langc 2>/dev/null || status=$?
[ "$status" -eq 2 ] || { echo "FAIL usage: want exit 2, got $status"; fail=$((fail + 1)); }
status=0
build/langc --kit 2>/dev/null || status=$?
[ "$status" -eq 2 ] || { echo "FAIL usage --kit: want exit 2, got $status"; fail=$((fail + 1)); }
status=0
build/langc check "$tmp/missing.lang" 2>/dev/null || status=$?
[ "$status" -eq 2 ] || { echo "FAIL missing file: want exit 2, got $status"; fail=$((fail + 1)); }

# accrue S10 P3 (from cairn-lang S10): `sh test/gate.sh gpu` (make gpu-check,
# outside the sandbox: deno needs a GPU adapter) runs the WGSL modules in
# build/wgsltool. make check skips them.
if [ "$gpu" = gpu ]; then
  # Each WGSL module of the examples and test/wasm loops compiles, with one
  # compute pipeline for each entry. A module with no entry prints
  # `valid 0 entries`.
  # Ruling Q-S10-9 (cairn-lang Q-S10-r13): each line of test/wgsl/internal.txt
  # is `MODULE ENTRY`, an entry whose pipeline gives GPUInternalError (a Metal
  # backend compile limit). A listed module is known when its error lines are
  # its listed entries and it has no other line. A new error or a listed entry
  # that compiles is a FAIL.
  wgsl_valid=0
  known_mods=0
  known_entries=0
  for g in "$tmp"/ex/*.wgsl "$tmp"/tw/*.wgsl; do
    [ -f "$g" ] || continue
    case "$g" in
      "$tmp"/ex/*) f="examples/$(basename "$g" .wgsl).lang" ;;
      *) f="test/wasm/$(basename "$g" .wgsl).lang" ;;
    esac
    awk -v f="$f" '$1 == f {print $2}' test/wgsl/internal.txt | sort >"$tmp/want.txt"
    if build/wgsltool --valid "$g" >"$tmp/v.out" 2>"$tmp/w.err" && rg -q -x 'valid [0-9]+ entries' "$tmp/v.out"; then
      if [ -s "$tmp/want.txt" ]; then
        echo "FAIL wgsl known $f: valid, but listed"
        fail=$((fail + 1))
      else
        wgsl_valid=$((wgsl_valid + 1))
      fi
    else
      { rg -o -r '$1' -x 'error (\w+) pipeline GPUInternalError' "$tmp/v.out" || true; } | sort >"$tmp/got.txt"
      other=$(rg -v -c -x 'error \w+ pipeline GPUInternalError|invalid [0-9]+ errors' "$tmp/v.out" || echo 0)
      if [ -s "$tmp/want.txt" ] && cmp -s "$tmp/want.txt" "$tmp/got.txt" && [ "$other" -eq 0 ]; then
        known_mods=$((known_mods + 1))
        known_entries=$((known_entries + $(rg -c '.' "$tmp/got.txt")))
      else
        echo "FAIL wgsl valid $g: $(head -n 1 "$tmp/v.out") $(head -n 1 "$tmp/w.err"), unlisted: $(comm -13 "$tmp/want.txt" "$tmp/got.txt" | tr '\n' ' ')"
        fail=$((fail + 1))
      fi
    fi
  done
  echo "wgsl valid: $wgsl_valid modules"
  echo "wgsl known internal: $known_mods modules, $known_entries entries"
  # The differential rows on the WGSL module must print the same OUTPUT. A file
  # with no module in $tmp/ex (the examples loop writes none) has no WGSL rows.
  wgsl_rows=0
  wgsl_bad=0
  wgsl_skip=0
  for in_file in "$tmp"/wd/*.in; do
    file=$(basename "$in_file" .in)
    [ -f "$tmp/ex/$file.wgsl" ] || { wgsl_skip=$((wgsl_skip + 1)); continue; }
    k=$(kit examples/kit.txt "$file.lang")
    if ! build/langc $k build "examples/$file.lang" --wgsl "$tmp/wd/$file.wgsl" 2>"$tmp/w.err" ||
      ! build/wgsltool "$tmp/wd/$file.wgsl" "$tmp/wd/$file.abi" <"$in_file" >"$tmp/wd/$file.gpu" 2>>"$tmp/w.err"; then
      echo "FAIL wgsltool $file: $(head -n 1 "$tmp/w.err")"
      fail=$((fail + 1))
      continue
    fi
    wgsl_rows=$((wgsl_rows + $(rg -c '.' "$in_file" || echo 0)))
    paste -d '|' "$in_file" "$tmp/wd/$file.want" "$tmp/wd/$file.gpu" | awk -F '|' '$2 != $3' >"$tmp/bad.txt"
    bad=$(rg -c '.' "$tmp/bad.txt" || echo 0)
    wgsl_bad=$((wgsl_bad + bad))
    [ "$bad" -eq 0 ] || { echo "FAIL wgsl differential $file: $bad mismatches, first $(head -n 1 "$tmp/bad.txt")"; fail=$((fail + 1)); }
  done
  echo "wgsl differential: $wgsl_rows rows, $wgsl_bad mismatches, $wgsl_skip files with no WGSL module"
  # The test/wasm tables of the Makefile regressions on the WGSL module.
  for t in hash secp traps edge; do
    if ! build/langc --kit build "test/wasm/$t.lang" --wgsl "$tmp/tw/$t.wgsl" 2>"$tmp/w.err" ||
      ! build/langc --kit abi "test/wasm/$t.lang" --text >"$tmp/tw/$t.abi" 2>>"$tmp/w.err"; then
      echo "FAIL wgsl regressions $t: $(head -n 1 "$tmp/w.err")"
      fail=$((fail + 1))
      continue
    fi
    awk -F ' => ' '/^[^#]/ {print $1}' "test/wasm/$t.txt" | build/wgsltool "$tmp/tw/$t.wgsl" "$tmp/tw/$t.abi" >"$tmp/tw/$t.gpu" 2>"$tmp/w.err" || :
    if awk -F ' => ' '/^[^#]/ {print $2}' "test/wasm/$t.txt" | cmp -s - "$tmp/tw/$t.gpu"; then
      echo "wgsl regressions $t: passed"
    else
      echo "FAIL wgsl regressions $t: the wgsltool output of test/wasm/$t.lang is not the RESULT text of test/wasm/$t.txt"
      fail=$((fail + 1))
    fi
  done
  # The test/ir rows on the WGSL module.
  ir_wgsl_rows=0
  ir_wgsl_bad=0
  for m in comb lazy spine proof-trap memname; do
    [ -f "$tmp/ir/$m.in" ] || continue
    if ! build/langc --kit build "test/ir/$m.lang" --wgsl "$tmp/ir/$m.wgsl" 2>"$tmp/w.err" ||
      ! build/wgsltool "$tmp/ir/$m.wgsl" "$tmp/ir/$m.abi" <"$tmp/ir/$m.in" >"$tmp/ir/$m.gpu" 2>>"$tmp/w.err"; then
      echo "FAIL wgsltool test/ir/$m.lang: $(head -n 1 "$tmp/w.err")"
      fail=$((fail + 1))
      continue
    fi
    ir_wgsl_rows=$((ir_wgsl_rows + $(rg -c '.' "$tmp/ir/$m.in" || echo 0)))
    paste -d '|' "$tmp/ir/$m.in" "$tmp/ir/$m.want" "$tmp/ir/$m.gpu" | awk -F '|' '$2 != $3' >"$tmp/bad.txt"
    bad=$(rg -c '.' "$tmp/bad.txt" || echo 0)
    ir_wgsl_bad=$((ir_wgsl_bad + bad))
    [ "$bad" -eq 0 ] || { echo "FAIL test/ir wgsl $m: $bad mismatches, first $(head -n 1 "$tmp/bad.txt")"; fail=$((fail + 1)); }
  done
  echo "test/ir wgsl: $ir_wgsl_rows rows, $ir_wgsl_bad mismatches"
  # The stored rows of test/wgsl/batch.txt (accrue S10 P4, from cairn-lang
  # S10 B4b, cairn-lang Q-S10-r9 and Q-S10-r11; accrue Q-S10-10). A ballot is
  # 3 entries: sigOk, or0Ok and or1Ok. The wasm module
  # gives the want text and the WGSL module must give the same text. The or1Ok
  # row of ballot 37 and chain link 700 are bad: each gives 0. Each other row
  # gives 1.
  if build/langc --kit build test/wgsl/batch.lang -o "$tmp/proof.wasm" 2>"$tmp/w.err" &&
    build/langc --kit build test/wgsl/batch.lang --wgsl "$tmp/proof.wgsl" 2>>"$tmp/w.err" &&
    build/langc --kit abi test/wgsl/batch.lang --text >"$tmp/proof.abi" 2>>"$tmp/w.err"; then
    for n in sigOk or0Ok or1Ok linkOk; do
      rg "^$n " test/wgsl/batch.txt >"$tmp/$n.in" || :
      build/wasmtool "$tmp/proof.wasm" "$tmp/proof.abi" <"$tmp/$n.in" >"$tmp/$n.want" 2>"$tmp/w.err" || :
      build/wgsltool "$tmp/proof.wgsl" "$tmp/proof.abi" <"$tmp/$n.in" >"$tmp/$n.gpu" 2>>"$tmp/w.err" || :
      paste -d '|' "$tmp/$n.in" "$tmp/$n.want" "$tmp/$n.gpu" | awk -F '|' '$2 != $3' >"$tmp/bad.txt"
      echo "$(rg -c '.' "$tmp/$n.in" || echo 0) $(rg -c '.' "$tmp/bad.txt" || echo 0) $(rg -c -x 1 "$tmp/$n.gpu" || echo 0) $(rg -n -x 0 "$tmp/$n.gpu" | cut -d: -f1 | tr '\n' ' ')" >"$tmp/$n.sum"
    done
    read -r s_rows s_bad s_ones s_zero <"$tmp/sigOk.sum"
    read -r o0_rows o0_bad o0_ones o0_zero <"$tmp/or0Ok.sum"
    read -r o1_rows o1_bad o1_ones o1_zero <"$tmp/or1Ok.sum"
    read -r l_rows l_bad l_ones l_zero <"$tmp/linkOk.sum"
    b_rows=$((s_rows + o0_rows + o1_rows))
    b_bad=$((s_bad + o0_bad + o1_bad))
    b_ones=$((s_ones + o0_ones + o1_ones))
    if [ "$b_rows $b_bad $b_ones $s_zero|$o0_zero|$o1_zero" = "192 0 191 ||37" ]; then
      echo "wgsl proofs: $b_rows rows, $b_bad mismatches"
    else
      echo "FAIL wgsl proofs: $b_rows rows, $b_bad mismatches, $b_ones rows give 1, 0 at sigOk rows $s_zero, or0Ok rows $o0_zero, or1Ok rows $o1_zero (want 192 0 191, 0 at or1Ok row 37)"
      fail=$((fail + 1))
    fi
    if [ "$l_rows $l_bad $l_ones $l_zero" = "1024 0 1023 700" ]; then
      echo "wgsl chain: $l_rows links, 1 bad link found"
    else
      echo "FAIL wgsl chain: $l_rows links, $l_bad mismatches, $l_ones links give 1, 0 at links $l_zero (want 1024 0 1023 700)"
      fail=$((fail + 1))
    fi
  else
    echo "FAIL wgsl proofs: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
  fi
  # The zk ring rows of test/wgsl/zk.txt (accrue S10 P5, Q-S10-11 (a) and
  # Q-S10-12 (a)): a bLSAG ring of 2 keys, one link in 5 split entries. The
  # wasm module gives the want text and the WGSL module must give the same
  # text. The chain check reads the WGSL text: the L and R words of each
  # zkStep row are the zkLx, zkLy, zkRx and zkRy results of its link, the c
  # word of link 2 is the zkStep result of link 1, link 2 gives c1 again, the
  # bad link 2 (s2 + 1) does not, a count of 3 or 1 gives none, and zkDig
  # gives the D word of the zkStep rows.
  if build/langc --kit build test/wgsl/zk.lang -o "$tmp/zk.wasm" 2>"$tmp/w.err" &&
    build/langc --kit build test/wgsl/zk.lang --wgsl "$tmp/zk.wgsl" 2>>"$tmp/w.err" &&
    build/langc --kit abi test/wgsl/zk.lang --text >"$tmp/zk.abi" 2>>"$tmp/w.err"; then
    z_rows=0
    z_bad=0
    for n in zkLx zkLy zkRx zkRy zkStep zkDig; do
      rg "^$n " test/wgsl/zk.txt >"$tmp/$n.in" || :
      build/wasmtool "$tmp/zk.wasm" "$tmp/zk.abi" <"$tmp/$n.in" >"$tmp/$n.want" 2>"$tmp/w.err" || :
      build/wgsltool "$tmp/zk.wgsl" "$tmp/zk.abi" <"$tmp/$n.in" >"$tmp/$n.gpu" 2>>"$tmp/w.err" || :
      paste -d '|' "$tmp/$n.in" "$tmp/$n.want" "$tmp/$n.gpu" | awk -F '|' '$2 != $3' >"$tmp/bad.txt"
      z_rows=$((z_rows + $(rg -c '.' "$tmp/$n.in" || echo 0)))
      z_bad=$((z_bad + $(rg -c '.' "$tmp/bad.txt" || echo 0)))
    done
    z_none=$(rg -c -x none "$tmp/zkStep.gpu" || echo 0)
    z_chain=$(awk '
      FILENAME == ARGV[1] { lx[FNR] = $1 }
      FILENAME == ARGV[2] { ly[FNR] = $1 }
      FILENAME == ARGV[3] { rx[FNR] = $1 }
      FILENAME == ARGV[4] { ry[FNR] = $1 }
      FILENAME == ARGV[5] { lc[FNR] = $3 }
      FILENAME == ARGV[6] { rc[FNR] = $3 }
      FILENAME == ARGV[7] { lr[FNR] = $8 " " $9 " " $10 " " $11; d[FNR] = $3 }
      FILENAME == ARGV[8] { c[FNR] = $1 }
      FILENAME == ARGV[9] { dig[FNR] = $1 }
      END {
        ok = (c[1] == lc[2] && c[1] == lc[3] && c[2] == lc[1] && c[3] != lc[1] && c[4] == "none" && c[5] == "none" && dig[1] == d[1])
        for (i = 1; i <= 3; i++) if (lr[i] != lx[i] " " ly[i] " " rx[i] " " ry[i] || rc[i] != lc[i]) ok = 0
        print (ok ? "chain ok" : "chain bad")
      }' "$tmp/zkLx.gpu" "$tmp/zkLy.gpu" "$tmp/zkRx.gpu" "$tmp/zkRy.gpu" "$tmp/zkLx.in" "$tmp/zkRx.in" "$tmp/zkStep.in" "$tmp/zkStep.gpu" "$tmp/zkDig.gpu")
    if [ "$z_rows $z_bad $z_none $z_chain" = "18 0 2 chain ok" ]; then
      echo "wgsl zk: $z_rows rows, $z_bad mismatches, $z_none none rows, $z_chain"
    else
      echo "FAIL wgsl zk: $z_rows rows, $z_bad mismatches, $z_none none rows, $z_chain (want 18 0 2 chain ok)"
      fail=$((fail + 1))
    fi
  else
    echo "FAIL wgsl zk: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
  fi
  # The earned rows of test/wgsl/earned.txt (accrue S10 P6, Q-S10-3 (a)):
  # earnedAt in the flat entry earnedFlat, the hA history of
  # examples/earned.lang at 9 times, an overflow row and a zero divisor row.
  # The wasm module gives the want text and the WGSL module must give the
  # same text. The WGSL words must also be the earnedAt values (the eval of
  # examples/earned.lang: ea9 to ea70 in test/eval/expect.txt, then 2 none).
  if build/langc --kit build test/wgsl/earned.lang -o "$tmp/earned.wasm" 2>"$tmp/w.err" &&
    build/langc --kit build test/wgsl/earned.lang --wgsl "$tmp/earned.wgsl" 2>>"$tmp/w.err" &&
    build/langc --kit abi test/wgsl/earned.lang --text >"$tmp/earned.abi" 2>>"$tmp/w.err"; then
    rg '^earnedFlat ' test/wgsl/earned.txt >"$tmp/earned.in" || :
    build/wasmtool "$tmp/earned.wasm" "$tmp/earned.abi" <"$tmp/earned.in" >"$tmp/earned.want" 2>"$tmp/w.err" || :
    build/wgsltool "$tmp/earned.wgsl" "$tmp/earned.abi" <"$tmp/earned.in" >"$tmp/earned.gpu" 2>>"$tmp/w.err" || :
    paste -d '|' "$tmp/earned.in" "$tmp/earned.want" "$tmp/earned.gpu" | awk -F '|' '$2 != $3' >"$tmp/bad.txt"
    e_rows=$(rg -c '.' "$tmp/earned.in" || echo 0)
    e_bad=$(rg -c '.' "$tmp/bad.txt" || echo 0)
    e_none=$(rg -c -x none "$tmp/earned.gpu" || echo 0)
    e_vals=bad
    if [ "$(tr '\n' ' ' <"$tmp/earned.gpu")" = "0u 50u 54u 72u 80u 94u 109u 149u 149u none none " ]; then
      e_vals=ok
    fi
    if [ "$e_rows $e_bad $e_none $e_vals" = "11 0 2 ok" ]; then
      echo "wgsl earned: $e_rows rows, $e_bad mismatches, $e_none none rows, values $e_vals"
    else
      echo "FAIL wgsl earned: $e_rows rows, $e_bad mismatches, $e_none none rows, values $e_vals (want 11 0 2 ok)"
      fail=$((fail + 1))
    fi
  else
    echo "FAIL wgsl earned: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
  fi
  # The batch rows (Q-S10-8 (a)): the first row of test/wasm/hash.txt, 192
  # times and 1024 times in one run (more than 64 calls in one dispatch). The
  # wasm module gives the want text and the WGSL module must give the same text.
  row=$(awk -F ' => ' '/^[^#]/ {print $1; exit}' test/wasm/hash.txt)
  if build/langc --kit build test/wasm/hash.lang -o "$tmp/batch.wasm" 2>"$tmp/w.err" &&
    build/langc --kit build test/wasm/hash.lang --wgsl "$tmp/batch.wgsl" 2>>"$tmp/w.err" &&
    build/langc --kit abi test/wasm/hash.lang --text >"$tmp/batch.abi" 2>>"$tmp/w.err"; then
    for n in 192 1024; do
      awk -v n="$n" -v row="$row" 'BEGIN { for (i = 0; i < n; i++) print row }' >"$tmp/batch-$n.in"
      build/wasmtool "$tmp/batch.wasm" "$tmp/batch.abi" <"$tmp/batch-$n.in" >"$tmp/batch-$n.want" 2>"$tmp/w.err" || :
      build/wgsltool "$tmp/batch.wgsl" "$tmp/batch.abi" <"$tmp/batch-$n.in" >"$tmp/batch-$n.gpu" 2>>"$tmp/w.err" || :
      want_rows=$(rg -c '.' "$tmp/batch-$n.want" || echo 0)
      if [ "$want_rows" -eq "$n" ] && cmp -s "$tmp/batch-$n.want" "$tmp/batch-$n.gpu"; then
        echo "wgsl batch $n: $n rows, 0 mismatches"
      else
        echo "FAIL wgsl batch $n: $want_rows wasm rows, $(paste -d '|' "$tmp/batch-$n.want" "$tmp/batch-$n.gpu" | awk -F '|' '$1 != $2' | rg -c '.' || echo 0) mismatches, first gpu line $(head -n 1 "$tmp/batch-$n.gpu")"
        fail=$((fail + 1))
      fi
    done
  else
    echo "FAIL wgsl batch: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
  fi
fi

rm -rf "$tmp"

if rg -n '\x{2013}|\x{2014}' . >/dev/null; then
  echo "FAIL: an em-dash or en-dash is in the kit"
  fail=$((fail + 1))
fi
echo "gate: $fail failures"
[ "$fail" -eq 0 ]
