# Validation

Date: 2026-11-06. tcc: 0.9.28rc 2026-09-04 mob@0fb54300 (AArch64 Darwin).
Host executable: `build/langc` at d3ddc7b.

`make check` passes: it builds `build/langc` and the test tools, and it
checks `check --json`, `mcp`, `explain`, the skill, `fmt`, the regression
groups (among them the domain, the crypto, the Wasm and ES module outputs,
the traps and the run fuel) and the token report. It prints 40 count lines
with zero failures. The run took 592 seconds.

Each slice from S0 to S16 has a row in docs/PLAN.md section 10 and a DONE
paragraph at the start of docs/PLAN.md that gives the changes of the slice.
The tables below give the trusted constants, the signature check and the
assumptions V5 and V6 that the gate does not prove.

## Trusted constants

Each row is a kernel constant with no computation rule. The checker
accepts its type as given, so each row is a trust assumption.

| Constant | Type | Since | Reason that the type holds |
|---|---|---|---|
| `natIsSet` | `isSet Nat` | S5, Q-S5-6 (b) | `Nat` has a decidable equality, so the Hedberg theorem gives `isSet Nat`. |
| `u256IsSet` | `isSet U256` | S5, Q-S5-6 (b) | `U256` has a decidable equality (Hedberg). |
| `flagIsSet` | `isSet Flag` | S5, Q-S5-6 (b) | `Flag` has a decidable equality (Hedberg). |
| `optNatIsSet` | `isSet (Option Nat)` | S3, Q-S3-11 (a) | `Option Nat` has a decidable equality, so the Hedberg theorem gives `isSet (Option Nat)`. |

Each constant is a `VIS_INTERNAL` opaque op (`OP_NAT_IS_SET`,
`OP_U256_IS_SET`, `OP_FLAG_IS_SET`, `OP_OPT_NAT_IS_SET` in `src/front/core.h`). The evaluator
keeps each one stuck. `rule_is_set` in `src/front/check.c` gives each one
its fixed kernel type, independently of the domain definition named
`isSet`. The domain folds `len`, `chainFrom` (thus `head`), `verify` and
`verifyAt` use them (`domain/domain.lang`), directly or through
`isSetFun`.

## Signature verification

`schnorrOk` is a computed `VIS_INTERNAL` kernel builtin
(`OP_SCHNORR_OK`). `rule_fixed` in `src/front/check.c` checks its argument
types, and `schnorr_ok` in `src/front/eval.c` computes the result when all
five arguments are closed `U256` words. Open arguments keep it stuck.

| Builtin | Type | Since | Check |
|---|---|---|---|
| `schnorrOk` | `U256 -> U256 -> U256 -> U256 -> U256 -> Flag` | S7, Q-S7-6 (a), Q-S7-8 (a) | The arguments are the key, R.x, z, head r and e. The C check is the secp256k1 x-only Schnorr check of Q-S7-2 (a): it gives 1 when z G = R + c P, else 0. The unforgeability of the threshold Schnorr signature is V1 to V4 (PLAN section 6). |

## Assumptions

Each row is an assumption that no path can prove. V1 to V4
stay in PLAN section 6.

| Item | Since | Assumption |
|---|---|---|
| V5 | S6, Q-S0-7 (a), Q-S6-8 (a) | The Pedersen DKG lets a member bias the distribution of the public key (Gennaro, Jarecki, Krawczyk, Rabin). The security of ElGamal and of the threshold Schnorr signature under that bias is an assumption (PLAN section 6, row V1 to V5). |
| V6 | S15, Q-S15-3 (a) | The counts in `docs/TOKEN-BASELINE.md` are an estimate, ceil(bytes / 4), not the counts of a tokenizer: the ratios of the languages are byte ratios, and the Claude token counts can differ. `make check` checks only that the hashes and the estimates match the files (`token report hashes: 15 of 15`, `token estimate: passed`). `make tokens` (the API counts) was not run. |

`src/front/dkg.c` deals and reshares the keys `keyOpen` and `keyAppend`:
a Pedersen DKG with Feldman commitments and a Schnorr proof for each dealt
secret. The reshare keeps the public key. `thr_deal` in
`src/front/threshold.c` is a trusted dealer. It is for test vectors only.

Initial DKG combination requires the agreed threshold and member count
from trusted caller state and one verified contribution from every member.
The caller must authenticate dealer identities, reliably broadcast the same
commitments to all members, and deliver the private shares over authenticated
confidential channels, as in the [FROST DKG protocol](https://github.com/ZcashFoundation/frost/blob/main/book/src/tutorial/dkg.md).
Quorum resharing instead uses the old threshold and verification keys.

The FROST signing of `src/front/frost.c` (S7) needs the same caller
guarantees. The caller must authenticate the signers and send the same
round 1 list of commitments to every signer (`src/front/dkg.h`).
