# accrue-lang

accrue-lang is a domain-specific language. It comes from lang-template with
the tcc-wasm host.

## When to use accrue-lang

| Task | accrue-lang? | Use instead |
| --- | --- | --- |
| A retainer: open an engagement, countersign it, fund it and read the recognized value (G1) | yes | |
| A closed linear stream and a cancel that pays the unearned value back to the client (G2) | yes | |
| An encrypted policy vote, with a tally and a choice (G3) | yes | |
| A rate change during the engagement that keeps the earned value before the change (G4) | yes | |
| A change of the rules by the DAO (G5) | yes | |
| An on-chain contract (an EVM allowance, a stream id or a cron transfer) | no | a smart contract language, for example Solidity |
| A deadline from a host or chain clock (a program reads time only as a `Stamp` argument) | no | a service or a contract that reads a clock |
| A change to recognized revenue (a dispute or an amendment that rewrites the past) | no | an accounting system or a database |
| A vote that sets a balance (the DAO votes on a policy) | no | a multisig wallet |
| A general program: a new data type, general recursion, an unproved fact or an exception | no | a general-purpose language, for example Rust or Python |

`skills/accrue-lang/SKILL.md` gives the examples G1 to G5 (the files of
`examples/skill/`). `AGENTS.md` gives the loop and the commands for an
agent.

## Files

- `SPEC.md`: the specification. Fill each section.
- `design/DESIGN.md`: the domain design. It is the only source of the core
  types and the core operations.
- `formers/FORMERS.md`: the type formers. `formers/tcc-wasm.md` gives their
  status on the tcc-wasm host.
- `probe/CAPABILITY.md`: the host probe.
- `docs/STATUS.md` and `docs/VALIDATION.md`: the status and the gate results.
- `docs/host/README.md`: how to build and test the host kit.
- `domain/README.md`: what the domain holds and how to change it.
