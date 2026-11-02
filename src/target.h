/* The interface of a target back end. SHARED by the tcc kits. Each kit
   links one file that defines these names: src/wasm.c or src/evm.c. Shared
   files get each target fact from here, never from #ifdef. */
#ifndef LANG_TARGET_H
#define LANG_TARGET_H

#include <stdio.h>

#include "ir.h"

typedef enum {
  TARGET_PART_MAIN,   /* Wasm: the module bytes. EVM: creation code, hex. */
  TARGET_PART_RUNTIME /* Wasm: the module bytes. EVM: runtime code, hex. */
} TargetPart;

extern const char target_name[];

/* Writes one `langc abi` line for FN: the name, then target data (EVM: the
   selector). Returns 1, or 0 after a message on ERR. */
int target_abi_line(const IrFunc *fn, FILE *out, FILE *err);

/* Writes PART of the target for PROG. Returns 1, or 0 after a message on ERR. */
int target_write(const IrProgram *prog, TargetPart part, FILE *out, FILE *err);

/* 1 when the heap of each entry of PROG fits WASM_MAX_PAGES pages (wasm.c
   program_pages), else 0 after a REFUSE_CAPACITY message on ERR. accrue S9:
   the JS target (src/js.c) calls it too, so both refuse the same programs. */
int target_capacity(const IrProgram *prog, FILE *err);

#endif
