/* The no-trap checker (PLAN N4 and R0, Q-S8-4 (a)). It reads each Wasm
   module that langc builds and fails on each form that can trap:
   - unreachable, call_indirect, memory.grow, each float op (the trunc ops
     too), each 0xFC or 0xFD prefix op, a signed div or rem;
   - an unsigned div or rem whose divisor is not the idiom `local.get X;
     local.get X; eqz; [i64.extend_i32_u;] or`: that divisor is
     X | (X == 0), so it is never 0;
   - a load or store whose address has no bound: the address must come from
     `i32.const C`, or from `i32.and` with an `i32.const C` operand, and
     C + offset + width must be at most the memory size;
   - a memory with max != min (N3), a data segment past the memory, an
     element section, a call-graph cycle (N2).
   To find the address operand of a store, it keeps the stack height of each
   body as the validator does. It does not check the types: run it after
   wasm-validate.

   Usage: notraptool MODULE.wasm...
   Prints one FAIL line for each trap form, then
   `notrap: N modules checked, T trap forms`. Exit 0: T = 0. Exit 1: T > 0.
   Exit 2: usage, IO or a module that does not decode.
   notraptool --self-test: checks the bad modules in this file (byte arrays)
   and prints `NAME: REASON` for each (test/notrap/expect.txt). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TYPES 4096u
#define MAX_FUNCS 16384u
#define MAX_EDGES (1u << 20)
#define MAX_STACK 16384u
#define MAX_FRAMES 4096u
#define MAX_MODULE (256u << 20)
#define PAGE_BYTES 65536u

typedef enum { RUN_OK, RUN_TRAP, RUN_BAD } Run;

typedef struct {
  const uint8_t *p;
  size_t end;
  size_t at;
} Reader;

typedef struct {
  uint32_t params;
  uint32_t results;
} FuncType;

/* A stack value. bound = 1: the value is at most max (an i32 address). */
typedef struct {
  int bound;
  uint64_t max;
} Slot;

typedef struct {
  size_t base; /* the stack height under the block */
  uint32_t params;
  uint32_t results;
  int dead; /* 1: after br, br_table or return; the stack is polymorphic */
} Frame;

typedef struct {
  uint8_t op;
  uint32_t imm;
} Past;

typedef struct {
  FuncType types[MAX_TYPES];
  uint32_t type_count;
  uint32_t func_type[MAX_FUNCS];
  uint32_t func_count; /* the imported and the defined functions */
  uint32_t import_funcs;
  int has_memory;
  uint64_t memory_bytes;
  uint32_t edge_from[MAX_EDGES]; /* calls between defined functions (indices without the imports) */
  uint32_t edge_to[MAX_EDGES];
  size_t edge_count;
  uint32_t start[MAX_FUNCS + 1u];
  uint32_t fill[MAX_FUNCS];
  uint32_t in_degree[MAX_FUNCS];
  uint32_t queue[MAX_FUNCS];
  uint32_t next[MAX_EDGES];
  Slot stack[MAX_STACK];
  size_t height;
  Frame frames[MAX_FRAMES];
  size_t depth;
  Past past[5]; /* past[0] is the last instruction */
  const char *why;
  const char *file;
  int quiet;
  char reason[160]; /* the first trap form */
  unsigned long fails;
} Check;

static Check state;

static int get_byte(Reader *r, uint8_t *out) {
  if (r->at >= r->end) return 0;
  *out = r->p[r->at];
  r->at++;
  return 1;
}

static int skip_bytes(Reader *r, uint32_t n) {
  if (n > r->end - r->at) return 0;
  r->at += n;
  return 1;
}

/* An unsigned LEB128 of at most 32 bits. */
static int get_u32(Reader *r, uint32_t *out) {
  uint64_t value = 0;
  unsigned shift = 0;
  uint8_t b = 0x80;
  while ((b & 0x80) != 0) {
    if (shift > 28u || !get_byte(r, &b)) return 0;
    value |= (uint64_t)(b & 0x7F) << shift;
    shift += 7u;
  }
  if (value > 0xFFFFFFFFu) return 0;
  *out = (uint32_t)value;
  return 1;
}

/* A signed LEB128 of at most BITS bits (32, 33 or 64). */
static int get_signed(Reader *r, unsigned bits, int64_t *out) {
  uint64_t value = 0;
  unsigned shift = 0;
  uint8_t b = 0x80;
  while ((b & 0x80) != 0) {
    if (shift >= 7u * ((bits + 6u) / 7u) || !get_byte(r, &b)) return 0;
    value |= (uint64_t)(b & 0x7F) << shift;
    shift += 7u;
  }
  if (shift < 64u && (b & 0x40) != 0) value |= ~(uint64_t)0 << shift;
  *out = (int64_t)value;
  return 1;
}

static int skip_name(Reader *r) {
  uint32_t n;
  return get_u32(r, &n) && skip_bytes(r, n);
}

static int get_limits(Reader *r, uint32_t *min, uint32_t *max, int *has_max) {
  uint8_t flag;
  if (!get_byte(r, &flag) || flag > 1u || !get_u32(r, min)) return 0;
  *has_max = flag == 1u;
  *max = *min;
  return flag == 0u || get_u32(r, max);
}

static Run trap_form(Check *c, long func, size_t at, const char *why) {
  c->fails++;
  if (c->reason[0] == '\0') snprintf(c->reason, sizeof c->reason, "%s", why);
  if (c->quiet) return RUN_TRAP;
  if (func < 0)
    printf("FAIL notrap %s: %s\n", c->file, why);
  else
    printf("FAIL notrap %s: func %ld at byte %lu: %s\n", c->file, func, (unsigned long)at, why);
  return RUN_TRAP;
}

static Run trap(Check *c, const char *why) {
  c->why = why;
  return RUN_TRAP;
}

static Run ok_if(int ok) {
  if (ok) return RUN_OK;
  return RUN_BAD;
}

static int push(Check *c, int bound, uint64_t max) {
  if (c->height >= MAX_STACK) return 0;
  c->stack[c->height].bound = bound;
  c->stack[c->height].max = max;
  c->height++;
  return 1;
}

/* Below the block base the stack is polymorphic after br, br_table or
   return: that code never runs, so its value passes each check. */
static int pop(Check *c, Slot *out) {
  const Frame *f = &c->frames[c->depth - 1u];
  if (c->height > f->base) {
    c->height--;
    *out = c->stack[c->height];
    return 1;
  }
  out->bound = 1;
  out->max = 0;
  return f->dead;
}

static int effect(Check *c, uint32_t in, uint32_t out) {
  Slot s;
  uint32_t i;
  for (i = 0; i < in; i++)
    if (!pop(c, &s)) return 0;
  for (i = 0; i < out; i++)
    if (!push(c, 0, 0)) return 0;
  return 1;
}

static int enter(Check *c, uint32_t params, uint32_t results) {
  Frame *f;
  if (c->depth >= MAX_FRAMES || !effect(c, params, 0)) return 0;
  f = &c->frames[c->depth];
  f->base = c->height;
  f->params = params;
  f->results = results;
  f->dead = 0;
  c->depth++;
  return effect(c, 0, params);
}

static int otherwise(Check *c) {
  Frame *f = &c->frames[c->depth - 1u];
  c->height = f->base;
  f->dead = 0;
  return effect(c, 0, f->params);
}

static int leave(Check *c) {
  uint32_t results = c->frames[c->depth - 1u].results;
  c->height = c->frames[c->depth - 1u].base;
  c->depth--;
  return effect(c, 0, results);
}

static int stop(Check *c) {
  Frame *f = &c->frames[c->depth - 1u];
  c->height = f->base;
  f->dead = 1;
  return 1;
}

static int block_type(Check *c, Reader *r, uint32_t *params, uint32_t *results) {
  int64_t index;
  uint8_t b;
  *params = 0;
  *results = 0;
  if (r->at >= r->end) return 0;
  b = r->p[r->at];
  if (b == 0x40) return skip_bytes(r, 1);
  *results = 1;
  if (b == 0x7F || b == 0x7E || b == 0x7D || b == 0x7C || b == 0x7B || b == 0x70 || b == 0x6F) return skip_bytes(r, 1);
  if (!get_signed(r, 33, &index) || index < 0 || (uint64_t)index >= c->type_count) return 0;
  *params = c->types[index].params;
  *results = c->types[index].results;
  return 1;
}

static unsigned load_width(uint8_t op) {
  switch (op) {
    case 0x2C:
    case 0x2D:
    case 0x30:
    case 0x31:
      return 1;
    case 0x2E:
    case 0x2F:
    case 0x32:
    case 0x33:
      return 2;
    case 0x28:
    case 0x34:
    case 0x35:
      return 4;
    case 0x29:
      return 8;
    default:
      return 0;
  }
}

static unsigned store_width(uint8_t op) {
  switch (op) {
    case 0x3A:
    case 0x3C:
      return 1;
    case 0x3B:
    case 0x3D:
      return 2;
    case 0x36:
    case 0x3E:
      return 4;
    case 0x37:
      return 8;
    default:
      return 0;
  }
}

static int is_float(uint8_t op) {
  return op == 0x2A || op == 0x2B || op == 0x38 || op == 0x39 || op == 0x43 || op == 0x44 ||
         (op >= 0x5B && op <= 0x66) || (op >= 0x8B && op <= 0xA6) || (op >= 0xA8 && op <= 0xAB) ||
         (op >= 0xAE && op <= 0xBF);
}

/* The inputs of an integer op with one result: 1 or 2; 0 for any other op. */
static unsigned plain_inputs(uint8_t op) {
  if (op == 0x45 || op == 0x50 || (op >= 0x67 && op <= 0x69) || (op >= 0x79 && op <= 0x7B) || op == 0xA7 ||
      op == 0xAC || op == 0xAD || (op >= 0xC0 && op <= 0xC4))
    return 1;
  if ((op >= 0x46 && op <= 0x4F) || (op >= 0x51 && op <= 0x5A) || (op >= 0x6A && op <= 0x78) ||
      (op >= 0x7C && op <= 0x8A))
    return 2;
  return 0;
}

/* The divisor idiom: local.get X; local.get X; eqz; [i64.extend_i32_u;] or. */
static int idiom(const Check *c, int wide) {
  const Past *p = c->past;
  if (wide)
    return p[0].op == 0x84 && p[1].op == 0xAD && p[2].op == 0x50 && p[3].op == 0x20 && p[4].op == 0x20 &&
           p[3].imm == p[4].imm;
  return p[0].op == 0x72 && p[1].op == 0x45 && p[2].op == 0x20 && p[3].op == 0x20 && p[2].imm == p[3].imm;
}

/* x & y is at most each operand that has a bound. */
static uint64_t tighter(Slot a, Slot b) {
  if (a.bound && b.bound && b.max < a.max) return b.max;
  if (a.bound) return a.max;
  return b.max;
}

static Run access(Check *c, Reader *r, unsigned width, int store) {
  uint32_t align, offset;
  Slot value, address;
  if (!get_u32(r, &align) || !get_u32(r, &offset) || (store && !pop(c, &value)) || !pop(c, &address))
    return RUN_BAD;
  if (!address.bound) return trap(c, "a load or store address that is not i32.const C or i32.and with i32.const C");
  if (!c->has_memory || address.max + offset + width > c->memory_bytes)
    return trap(c, "a load or store past the memory (C + offset + width > the memory size)");
  return ok_if(store || push(c, 0, 0));
}

static int edge(Check *c, uint32_t from, uint32_t to) {
  if (to < c->import_funcs) return 1;
  if (c->edge_count >= MAX_EDGES) return 0;
  c->edge_from[c->edge_count] = from - c->import_funcs;
  c->edge_to[c->edge_count] = to - c->import_funcs;
  c->edge_count++;
  return 1;
}

static Run instruction(Check *c, Reader *r, uint8_t op, uint32_t *imm, uint32_t func) {
  Slot a, b;
  uint32_t params, results, count, k, label;
  unsigned width;
  int64_t value;
  uint8_t byte;
  const FuncType *t;
  switch (op) {
    case 0x00:
      return trap(c, "unreachable");
    case 0x01:
      return RUN_OK;
    case 0x02:
    case 0x03:
      return ok_if(block_type(c, r, &params, &results) && enter(c, params, results));
    case 0x04:
      return ok_if(pop(c, &a) && block_type(c, r, &params, &results) && enter(c, params, results));
    case 0x05:
      return ok_if(otherwise(c));
    case 0x0B:
      return ok_if(leave(c));
    case 0x0C:
      return ok_if(get_u32(r, imm) && stop(c));
    case 0x0D:
      return ok_if(get_u32(r, imm) && pop(c, &a));
    case 0x0E:
      if (!get_u32(r, &count)) return RUN_BAD;
      for (k = 0; k <= count; k++)
        if (!get_u32(r, &label)) return RUN_BAD;
      return ok_if(pop(c, &a) && stop(c));
    case 0x0F:
      return ok_if(stop(c));
    case 0x10:
      if (!get_u32(r, imm) || *imm >= c->func_count || !edge(c, func, *imm)) return RUN_BAD;
      t = &c->types[c->func_type[*imm]];
      return ok_if(effect(c, t->params, t->results));
    case 0x11:
      return trap(c, "call_indirect");
    case 0x1A:
      return ok_if(pop(c, &a));
    case 0x1B:
      return ok_if(effect(c, 3, 1));
    case 0x1C:
      if (!get_u32(r, &count)) return RUN_BAD;
      for (k = 0; k < count; k++)
        if (!get_byte(r, &byte)) return RUN_BAD;
      return ok_if(effect(c, 3, 1));
    case 0x20:
    case 0x23:
      return ok_if(get_u32(r, imm) && push(c, 0, 0));
    case 0x21:
    case 0x24:
      return ok_if(get_u32(r, imm) && pop(c, &a));
    case 0x22:
      return ok_if(get_u32(r, imm) && pop(c, &a) && push(c, a.bound, a.max));
    case 0x3F:
      return ok_if(get_byte(r, &byte) && push(c, 0, 0));
    case 0x40:
      return trap(c, "memory.grow");
    case 0x41:
      return ok_if(get_signed(r, 32, &value) && push(c, 1, (uint64_t)(uint32_t)value));
    case 0x42:
      return ok_if(get_signed(r, 64, &value) && push(c, 0, 0));
    case 0x6D:
    case 0x6F:
    case 0x7F:
    case 0x81:
      return trap(c, "a signed div or rem");
    case 0x6E:
    case 0x70:
    case 0x80:
    case 0x82:
      if (!idiom(c, op >= 0x80)) return trap(c, "a div or rem whose divisor is not X | (X == 0)");
      return ok_if(effect(c, 2, 1));
    case 0x71:
      if (!pop(c, &b) || !pop(c, &a)) return RUN_BAD;
      return ok_if(push(c, a.bound || b.bound, tighter(a, b)));
    case 0xFC:
    case 0xFD:
      return trap(c, "a 0xFC or 0xFD prefix op");
    default:
      break;
  }
  width = load_width(op);
  if (width > 0u) return access(c, r, width, 0);
  width = store_width(op);
  if (width > 0u) return access(c, r, width, 1);
  if (is_float(op)) return trap(c, "a float op");
  width = plain_inputs(op);
  if (width > 0u) return ok_if(effect(c, width, 1));
  return RUN_BAD; /* not an MVP opcode */
}

static Run check_body(Check *c, Reader *r, uint32_t func) {
  const FuncType *t = &c->types[c->func_type[func]];
  uint32_t groups, n, i;
  uint8_t type;
  if (!get_u32(r, &groups)) return RUN_BAD;
  for (i = 0; i < groups; i++)
    if (!get_u32(r, &n) || !get_byte(r, &type)) return RUN_BAD;
  c->height = 0;
  c->depth = 1;
  c->frames[0].base = 0;
  c->frames[0].params = 0;
  c->frames[0].results = t->results;
  c->frames[0].dead = 0;
  for (i = 0; i < 5u; i++) {
    c->past[i].op = 0xFF;
    c->past[i].imm = 0;
  }
  while (c->depth > 0u) {
    size_t at = r->at;
    uint8_t op;
    uint32_t imm = 0;
    Run run;
    if (!get_byte(r, &op)) return RUN_BAD;
    run = instruction(c, r, op, &imm, func);
    if (run == RUN_TRAP) return trap_form(c, (long)func, at, c->why);
    if (run == RUN_BAD) return RUN_BAD;
    for (i = 4; i > 0u; i--) c->past[i] = c->past[i - 1u];
    c->past[0].op = op;
    c->past[0].imm = imm;
  }
  return ok_if(r->at == r->end);
}

/* Kahn: the call graph of the defined functions has no cycle (N2). */
static int acyclic(Check *c) {
  uint32_t count = c->func_count - c->import_funcs;
  uint32_t i, k, head = 0, tail = 0;
  size_t e;
  for (e = 0; e < c->edge_count; e++) {
    c->start[c->edge_from[e] + 1u]++;
    c->in_degree[c->edge_to[e]]++;
  }
  for (i = 0; i < count; i++) {
    c->start[i + 1u] += c->start[i];
    c->fill[i] = c->start[i];
  }
  for (e = 0; e < c->edge_count; e++) {
    c->next[c->fill[c->edge_from[e]]] = c->edge_to[e];
    c->fill[c->edge_from[e]]++;
  }
  for (i = 0; i < count; i++)
    if (c->in_degree[i] == 0u) c->queue[tail++] = i;
  while (head < tail) {
    uint32_t u = c->queue[head++];
    for (k = c->start[u]; k < c->start[u + 1u]; k++) {
      c->in_degree[c->next[k]]--;
      if (c->in_degree[c->next[k]] == 0u) c->queue[tail++] = c->next[k];
    }
  }
  return tail == count;
}

static Run memory_limits(Check *c, Reader *r) {
  uint32_t min, max;
  int has_max;
  if (!get_limits(r, &min, &max, &has_max)) return RUN_BAD;
  c->has_memory = 1;
  c->memory_bytes = (uint64_t)min * PAGE_BYTES;
  if (!has_max || max != min) return trap_form(c, -1, 0, "a memory whose max is not its min (N3)");
  return RUN_OK;
}

static Run types(Check *c, Reader *s) {
  uint32_t count, k, params, results;
  uint8_t form;
  if (!get_u32(s, &count) || count > MAX_TYPES) return RUN_BAD;
  for (k = 0; k < count; k++) {
    if (!get_byte(s, &form) || form != 0x60 || !get_u32(s, &params) || !skip_bytes(s, params) ||
        !get_u32(s, &results) || !skip_bytes(s, results))
      return RUN_BAD;
    c->types[k].params = params;
    c->types[k].results = results;
  }
  c->type_count = count;
  return RUN_OK;
}

static Run imports(Check *c, Reader *s) {
  uint32_t count, k, index, min, max;
  uint8_t kind, b;
  int has_max;
  if (!get_u32(s, &count)) return RUN_BAD;
  for (k = 0; k < count; k++) {
    if (!skip_name(s) || !skip_name(s) || !get_byte(s, &kind)) return RUN_BAD;
    switch (kind) {
      case 0:
        if (!get_u32(s, &index) || index >= c->type_count || c->func_count >= MAX_FUNCS) return RUN_BAD;
        c->func_type[c->func_count] = index;
        c->func_count++;
        c->import_funcs++;
        break;
      case 1:
        if (!get_byte(s, &b) || !get_limits(s, &min, &max, &has_max)) return RUN_BAD;
        break;
      case 2:
        if (memory_limits(c, s) == RUN_BAD) return RUN_BAD;
        break;
      case 3:
        if (!get_byte(s, &b) || !get_byte(s, &b)) return RUN_BAD;
        break;
      default:
        return RUN_BAD;
    }
  }
  return RUN_OK;
}

static Run functions(Check *c, Reader *s, uint32_t *defined) {
  uint32_t count, k, index;
  if (!get_u32(s, &count) || count > MAX_FUNCS - c->func_count) return RUN_BAD;
  for (k = 0; k < count; k++) {
    if (!get_u32(s, &index) || index >= c->type_count) return RUN_BAD;
    c->func_type[c->func_count] = index;
    c->func_count++;
  }
  *defined = count;
  return RUN_OK;
}

static Run tables(Reader *s) {
  uint32_t count, k, min, max;
  uint8_t type;
  int has_max;
  if (!get_u32(s, &count)) return RUN_BAD;
  for (k = 0; k < count; k++)
    if (!get_byte(s, &type) || !get_limits(s, &min, &max, &has_max)) return RUN_BAD;
  return RUN_OK;
}

static Run memories(Check *c, Reader *s) {
  uint32_t count, k;
  if (!get_u32(s, &count)) return RUN_BAD;
  for (k = 0; k < count; k++)
    if (memory_limits(c, s) == RUN_BAD) return RUN_BAD;
  return RUN_OK;
}

static int const_expr(Reader *r) {
  uint8_t op = 0;
  int64_t value;
  uint32_t index;
  while (op != 0x0B) {
    if (!get_byte(r, &op)) return 0;
    switch (op) {
      case 0x0B:
        break;
      case 0x41:
        if (!get_signed(r, 32, &value)) return 0;
        break;
      case 0x42:
        if (!get_signed(r, 64, &value)) return 0;
        break;
      case 0x43:
        if (!skip_bytes(r, 4)) return 0;
        break;
      case 0x44:
        if (!skip_bytes(r, 8)) return 0;
        break;
      case 0x23:
        if (!get_u32(r, &index)) return 0;
        break;
      default:
        return 0;
    }
  }
  return 1;
}

static Run globals(Reader *s) {
  uint32_t count, k;
  uint8_t type, mut;
  if (!get_u32(s, &count)) return RUN_BAD;
  for (k = 0; k < count; k++)
    if (!get_byte(s, &type) || !get_byte(s, &mut) || !const_expr(s)) return RUN_BAD;
  return RUN_OK;
}

static Run exports(Reader *s) {
  uint32_t count, k, index;
  uint8_t kind;
  if (!get_u32(s, &count)) return RUN_BAD;
  for (k = 0; k < count; k++)
    if (!skip_name(s) || !get_byte(s, &kind) || !get_u32(s, &index)) return RUN_BAD;
  return RUN_OK;
}

static Run code(Check *c, Reader *s, uint32_t defined) {
  uint32_t count, k, size;
  if (!get_u32(s, &count) || count != defined) return RUN_BAD;
  for (k = 0; k < count; k++) {
    Reader body;
    if (!get_u32(s, &size) || size > s->end - s->at) return RUN_BAD;
    body.p = s->p;
    body.at = s->at;
    body.end = s->at + size;
    s->at += size;
    if (check_body(c, &body, c->import_funcs + k) == RUN_BAD) return RUN_BAD;
  }
  return RUN_OK;
}

static Run data(Check *c, Reader *s) {
  uint32_t count, k, flag, memory, length;
  int64_t offset;
  uint8_t b;
  if (!get_u32(s, &count)) return RUN_BAD;
  for (k = 0; k < count; k++) {
    offset = 0;
    if (!get_u32(s, &flag) || flag > 2u || (flag == 2u && !get_u32(s, &memory))) return RUN_BAD;
    if (flag != 1u &&
        (!get_byte(s, &b) || b != 0x41 || !get_signed(s, 32, &offset) || !get_byte(s, &b) || b != 0x0B))
      return trap_form(c, -1, 0, "a data segment offset that is not i32.const");
    if (!get_u32(s, &length) || !skip_bytes(s, length)) return RUN_BAD;
    if (flag != 1u && (!c->has_memory || (uint64_t)(uint32_t)offset + length > c->memory_bytes))
      trap_form(c, -1, 0, "a data segment past the memory");
  }
  return RUN_OK;
}

static Run check_module(Check *c, const uint8_t *p, size_t n) {
  static const uint8_t magic[8] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00};
  Reader r;
  uint32_t defined = 0, size, index;
  int has_code = 0;
  if (n < 8u || memcmp(p, magic, 8) != 0) return RUN_BAD;
  r.p = p;
  r.at = 8;
  r.end = n;
  while (r.at < r.end) {
    Reader s;
    uint8_t id;
    Run run = RUN_OK;
    if (!get_byte(&r, &id) || !get_u32(&r, &size) || size > r.end - r.at) return RUN_BAD;
    s.p = p;
    s.at = r.at;
    s.end = r.at + size;
    r.at += size;
    switch (id) {
      case 0:
        break;
      case 1:
        run = types(c, &s);
        break;
      case 2:
        run = imports(c, &s);
        break;
      case 3:
        run = functions(c, &s, &defined);
        break;
      case 4:
        run = tables(&s);
        break;
      case 5:
        run = memories(c, &s);
        break;
      case 6:
        run = globals(&s);
        break;
      case 7:
        run = exports(&s);
        break;
      case 8:
        run = ok_if(get_u32(&s, &index));
        break;
      case 9:
        run = trap_form(c, -1, 0, "an element section");
        break;
      case 10:
        has_code = 1;
        run = code(c, &s, defined);
        break;
      case 11:
        run = data(c, &s);
        break;
      case 12:
        run = ok_if(get_u32(&s, &index));
        break;
      default:
        run = RUN_BAD;
        break;
    }
    if (run == RUN_BAD) return RUN_BAD;
  }
  if (defined > 0u && !has_code) return RUN_BAD;
  if (!acyclic(c)) trap_form(c, -1, 0, "a call-graph cycle (N2)");
  if (c->fails > 0u) return RUN_TRAP;
  return RUN_OK;
}

/* The self-test: small modules as byte arrays. */

typedef struct {
  uint8_t b[512];
  size_t n;
} Buf;

static void put(Buf *o, uint8_t v) {
  if (o->n < sizeof o->b) o->b[o->n] = v;
  o->n++;
}

static void put_u32(Buf *o, uint32_t v) {
  uint8_t b;
  do {
    b = (uint8_t)(v & 0x7Fu);
    v >>= 7;
    put(o, v != 0u ? (uint8_t)(b | 0x80u) : b);
  } while (v != 0u);
}

/* A signed LEB128 of a value that is not negative. */
static void put_s32(Buf *o, uint32_t v) {
  uint8_t b;
  int more = 1;
  while (more) {
    b = (uint8_t)(v & 0x7Fu);
    v >>= 7;
    more = v != 0u || (b & 0x40u) != 0u;
    put(o, more ? (uint8_t)(b | 0x80u) : b);
  }
}

static void put_bytes(Buf *o, const uint8_t *p, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) put(o, p[i]);
}

static void put_section(Buf *o, uint8_t id, const Buf *s) {
  put(o, id);
  put_u32(o, (uint32_t)s->n);
  put_bytes(o, s->b, s->n);
}

typedef struct {
  const char *name;
  const uint8_t *body[2]; /* the code of each function: the locals, the instructions, end */
  size_t size[2];
  uint32_t funcs;
  uint32_t min, max;           /* the memory pages */
  uint32_t data_at, data_size; /* data_size 0: no data section */
} Case;

/* store x at (x & 65528), load at (x & 65528), divide by x | (x == 0). The
   i64.const 0 has a 2-byte immediate (0x80 0x00; 0x80 is the i64.div_u
   opcode), and the block has a br: a tool that does not skip an immediate
   or a label fails here. */
static const uint8_t good[] = {0x00, 0x42, 0x80, 0x00, 0x1A, 0x02, 0x40, 0x0C, 0x00, 0x0B, 0x20, 0x00, 0xA7,
                               0x41, 0xF8, 0xFF, 0x03, 0x71, 0x20, 0x00, 0x37, 0x03, 0x00, 0x20, 0x00, 0xA7,
                               0x41, 0xF8, 0xFF, 0x03, 0x71, 0x29, 0x03, 0x00, 0x20, 0x00, 0x20, 0x00, 0x50,
                               0xAD, 0x84, 0x80, 0x0B};
static const uint8_t c_unreachable[] = {0x00, 0x00, 0x0B};
static const uint8_t c_grow[] = {0x00, 0x41, 0x01, 0x40, 0x00, 0x1A, 0x20, 0x00, 0x0B};
static const uint8_t c_indirect[] = {0x00, 0x20, 0x00, 0x41, 0x00, 0x11, 0x00, 0x00, 0x0B};
static const uint8_t c_float[] = {0x00, 0x44, 0, 0, 0, 0, 0, 0, 0, 0, 0xB0, 0x0B};
static const uint8_t c_prefix[] = {0x00, 0x20, 0x00, 0xFC, 0x00, 0x0B};
static const uint8_t c_signed[] = {0x00, 0x20, 0x00, 0x20, 0x00, 0x7F, 0x0B};
static const uint8_t c_div[] = {0x00, 0x20, 0x00, 0x20, 0x00, 0x80, 0x0B};
static const uint8_t c_div_two[] = {0x01, 0x01, 0x7E, 0x20, 0x00, 0x20, 0x00, 0x20, 0x01, 0x50, 0xAD, 0x84, 0x80, 0x0B};
static const uint8_t c_rem32[] = {0x00, 0x20, 0x00, 0xA7, 0x20, 0x00, 0xA7, 0x70, 0xAD, 0x0B};
static const uint8_t c_load[] = {0x00, 0x20, 0x00, 0xA7, 0x29, 0x03, 0x00, 0x0B};
static const uint8_t c_load_past[] = {0x00, 0x20, 0x00, 0xA7, 0x41, 0xF8, 0xFF, 0x03, 0x71, 0x29, 0x03, 0x08, 0x0B};
static const uint8_t c_store[] = {0x00, 0x20, 0x00, 0xA7, 0x20, 0x00, 0x37, 0x03, 0x00, 0x20, 0x00, 0x0B};
static const uint8_t c_store_value[] = {0x00, 0x20, 0x00, 0xA7, 0x41, 0xF8, 0xFF, 0x03, 0x20,
                                        0x00, 0xA7, 0x71, 0x36, 0x02, 0x00, 0x20, 0x00, 0x0B};
static const uint8_t c_call0[] = {0x00, 0x20, 0x00, 0x10, 0x00, 0x0B};
static const uint8_t c_call1[] = {0x00, 0x20, 0x00, 0x10, 0x01, 0x0B};

#define ONE(name, body) {name, {body, NULL}, {sizeof body, 0}, 1u, 1u, 1u, 0u, 0u}

static const Case cases[] = {
    ONE("good", good),
    {"good_call", {c_call1, good}, {sizeof c_call1, sizeof good}, 2u, 1u, 1u, 0u, 0u},
    ONE("unreachable", c_unreachable),
    ONE("memory_grow", c_grow),
    ONE("call_indirect", c_indirect),
    ONE("float", c_float),
    ONE("prefix", c_prefix),
    ONE("signed_div", c_signed),
    ONE("div_no_idiom", c_div),
    ONE("div_two_locals", c_div_two),
    ONE("rem32_no_idiom", c_rem32),
    ONE("load_no_bound", c_load),
    ONE("load_past_memory", c_load_past),
    ONE("store_no_bound", c_store),
    ONE("store_bound_on_value", c_store_value),
    {"memory_max", {good, NULL}, {sizeof good, 0}, 1u, 1u, 2u, 0u, 0u},
    {"data_past_memory", {good, NULL}, {sizeof good, 0}, 1u, 1u, 1u, 65530u, 8u},
    ONE("self_call", c_call0),
    {"call_cycle", {c_call1, c_call0}, {sizeof c_call1, sizeof c_call0}, 2u, 1u, 1u, 0u, 0u},
};

/* type 0 = (i64) -> i64 for each function, one memory, the code, then an
   optional data segment of zero bytes. */
static void build(const Case *k, Buf *o) {
  static const uint8_t head[8] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00};
  static const uint8_t type[6] = {0x01, 0x60, 0x01, 0x7E, 0x01, 0x7E};
  Buf s;
  uint32_t i;
  o->n = 0;
  put_bytes(o, head, sizeof head);
  s.n = 0;
  put_bytes(&s, type, sizeof type);
  put_section(o, 1, &s);
  s.n = 0;
  put_u32(&s, k->funcs);
  for (i = 0; i < k->funcs; i++) put(&s, 0);
  put_section(o, 3, &s);
  s.n = 0;
  put(&s, 1);
  put(&s, 1);
  put_u32(&s, k->min);
  put_u32(&s, k->max);
  put_section(o, 5, &s);
  s.n = 0;
  put_u32(&s, k->funcs);
  for (i = 0; i < k->funcs; i++) {
    put_u32(&s, (uint32_t)k->size[i]);
    put_bytes(&s, k->body[i], k->size[i]);
  }
  put_section(o, 10, &s);
  if (k->data_size == 0u) return;
  s.n = 0;
  put(&s, 1);
  put(&s, 0);
  put(&s, 0x41);
  put_s32(&s, k->data_at);
  put(&s, 0x0B);
  put_u32(&s, k->data_size);
  for (i = 0; i < k->data_size; i++) put(&s, 0);
  put_section(o, 11, &s);
}

static const char *outcome(Run run, const Check *c) {
  if (run == RUN_OK) return "ok";
  if (run == RUN_TRAP) return c->reason;
  return "the module does not decode";
}

static int self_test(void) {
  size_t i;
  for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    Buf o;
    Run run = RUN_BAD;
    build(&cases[i], &o);
    memset(&state, 0, sizeof state);
    state.quiet = 1;
    state.file = cases[i].name;
    if (o.n <= sizeof o.b) run = check_module(&state, o.b, o.n);
    printf("%s: %s\n", cases[i].name, outcome(run, &state));
  }
  return 0;
}

static int read_file(const char *path, uint8_t **out, size_t *n) {
  FILE *f = fopen(path, "rb");
  uint8_t *buf = NULL;
  size_t cap = 0, len = 0, got = 1;
  if (f == NULL) return 0;
  while (got > 0u) {
    if (len == cap) {
      uint8_t *grow;
      cap = cap == 0u ? 65536u : cap * 2u;
      grow = cap > MAX_MODULE ? NULL : realloc(buf, cap);
      if (grow == NULL) {
        free(buf);
        fclose(f);
        return 0;
      }
      buf = grow;
    }
    got = fread(buf + len, 1, cap - len, f);
    len += got;
  }
  fclose(f);
  *out = buf;
  *n = len;
  return 1;
}

int main(int argc, char **argv) {
  int i, bad = 0;
  unsigned long modules = 0, forms = 0;
  if (argc == 2 && strcmp(argv[1], "--self-test") == 0) return self_test();
  if (argc < 2) {
    fprintf(stderr, "usage: notraptool MODULE.wasm... | notraptool --self-test\n");
    return 2;
  }
  for (i = 1; i < argc; i++) {
    uint8_t *buf;
    size_t n;
    Run run;
    if (!read_file(argv[i], &buf, &n)) {
      printf("FAIL notrap %s: cannot read the file\n", argv[i]);
      bad = 1;
      continue;
    }
    memset(&state, 0, sizeof state);
    state.file = argv[i];
    run = check_module(&state, buf, n);
    free(buf);
    modules++;
    forms += state.fails;
    if (run == RUN_BAD) {
      printf("FAIL notrap %s: the module does not decode\n", argv[i]);
      bad = 1;
    }
  }
  printf("notrap: %lu modules checked, %lu trap forms\n", modules, forms);
  if (bad) return 2;
  if (forms > 0u) return 1;
  return 0;
}
