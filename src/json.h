/* The JSON abi (accrue S9 P5, ported from cairn-lang 6b73467): `langc abi
   PROG` without --text writes it. See src/json.c. */
#ifndef LANG_JSON_H
#define LANG_JSON_H

#include <stdio.h>

#include "front/check.h"
#include "ir.h"

/* Writes entry FN as a JSON object, after a comma unless FIRST. Returns 1. */
int json_abi_entry(const IrFunc *fn, int first, FILE *out);

/* accrue S12 A2 (the `langc check --json` writer of cairn-lang 98d18ef
   src/front/json.h). Writes TEXT as a JSON string: `"`, `\\`, newline and
   tab get a short escape, other control bytes `\u00XX`, an invalid UTF-8
   byte `�`, valid UTF-8 raw. The escapes of quorum-lang
   src/front/json.c. */
void json_string_n(const char *text, size_t len, FILE *out);
void json_string(const char *text, FILE *out);
/* Writes the one-line object of `langc check --json` and a newline:
   `{"diagnostics":[...],"progress":{...}}` in this key order. DIAG gives
   the one diagnostic when DIAG->set, else the list is empty; its fix is
   null, or the edit of diag_fix. PATH is the span file, as given. */
void json_check(FILE *out, const char *path, const Diag *diag, const CheckProgress *progress);

#endif
