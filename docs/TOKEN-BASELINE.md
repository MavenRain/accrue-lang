# Token baseline (S15)

This file compares the tokens of the worked examples G1 to G5 in
accrue-lang, Python and Rust (PLAN 8, row A6). The counts in this file are
an offline estimate: ceil(bytes / 4) of each file (Q-S15-3 (a)). They are
not tokenizer counts. The API route (`make tokens`: the Anthropic
`count_tokens` endpoint with the newest Claude model, Q-S0-8 (a), Q-S15-3
(a)) is not run, because no API Console key is available. `make check`
checks the file hashes and the estimate block in this file, with no key
and no network (Q-S15-6 (a)).

Status: the estimate is from 2026-11-05 (`build/tokcount --estimate`). The
API route is not run.

## The task

Each G is one worked example of `skills/accrue-lang/SKILL.md` (S14). The 3
programs of a G do the same task and print the same lines. The 16 eval
rows are the `TOKEN_EVALS` rows of the Makefile (Q-S15-4 (a)):

| G | task | eval rows |
| :--- | :--- | :--- |
| G1 | a retainer, opened and countersigned with 2 of 3 FROST Schnorr signatures | `egGood`, `csGood`, `csFundCtl` |
| G2 | a closed linear stream and a cancel that pays the unearned amount back | `eStream12`, `eStream17`, `payCancel` |
| G3 | an encrypted policy vote (the tally of each decision stays encrypted) | `boxPlur`, `boxMaj`, `tally1` |
| G4 | a rate change during the engagement | `ea37`, `cvAt`, `cvAfterNew` |
| G5 | a DAO amendment of `tAmend` and of the rule | `sLowAmend`, `sLowMajRule`, `govSplit0`, `govSplitMaj` |

## The files

- `examples/skill/g1-retainer.lang` to `g5-amend.lang`: the accrue-lang
  programs (S14). All 5 run in kit mode (`langc --kit`). The count is of
  the `.lang` file only: `kit.txt` is a list of 5 file names, not program
  text.
- `test/baseline/g1-retainer.py` to `g5-amend.py`: the same tasks in
  Python, with the standard library only (Q-S15-5 (a)).
- `test/baseline/rust/src/bin/g1.rs` to `g5.rs`: the same tasks in Rust,
  1 crate with 5 bins (Q-S15-5 (a)). The crate uses `sha2` 0.10 and
  `k256` 0.13. Only G1 uses `sha2` and the secp256k1 group of `k256`. G1,
  G2 and G4 use the `U256` type of `k256` for the amounts. G3 and G5 use
  no crate. `Cargo.lock` pins the versions.

Each Python and each Rust program prints the eval lines of its G in the
`TOKEN_EVALS` order. The lines must be equal, byte for byte, to the output
of `langc --kit eval`. `make tokens` checks this before it counts.

## The method

- Estimate: `build/tokcount --estimate` reads the full text of each file,
  comments included. The estimate of a file is ceil(bytes / 4), that is
  (bytes + 3) / 4 in integers (Q-S15-3 (a)). It needs no key and no
  network. An estimate is not a tokenizer count: it has no model and no
  message frame.
- Request (the API route): `POST /v1/messages/count_tokens` with 1 user
  message. The message is the full text of 1 file, comments included. The
  reply is `{"input_tokens": N}`.
- Model (the API route): the model with the newest `created_at` in the
  reply of `GET /v1/models` on the run date (Q-S15-3 (a)).
- Frame (the API route): N includes the tokens of the message frame. Thus
  `make tokens` also counts a fixed 1-byte message, `x`. The net count of
  a file is N minus the count of that message (Q-S15-3 (a)).
- Ratios: each ratio divides 2 estimates now (2 net counts on the API
  route), half up, with 2 decimals. One line gives each Python file and
  each Rust file over the accrue-lang file of the same G. The last 2 lines
  give the sum of each language over the sum of the 5 accrue-lang files.
  Each file is a full alternative, thus the table has no total row.
- Hashes: the SHA-256 of each counted file. `make check` runs
  `build/tokcount --check docs/TOKEN-BASELINE.md`. Each hash in the table
  must be the hash of the file now. `make check` also compares the lines
  between the markers with the output of `build/tokcount --estimate`
  (Q-S15-6 (a)). Thus a changed program fails the gate until a new
  estimate block replaces the old one.

## The run

The estimate: from the repo root, offline, with no key:

```
build/tokcount --estimate
```

The output is the block between the markers below, byte for byte.

The API route (not run; it needs an API Console key): from the repo root,
outside the sandbox, with `ANTHROPIC_API_KEY` set:

```
make tokens
```

The rule does these steps:

1. It builds the Rust crate (`RUST_BUILD`, by default `cargo build -j 2`).
2. It runs each program and compares its output with `langc --kit eval`.
3. It gets the model list and selects the model (`build/tokcount --model`).
4. It sends 1 request for the frame message and 1 request for each of the
   15 files (`build/tokcount --request` makes each body). It keeps each
   reply in `build/tokens/`.
5. It writes the table with `build/tokcount --table build/tokens`.

Now the lines between the markers below come from `build/tokcount
--estimate`. On the API route, step 5 replaces them.

## The table

Model: none (an offline estimate). Date: 2026-11-05.

<!-- tokcount begin -->
| file | bytes | estimate | sha256 |
| :--- | ---: | ---: | :--- |
| examples/skill/g1-retainer.lang | 2101 | 526 | 0d8c738788c57b50971d9375e3d5385aacf91eaeb3b7fd0ecdddc53ea3b750d4 |
| test/baseline/g1-retainer.py | 10893 | 2724 | e9ea9b0975a8d44f670b03c0486c2a41fc1573f01fb6678462fa41e384591d38 |
| test/baseline/rust/src/bin/g1.rs | 13942 | 3486 | fd7f8e83cc98002421c169a5a023812a803b8d85969917fa7c0d7398aa9f4dd5 |
| examples/skill/g2-cancel.lang | 1856 | 464 | 5c91d8f53fb87a9c25c828d729cc1d584212046125c7cc19fcefc9ae4f4ec553 |
| test/baseline/g2-cancel.py | 8936 | 2234 | 0a606d8d14c519c20c8c29367bd215aaf2080afe84a97009d784893867ef5e12 |
| test/baseline/rust/src/bin/g2.rs | 11877 | 2970 | f64e3202df9e1618ae8895bb4b5429af73ffc354ab7d8cb54886126d2895f0dd |
| examples/skill/g3-vote.lang | 1862 | 466 | d7fd9715e91309f3fca8a82243655ed032da834c758c988378eb68c070b4cef3 |
| test/baseline/g3-vote.py | 4602 | 1151 | 0a6378d3d1ac86d9ac7d00da59f5733c50474860ede30ded5c4219ead52193a3 |
| test/baseline/rust/src/bin/g3.rs | 4923 | 1231 | ccb08fb0ccff7d486d891182dc66b8246ac235f391181c489afa26949ced9e80 |
| examples/skill/g4-rate.lang | 1721 | 431 | 2d868b5bae7731319ef200f41b4b3e5f7e47e72063b32397e3037ac573e16c86 |
| test/baseline/g4-rate.py | 5068 | 1267 | 79204c3534dfbe18cc3d16aadec0cb1513a33f6d6bdc93c8846b51375ccf5de7 |
| test/baseline/rust/src/bin/g4.rs | 6882 | 1721 | 1c7422404942bb6b1da795986898fc6a0c3cd86b82ffc72dc75bb2fcc9706c04 |
| examples/skill/g5-amend.lang | 1674 | 419 | c5f7b39956cdc645487e260641c0f3644844d9f31f911d05ca857246090a3bc7 |
| test/baseline/g5-amend.py | 3583 | 896 | 54208d519b8133080fdb53a691beab1356c6d6f3f61b240cd63059322cba5f38 |
| test/baseline/rust/src/bin/g5.rs | 3905 | 977 | a69bd1f68d08115a42c91283b493c72010c0fe72bce3b88598119c68be879f48 |

estimate: ceil(bytes / 4) of each file; this is not a tokenizer count.

ratio of the estimates (test/baseline/g1-retainer.py over examples/skill/g1-retainer.lang): 5.18
ratio of the estimates (test/baseline/rust/src/bin/g1.rs over examples/skill/g1-retainer.lang): 6.63
ratio of the estimates (test/baseline/g2-cancel.py over examples/skill/g2-cancel.lang): 4.81
ratio of the estimates (test/baseline/rust/src/bin/g2.rs over examples/skill/g2-cancel.lang): 6.40
ratio of the estimates (test/baseline/g3-vote.py over examples/skill/g3-vote.lang): 2.47
ratio of the estimates (test/baseline/rust/src/bin/g3.rs over examples/skill/g3-vote.lang): 2.64
ratio of the estimates (test/baseline/g4-rate.py over examples/skill/g4-rate.lang): 2.94
ratio of the estimates (test/baseline/rust/src/bin/g4.rs over examples/skill/g4-rate.lang): 3.99
ratio of the estimates (test/baseline/g5-amend.py over examples/skill/g5-amend.lang): 2.14
ratio of the estimates (test/baseline/rust/src/bin/g5.rs over examples/skill/g5-amend.lang): 2.33
ratio of the estimates (the 5 Python files over the 5 accrue-lang files): 3.59
ratio of the estimates (the 5 Rust files over the 5 accrue-lang files): 4.50
<!-- tokcount end -->

Result: for the 5 G together, the estimate of the Python files is 3.59
times the estimate of the accrue-lang files, and the estimate of the Rust
files is 4.50 times the estimate of the accrue-lang files.

## Notes

- Versions: the estimate uses no tool version; it reads only the bytes of
  the files. On the API route, the `python3`, `rustc` and `cargo`
  versions come from `build/tokens/versions.txt`.
- A `make tokens` run that replaces the estimate block must also remove
  the estimate compare of `make check` (the 2 Makefile lines after
  `build/tokcount --check`).
- The standard library of Python has no elliptic curve. Thus G1 in Python
  holds its own secp256k1 arithmetic and its own Schnorr check, and G1 in
  Rust uses `k256`. G1 has the largest ratios of the 5 G.
- The amounts are 256-bit. Python uses `int` and checks the range
  (`U256_END`). Rust uses `U256` with `checked_add` and `checked_sub`;
  `mul_div` is `mul_wide`, then `div_rem` on `U512`. The times are `u64`.
- The 256-bit constants in Rust use `U256::from_be_hex`, which `k256`
  exports again.
- Cargo dependencies apply to the full crate. Thus `k256` and `sha2`
  build with each bin, but only `g1.rs` uses `sha2`.
- The Rust build gives no warnings. `g1.rs` to `g4.rs` have
  `#![allow(dead_code)]`.
