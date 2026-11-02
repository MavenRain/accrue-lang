/* The JSON abi (accrue S9 P5). `langc abi PROG` writes one line:
   {"entries":[{"name":N,"params":[K, ...],"result":R}, ...]}, one object for
   each entry, in the order of --text. Ported from the json_* functions in
   cairn-lang 6b73467 src/main.c (cairn-lang Q-S9-5); the file is src/json.c
   as PLAN section 7.2 names it (Q-S9-6 (a)). A kind is nat, flag or u256,
   without the Wasm layout of the --text form. An Option result has the word
   `option` before its kind, as in --text (Q-S8-2 (c), Q-S9-7 (a)). */
#include "json.h"

/* One character of a JSON string. */
static void json_char(unsigned char c, FILE *out) {
  switch (c) {
    case '"':
    case '\\':
      fprintf(out, "\\%c", c);
      return;
    default:
      if (c < 0x20)
        fprintf(out, "\\u%04x", (unsigned)c);
      else
        fputc(c, out);
      return;
  }
}

/* The kind of a scalar, without the Wasm layout. */
static const char *json_shape(IrScalar sc) {
  switch (sc) {
    case IR_SCALAR_NAT:
      return "nat";
    case IR_SCALAR_FLAG:
      return "flag";
    case IR_SCALAR_U256:
      return "u256";
  }
  return "?";
}

int json_abi_entry(const IrFunc *fn, int first, FILE *out) {
  const unsigned char *c;
  size_t k;
  fputs(first ? "{\"name\":\"" : ",{\"name\":\"", out);
  for (c = (const unsigned char *)fn->name; *c != '\0'; c++) json_char(*c, out);
  fputs("\",\"params\":[", out);
  for (k = 0; k < fn->param_count; k++) fprintf(out, "%s\"%s\"", k == 0 ? "" : ",", json_shape(fn->params[k]));
  fprintf(out, "],\"result\":\"%s%s\"}", fn->result_option ? "option " : "", json_shape(fn->result));
  return 1;
}
