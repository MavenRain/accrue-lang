/* The JSON abi (accrue S9 P5). `langc abi PROG` writes one line:
   {"entries":[{"name":N,"params":[K, ...],"result":R}, ...]}, one object for
   each entry, in the order of --text. Ported from the json_* functions in
   cairn-lang 6b73467 src/main.c (cairn-lang Q-S9-5); the file is src/json.c
   as PLAN section 7.2 names it (Q-S9-6 (a)). A kind is nat, flag or u256,
   without the Wasm layout of the --text form. An Option result has the word
   `option` before its kind, as in --text (Q-S8-2 (c), Q-S9-7 (a)).
   accrue S12 A2: `langc check --json PROG` writes one line with json_check
   (ported from cairn-lang 98d18ef src/front/json.c, in this file by
   Q-S9-6 (a)). The escapes of json_string are those of quorum-lang
   src/front/json.c; json_char of the abi does not change. */
#include "json.h"
#include "front/base.h"

#include <string.h>

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

/* One byte of a JSON string of json_string_n. */
static void put_char(unsigned char c, FILE *out) {
  const char *escape = c == '"' ? "\\\"" : c == '\\' ? "\\\\" : c == '\n' ? "\\n" : c == '\t' ? "\\t" : NULL;
  if (escape != NULL) {
    fputs(escape, out);
    return;
  }
  if (c < 0x20) {
    fprintf(out, "\\u%04x", (unsigned)c);
    return;
  }
  fputc(c, out);
}

void json_string_n(const char *text, size_t len, FILE *out) {
  fputc('"', out);
  for (size_t pos = 0; pos < len;) {
    size_t width = utf8_char_size(text + pos, len - pos);
    /* Paths and truncated diagnostic buffers can contain invalid bytes. */
    if (width == 0) {
      fputs("\\ufffd", out);
      pos++;
      continue;
    }
    for (size_t i = 0; i < width; i++) put_char((unsigned char)text[pos + i], out);
    pos += width;
  }
  fputc('"', out);
}

void json_string(const char *text, FILE *out) {
  json_string_n(text, strlen(text), out);
}

/* A span with line 0 has no source position: null. */
static void put_span(const char *path, Span span, FILE *out) {
  if (span.line == 0) {
    fputs("null", out);
    return;
  }
  fputs("{\"file\":", out);
  json_string(path, out);
  fprintf(out, ",\"line\":%d,\"col\":%d,\"end_line\":%d,\"end_col\":%d}", span.line, span.col, span.end_line, span.end_col);
}

/* The fix of DIAG: null, or {"span":{..},"text":".."} (S12). */
static void put_fix(const char *path, const Diag *diag, FILE *out) {
  if (!diag->fix_set) {
    fputs("null", out);
    return;
  }
  fputs("{\"span\":", out);
  put_span(path, diag->fix_span, out);
  fputs(",\"text\":", out);
  json_string(diag->fix, out);
  fputc('}', out);
}

void json_check(FILE *out, const char *path, const Diag *diag, const CheckProgress *progress) {
  fputs("{\"diagnostics\":[", out);
  if (diag->set) {
    /* The message is the text after `CODE: ` that diag_print writes. */
    char message[sizeof diag->def + sizeof diag->msg + 2];
    snprintf(message, sizeof message, "%s: %s", diag->def, diag->msg);
    fputs("{\"code\":", out);
    json_string(diag->code, out);
    fputs(",\"span\":", out);
    put_span(path, diag->span, out);
    fputs(",\"message\":", out);
    json_string(message, out);
    fputs(",\"fix\":", out);
    put_fix(path, diag, out);
    fputc('}', out);
  }
  fprintf(out, "],\"progress\":{\"parsed\":%d,\"typed\":%d,\"laws_discharged\":%d,\"laws_total\":%d}}\n",
          progress->parsed, progress->typed, progress->laws_discharged, progress->laws_total);
}
