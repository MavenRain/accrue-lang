/* The table of `langc explain CODE` (S13 Q-S13-4 (a)), ported from
   evolve-lang src/codes.c (8c5d8e1) by cairn-lang 60768e8. Each row is a
   refusal code of docs/host/README.md, in the same order: the 14 rows of
   the Source language table, then the 5 bullets of Compilation. The
   meaning is the README text verbatim. */
#include <string.h>

#include "codes.h"

static const CodeRow CODE_ROWS[] = {
  {"REFUSE_PARTIAL", "Source language", "a partial name (for example `div`, `mod` or `sub`), the `Option` result of a checked kit word where a plain value is wanted, or the literal 0 where a `NonZero` is wanted"},
  {"REFUSE_DATA", "Source language", "a `family` declaration in a program"},
  {"REFUSE_AXIOM", "Source language", "an `axiom` declaration"},
  {"REFUSE_REC", "Source language", "`def rec`, or a definition that refers to itself or to a later definition"},
  {"REFUSE_CLOCK", "Source language", "a clock word (`now`, `block`, `timestamp`, `clock`) as a name or a use; the message says to take the time as a `Stamp` argument (PLAN section 5)"},
  {"REFUSE_NAME", "Source language", "a core name, a domain name, a reserved name or a repeated name as a new definition"},
  {"REFUSE_CELL", "Source language", "a type constructor that is not one cell signature or one cocell signature, a cons-list name in place of a cell monad, or a sealed family that is not a cell monad"},
  {"REFUSE_EVENT", "Source language", "an event with more fields than the entry parameter limit, an event field that is not an ABI word type (`Nat`, `Flag`, `U256`, `Addr`), or a repeated event field"},
  {"REFUSE_HISTORY_WRITE", "Source language", "a history field `h` that is not `(h t)`, `ADD (h t) e` or the variable of `bind (ADD (h t) e)`, with `ADD` as `u256Add` or `natAdd`, a history constructor without all of its arguments, or a definition with `State` in its type that refers to a name that the history rule does not check"},
  {"REFUSE_KIT", "Source language", "a kit item outside the base span (PLAN section 4); only the kit mode `langc --kit` admits it"},
  {"REFUSE_LAW", "Source language", "a law-only name (`decrypt`, `observe`, an elim, a fold or a projection of a sealed family) in a definition whose type is not a law (a `Path` or `Trunc1` codomain)"},
  {"REFUSE_MAP", "Source language", "a `Map` key type that is not `Nat`, `U256` or `Addr`, or a `Map` value type that is not `Nat`, `Flag`, `U256` or `Addr`"},
  {"REFUSE_SIG", "Source language", "a name of the design signature (PLAN section 3): a slot with the wrong type, a reserved name that no slice gives yet, or a defined or primitive name that the kernel gives"},
  {"REFUSE_STATE", "Source language", "a second state, a state type with a name other than `State`, a state with parameters, an index telescope or not one constructor, a state field that is not first-order, or a history field that is not `Nat` or `U256`"},
  {"REFUSE_RUNTIME_CLOSURE", "Compilation", "a function value must exist at run time."},
  {"REFUSE_RUNTIME_TREE", "Compilation", "a fold walks a tree that exists only at run time."},
  {"REFUSE_CAPACITY", "Compilation", "a list walk, an unfold limit or the heap of a loop has no static bound (S8 P3)."},
  {"REFUSE_LOWER", "Compilation", "a construct does not lower."},
  {"REFUSE_TARGET_SECRET", "Compilation", "on the ES or WGSL target, the operation touches a run-time `Secret` or a zk secret op (O4)."},
};

size_t code_count(void) { return sizeof CODE_ROWS / sizeof CODE_ROWS[0]; }

const CodeRow *code_at(size_t index) { return index < code_count() ? &CODE_ROWS[index] : NULL; }

const CodeRow *code_find(const char *code) {
  for (size_t i = 0; i < code_count(); i++) {
    if (strcmp(CODE_ROWS[i].code, code) == 0) return &CODE_ROWS[i];
  }
  return NULL;
}
