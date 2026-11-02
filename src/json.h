/* The JSON abi (accrue S9 P5, ported from cairn-lang 6b73467): `langc abi
   PROG` without --text writes it. See src/json.c. */
#ifndef LANG_JSON_H
#define LANG_JSON_H

#include <stdio.h>

#include "ir.h"

/* Writes entry FN as a JSON object, after a comma unless FIRST. Returns 1. */
int json_abi_entry(const IrFunc *fn, int first, FILE *out);

#endif
