/* The langc command line. SHARED by the tcc kits.
   Exit 0: ok. Exit 1: the program is refused. Exit 2: usage or IO. */
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "front/check.h"
#include "front/fmt.h"
#include "front/front.h"
#include "front/lexer.h"
#include "front/lower.h"
#include "front/parser.h"
#include "js.h"
#include "json.h"
#include "target.h"
#include "wgsl.h"

#define ARENA_LIMIT_BYTES ((size_t)1 << 30)

typedef enum {
  CMD_CHECK,
  CMD_EVAL,
  CMD_RUN,
  CMD_IR,
  CMD_ABI,
  CMD_BUILD,
  CMD_PROVE,
  CMD_FMT
} Command;

typedef struct {
  Command command;
  const char *prog_path;
  const char *entry;
  char **args;
  int arg_count;
  const char *script_path;
  const char *out_path;
  const char *js_path; /* accrue S9: build --js OUT.mjs writes the ES module (src/js.c) */
  const char *wgsl_path; /* accrue S10: build --wgsl OUT.wgsl writes the WGSL module (src/wgsl.c) */
  int runtime;
  int json; /* abi: the JSON form (S9 P5); check: --json (S12 A2) */
  int fmt_check; /* fmt --check (S11 A1): compare PROG with its canonical text */
  CheckMode mode; /* orbit-lang S3: --kit selects the kit mode */
} Options;

static const struct {
  const char *word;
  Command command;
} COMMANDS[] = {
  {"check", CMD_CHECK},
  {"eval", CMD_EVAL},
  {"run", CMD_RUN},
  {"ir", CMD_IR},
  {"abi", CMD_ABI},
  {"build", CMD_BUILD},
  {"prove", CMD_PROVE},
  {"fmt", CMD_FMT},
};

static int usage(FILE *err) {
  fputs("usage: langc [--kit] check [--json] PROG\n"
        "       langc [--kit] eval PROG NAME [ARGS...]\n"
        "       langc [--kit] run PROG SCRIPT\n"
        "       langc [--kit] ir PROG\n"
        "       langc [--kit] abi PROG [--text]\n"
        "       langc [--kit] build PROG -o OUT [--runtime]\n"
        "       langc [--kit] build PROG --js OUT.mjs\n"
        "       langc [--kit] build PROG --wgsl OUT.wgsl\n"
        "       langc [--kit] prove PROG NAME [POLL]\n"
        "       langc fmt PROG [--check]\n"
        "--kit: the kit mode, the trusted test scope of the kit gate\n", err);
  return 2;
}

static int find_command(const char *word, Command *out) {
  for (size_t i = 0; i < sizeof COMMANDS / sizeof COMMANDS[0]; i++) {
    if (strcmp(COMMANDS[i].word, word) == 0) {
      *out = COMMANDS[i].command;
      return 1;
    }
  }
  return 0;
}

static int parse_build_options(int argc, char **argv, Options *opt, Diag *diag) {
  for (int i = 3; i < argc; i++) {
    int is_out = strcmp(argv[i], "-o") == 0 && i + 1 < argc;
    int is_js = strcmp(argv[i], "--js") == 0 && i + 1 < argc;
    int is_wgsl = strcmp(argv[i], "--wgsl") == 0 && i + 1 < argc;
    int is_runtime = strcmp(argv[i], "--runtime") == 0;
    if (!is_out && !is_js && !is_wgsl && !is_runtime) return diag_fail(diag, "USAGE", NULL, "unknown build argument '%s'", argv[i]);
    opt->out_path = is_out ? argv[i + 1] : opt->out_path;
    opt->js_path = is_js ? argv[i + 1] : opt->js_path;
    opt->wgsl_path = is_wgsl ? argv[i + 1] : opt->wgsl_path;
    opt->runtime |= is_runtime;
    i += is_out | is_js | is_wgsl;
  }
  /* Q-S8-8: build writes a file, so -o OUT, --js OUT.mjs (accrue S9) or
     --wgsl OUT.wgsl (accrue S10) is required, and only one of them. */
  int outs = (opt->out_path != NULL) + (opt->js_path != NULL) + (opt->wgsl_path != NULL);
  if (outs > 1 && opt->wgsl_path != NULL) {
    return diag_fail(diag, "USAGE", NULL, "build takes one of -o OUT, --js OUT.mjs and --wgsl OUT.wgsl");
  }
  if (outs > 1) {
    return diag_fail(diag, "USAGE", NULL, "build takes -o OUT or --js OUT.mjs, not both");
  }
  return outs == 1 ? 1 : diag_fail(diag, "USAGE", NULL, "build needs -o OUT, --js OUT.mjs or --wgsl OUT.wgsl");
}

static int parse_options(int argc, char **argv, Options *opt, Diag *diag) {
  memset(opt, 0, sizeof *opt);
  if (argc < 3 || !find_command(argv[1], &opt->command)) {
    return diag_fail(diag, "USAGE", NULL, "expected a command and a program path");
  }
  opt->prog_path = argv[2];
  switch (opt->command) {
    case CMD_CHECK:
      /* check --json PROG (S12 A2, the cairn-lang 98d18ef form): the
         json_check line. */
      opt->json = strcmp(argv[2], "--json") == 0;
      if (opt->json && argc < 4) return diag_fail(diag, "USAGE", NULL, "check --json needs a program path");
      opt->prog_path = argv[2 + opt->json];
      return argc == 3 + opt->json ? 1 : diag_fail(diag, "USAGE", NULL, "too many arguments");
    case CMD_IR:
      return argc == 3 ? 1 : diag_fail(diag, "USAGE", NULL, "too many arguments");
    case CMD_ABI:
      /* The JSON ABI is the default; --text gives the C-K4-16 lines (C-c7-1). */
      opt->json = argc == 3;
      return argc == 3 || (argc == 4 && strcmp(argv[3], "--text") == 0) ? 1
        : diag_fail(diag, "USAGE", NULL, "abi takes only --text after the program");
    case CMD_EVAL:
      if (argc < 4) return diag_fail(diag, "USAGE", NULL, "eval needs a definition name");
      opt->entry = argv[3];
      opt->args = argv + 4;
      opt->arg_count = argc - 4;
      return 1;
    case CMD_RUN:
      if (argc != 4) return diag_fail(diag, "USAGE", NULL, "run needs a program and a script");
      opt->script_path = argv[3];
      return 1;
    case CMD_BUILD:
      return parse_build_options(argc, argv, opt, diag);
    case CMD_PROVE:
      if (argc < 4) return diag_fail(diag, "USAGE", NULL, "prove needs a definition name");
      if (argc > 5) return diag_fail(diag, "USAGE", NULL, "prove takes only a poll id after the name");
      opt->entry = argv[3];
      opt->args = argv + 4;
      opt->arg_count = argc - 4;
      return 1;
    case CMD_FMT:
      opt->fmt_check = argc == 4;
      return argc == 3 || (argc == 4 && strcmp(argv[3], "--check") == 0) ? 1
        : diag_fail(diag, "USAGE", NULL, "fmt takes only --check after the program");
  }
  return 0;
}

static int read_source(Arena *arena, const char *path, char **out, size_t *len, Diag *diag) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) return diag_fail(diag, "IO", NULL, "cannot open %s", path);
  char *buf = arena_alloc(arena, SOURCE_MAX_BYTES + 1);
  size_t count = buf == NULL ? 0 : fread(buf, 1, SOURCE_MAX_BYTES + 1, file);
  int read_error = ferror(file);
  fclose(file);
  if (buf == NULL) return diag_fail(diag, "OOM", NULL, "no memory for %s", path);
  if (read_error) return diag_fail(diag, "IO", NULL, "cannot read %s", path);
  if (count > SOURCE_MAX_BYTES) return diag_fail(diag, "IO_SIZE", NULL, "%s is larger than %zu bytes", path, SOURCE_MAX_BYTES);
  *out = buf;
  *len = count;
  return 1;
}

static const char *command_word(Command command) {
  switch (command) {
    case CMD_CHECK: return "check";
    case CMD_EVAL: return "eval";
    case CMD_RUN: return "run";
    case CMD_IR: return "ir";
    case CMD_ABI: return "abi";
    case CMD_BUILD: return "build";
    case CMD_PROVE: return "prove";
    case CMD_FMT: return "fmt";
  }
  return "?";
}

/* The ABI comes from checked types, independently of lowering. An omitted
   IR function must not disappear from both sides of the gate's count check.
   An Option-result entry has the word `option` before its result kind
   (Q-S8-2 (c)). JSON (src/json.c) is the default form; --text gives one
   line for each entry (target_abi_line). */
static int print_abi(Machine *machine, int json) {
  uint32_t i;
  uint32_t k;
  uint32_t count = 0;
  Entry entry;
  IrFunc fn;
  IrScalar params[ENTRY_PARAMS_MAX];
  memset(&fn, 0, sizeof fn);
  fn.params = params;
  if (json) fputs("{\"entries\":[", stdout);
  for (i = 0; i < machine->def_count; i++) {
    machine->fuel = EVAL_FUEL_STEPS;
    if (machine->defs[i].origin != ORIGIN_PROGRAM || !entry_of(machine, machine->defs[i].type, &entry))
      continue;
    fn.name = machine->defs[i].name;
    fn.param_count = entry.param_count;
    for (k = 0; k < entry.param_count; k++)
      params[k] = entry.param_flag[k] ? IR_SCALAR_FLAG : entry.param_u256[k] ? IR_SCALAR_U256 : IR_SCALAR_NAT;
    fn.result = entry.result_flag ? IR_SCALAR_FLAG : entry.result_u256 ? IR_SCALAR_U256 : IR_SCALAR_NAT;
    fn.result_option = entry.result_option;
    if (!(json ? json_abi_entry(&fn, count++ == 0, stdout) : target_abi_line(&fn, stdout, stderr)))
      return 1;
  }
  if (json) fputs("]}\n", stdout);
  return machine->diag->set ? 1 : 0;
}

/* Lowers the entries, then runs the ir or build command (the kit target). */
static int run_target(const Options *opt, Machine *machine, Diag *diag) {
  IrProgram prog;
  FILE *out;
  const char *path = opt->wgsl_path != NULL ? opt->wgsl_path : opt->js_path != NULL ? opt->js_path : opt->out_path;
  int ok = 1;
  if (opt->command == CMD_ABI) return print_abi(machine, opt->json);
  if (!lower_program(machine, &prog, opt->js_path != NULL || opt->wgsl_path != NULL)) return 1;
  switch (opt->command) {
    case CMD_IR:
      ir_print(&prog, stdout);
      return 0;
    case CMD_ABI:
    case CMD_CHECK:
    case CMD_EVAL:
    case CMD_RUN:
    case CMD_PROVE:
    case CMD_BUILD:
    case CMD_FMT:
      break;
  }
  out = fopen(path, "wb");
  if (out == NULL) return diag_fail(diag, "IO", NULL, "cannot write '%s'", path) + 2;
  /* accrue S9: --js writes the ES module. The JS heap takes the Wasm limit,
     so both targets refuse the same programs (REFUSE_CAPACITY). accrue S10:
     --wgsl writes the WGSL module, with the same limit (S10 P2). */
  ok = opt->wgsl_path != NULL ? target_capacity(&prog, stderr) && wgsl_write(&prog, out, stderr)
       : opt->js_path != NULL ? target_capacity(&prog, stderr) && js_write(&prog, out, stderr)
                              : target_write(&prog, opt->runtime ? TARGET_PART_RUNTIME : TARGET_PART_MAIN, out, stderr);
  ok = fclose(out) == 0 && ok;
  if (ok) return 0;
  remove(path);
  diag_fail(diag, "BUILD", NULL, "no module written; '%s' removed", path);
  return 1;
}

/* langc [--kit] check --json PROG (S12 A2): exit 0 or 1 with the json_check
   line on stdout, and main prints no diag. After a lex or parse error the
   progress stays all zero. OOM is exit 2 with no JSON; an IO or USAGE
   failure exits 2 before this function. */
static int check_json(const Options *opt, Arena *arena, const char *text, size_t len, Diag *diag) {
  CheckProgress progress;
  DeclList decls;
  Machine machine;
  memset(&progress, 0, sizeof progress);
  int ok = front_load(arena, opt->prog_path, text, len, &decls, diag)
      && check_program_progress(arena, &decls, &machine, diag, opt->mode, &progress);
  if (!ok && diag->code != NULL && strcmp(diag->code, "OOM") == 0) return 2;
  json_check(stdout, opt->prog_path, diag, &progress);
  return ok ? 0 : 1;
}

static int run(const Options *opt, Arena *arena, Diag *diag) {
  char *text = NULL;
  size_t len = 0;
  if (!read_source(arena, opt->prog_path, &text, &len, diag)) return 2;
  if (opt->command == CMD_CHECK && opt->json) return check_json(opt, arena, text, len, diag);
  DeclList decls;
  if (!front_load(arena, opt->prog_path, text, len, &decls, diag)) return 1;
  Machine machine;
  if (!check_program(arena, &decls, &machine, diag, opt->mode)) return 1;
  switch (opt->command) {
    case CMD_CHECK:
      puts("ok");
      return 0;
    case CMD_EVAL:
      return eval_command(&machine, opt->entry, opt->args, opt->arg_count, stdout, stderr);
    case CMD_RUN: {
      char *script = NULL;
      size_t script_len = 0;
      if (!read_source(arena, opt->script_path, &script, &script_len, diag)) return 2;
      return run_command(&machine, opt->script_path, script, script_len, stdout);
    }
    case CMD_PROVE:
      signal(SIGPIPE, SIG_IGN);
      return prove_command(&machine, opt->entry, opt->arg_count == 1 ? opt->args[0] : NULL, stdout, stderr);
    case CMD_ABI:
    case CMD_IR:
    case CMD_BUILD:
      return run_target(opt, &machine, diag);
    case CMD_FMT: /* main runs run_fmt */
      break;
  }
  diag_fail(diag, "INTERNAL", NULL, "no handler for the %s command", command_word(opt->command));
  return 1;
}

/* The line of the first byte where TEXT and the canonical text CANON
   differ (1 + the newlines in the common prefix), or 0 when they are equal. */
static size_t first_diff_line(const char *text, size_t len, const FmtText *canon) {
  size_t same = 0;
  size_t line = 1;
  for (; same < len && same < canon->len && text[same] == canon->data[same]; same++) line += text[same] == '\n' ? 1u : 0u;
  return same == len && same == canon->len ? 0 : line;
}

/* langc fmt (S11 A1, ported from cairn-lang): lexes and parses the
   program only (no domain, no check), then prints the canonical text. With
   --check it prints nothing when the file is canonical, else
   `PROG:LINE: not canonical` at the first line that differs (exit 1). */
static int run_fmt(const Options *opt, Arena *arena, Diag *diag) {
  char *text = NULL;
  size_t len = 0;
  if (!read_source(arena, opt->prog_path, &text, &len, diag)) return 2;
  TokenList toks;
  DeclList decls = {NULL, 0, 0};
  if (!lex_source(arena, opt->prog_path, text, len, &toks, diag)) return 1;
  if (!parse_source(arena, opt->prog_path, ORIGIN_PROGRAM, &toks, &decls, diag)) return 1;
  FmtText out;
  fmt_text_init(&out, arena);
  if (!fmt_program(&out, decls.items, decls.count, &toks)) {
    return diag_fail(diag, "OOM", NULL, "no memory for the canonical text of %s", opt->prog_path) + 2;
  }
  if (!fmt_validate(&out, opt->prog_path, diag)) return strcmp(diag->code, "OOM") == 0 ? 2 : 1;
  if (opt->fmt_check) {
    size_t line = first_diff_line(text, len, &out);
    if (line != 0) fprintf(stderr, "%s:%zu: not canonical\n", opt->prog_path, line);
    return line != 0;
  }
  size_t written = out.len == 0 ? 0 : fwrite(out.data, 1, out.len, stdout);
  if (written != out.len || fflush(stdout) != 0) return diag_fail(diag, "IO", NULL, "cannot write the canonical text") + 2;
  return 0;
}

int main(int argc, char **argv) {
  Diag diag;
  diag_init(&diag);
  Options opt;
  int kit = argc > 1 && strcmp(argv[1], "--kit") == 0;
  if (!parse_options(argc - kit, argv + kit, &opt, &diag)) {
    diag_print(&diag, stderr);
    return usage(stderr);
  }
  opt.mode = kit ? CHECK_KIT : CHECK_GUEST;
  Arena arena;
  arena_init(&arena, ARENA_LIMIT_BYTES);
  int status = opt.command == CMD_FMT ? run_fmt(&opt, &arena, &diag) : run(&opt, &arena, &diag);
  arena_release(&arena);
  /* fmt --check prints its own `PROG:LINE: not canonical` line, with no diag;
     check --json puts the diag of exit 1 in its JSON line (S12 A2). */
  int in_json = opt.command == CMD_CHECK && opt.json && status == 1;
  if (status != 0 && !in_json && (opt.command != CMD_FMT || diag.set)) diag_print(&diag, stderr);
  return status;
}
