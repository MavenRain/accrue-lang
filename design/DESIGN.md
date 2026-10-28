# accrue-lang domain design

> Paste the domain design below the line, verbatim. Do not edit it.

This file is the only source of the core types and the core operations of
accrue-lang. `SPEC.md` sections 5 and 6 cite its sections. Do not add a core
type or a core operation that this file does not give.

When the design changes, paste the new version. Then record the change as an
open item in `SPEC.md` section 9.

---

## The request (USER, 2026-10-27, verbatim)

One change only: the paste had one trailing space after "criteria:". That
space is not here, because `git diff --check` refuses trailing white space.

```text
I want to write a programming language with the following characteristics:

First, the language should not allow a program to "trap" or surface an exception.

For type constructors/higher-kinded types, admit only cell monads and their duals as represented computationally by Cubical Type Theory.

For base types and operations, admit only what is necessary to implement programs like what can be admitted in https://github.com/MavenRain/retainer-lang, except that the contracts should instead be off-chain protocols, and the governance of these protocols should be performed by the off-chain equivalents of ZK/FHE/MPC DAOs (a ZK/FHE/MPC off-chain threshold-authorized, self-amending system).

Finally, I want agents to reach for this language when it best fits their use cases.  To that end, adhere to these additional criteria:

  The plan for making agents reach for the language includes:
  - one canonical form per operation and a langc fmt canonical printer;
  - langc check --json, giving fix hints and progress counts (parsed, typed, laws
    discharged);
  - a SKILL.md with 3 to 5 graded examples;
  - an MCP server written in TinyCC (langc mcp);
  - AGENTS.md and a README table saying when to use the language;
  - a token-count comparison against Python and Rust.

The host language shall be TinyCC.  Use your judgment to provide multiple-choice options for the compilation target(s).
```

## The S0 answers (USER, 2026-10-27)

- Targets: Wasm MVP, ES module + JSON, WGSL compute.
- Primary target: Wasm.
- Name: accrue-lang.

## The retainer-lang domain design (MavenRain/retainer-lang 6f46bc5, design/DESIGN.md below its line, verbatim)

The retainer is not a second DAO. It is a time-indexed measure acted on by the aggregation already fixed in the self-referential DAO: governance stays a left Kan extension, and the fee schedule is a fixed point of that governance. Scheduled transfer, pre-authorized pull, and Sablier-style stream are three representations of one integral.

The full note is in `DENOTATIONAL-DESIGN-RETAINER.md`.




## Stance

Elliott's rule: the model is fixed first, a representation second, and an operation is admitted only when the meaning of the instance is the instance of the meaning. The model is not an allowance slot, a stream id, or an EVM trace. Partiality of governance is `Option`-valued.

What is inherited, and not redefined, from the existing design:

\[
\llbracket\mathrm{DAO}\rrbracket = \mathrm{Aggregation}\,\mathrm{act}\, F = \mathrm{LeftKanExtension}\,(\mathrm{orbitProjection}\,\mathrm{act})\, F
\]

\[
\mathrm{Gov}\, L = \mathrm{orbitProjection}\,\mathrm{act} \ggg L.\mathrm{functor}, \qquad \llbracket\mathrm{amend}\rrbracket = \mathrm{Gov}
\]

Anonymity is the orbit quotient, legitimacy is the Lan universal property, and the three fates are UAT's trichotomy. The treasury hole left out of scope there is filled the way the escrow note fills it: settlement is an action of the aggregation, not a second aggregation. The acted-on algebra is a behavior \(\mathrm{Time} \to \mathrm{Amount}\), because unearned revenue is a residual of two measures, not a claim list.

## Semantic domain

\[
\mathrm{Time} = \mathbb{R}_{\ge 0}, \qquad \mathrm{Amount} = (A, 0, +)
\]

with \(A\) a cancellative commutative monoid, so the residual is well-defined. Continuous time is the denotation; block time is a sampling.

Governance decides a policy, not a balance:

\[
\mathrm{Policy} = \mathrm{Rate} \times \mathrm{Window} \times \mathrm{Auth} \times \mathrm{Dispute}
\]

\[
\mathrm{policyOf} : D \to \mathrm{Policy}, \qquad F : \mathrm{ChoiceRule}\,\mathrm{Obj}\, D
\]

An engagement is the signed scope the rate is earned against: client, firm, `scopeHash` of the engagement letter, window, policy. The hash is not a ballot. Custody is low because a claim is earned against that hash, not against a bare deposit.

The meaning of the whole is the dependent pair

\[
\llbracket\mathrm{RetainerDAO}\rrbracket = \Sigma\,(L : \mathrm{Aggregation}\,\mathrm{act}\, F).\;\mathrm{Book}\, L
\]

where \(\mathrm{Book}\, L\) reads rate, window, authorization, and dispute off \(\mathrm{Gov}\, L\). The monoid \(A\) is the codomain of settlement, not an object of the Kan extension.

## One measure, three instruments

For engagement \(E\) under aggregation \(L\),

\[
\mathrm{rateAt}(L, E, s) = \mathrm{Rate}\bigl(\mathrm{policyOf}\,((\mathrm{Gov}\, L).\mathrm{obj}\,(\mathrm{ballotAt}\, s))\bigr)(s)
\]

\[
\mathrm{earned}(L, E, t) = \int_{\mathrm{start}(E)}^{\min(t,\,\mathrm{end}(E))} \mathbf{1}_{\mathrm{Auth}(E,s)}\cdot\mathrm{rateAt}(L, E, s)\, ds
\]

Zero before start, nondecreasing in \(t\) for a nonnegative retainer rate. The three shapes are representations of this measure.

- Scheduled transfer. The rate is a Dirac comb. Installment \(a\) at period \(\Delta\) gives \(\mathrm{earned}(t) = a \cdot \#\{k \mid t_k \le t \le \mathrm{end}\}\). The monthly push is evaluation of the comb, not a separate obligation.
- Pre-authorized pull. The same measure, plus \(\mathrm{Auth}(\mathrm{client})\) and an allowance at least the withdrawable amount. The pull chooses the actor. It does not change the obligation, which is why invoice-and-chase disappears: the engagement signature is the authorization, and the integral is the invoice.
- Continuous stream. Locally constant rate. A closed Sablier linear stream funded by deposit \(d\) is \(\mathrm{rate} = d/(\mathrm{end}-\mathrm{start})\) and \(\mathrm{earned}(t) = \mathrm{clamp}(0, d, \mathrm{rate}\cdot(t-\mathrm{start}))\). An open retainer takes the rate as primary and the deposit as a buffer. Cliff and step-unlock are piecewise rates, not new denotations.

So \(\llbracket\mathrm{scheduled}\rrbracket = \llbracket\mathrm{pull}\rrbracket = \llbracket\mathrm{stream}\rrbracket = (\mathrm{earned}, \mathrm{recognized}, \mathrm{withdrawable})\). A representation that distinguishes them observationally, other than by who moves the residual, is an abstraction leak.

## Books

Let \(\mathrm{funded}\) be cash in custody and \(\mathrm{withdrawn}\) be what the firm has taken.

\[
\begin{align*}
\mathrm{recognized}(t) &= \min(\mathrm{funded}(t), \mathrm{earned}(t)) \\
\mathrm{unearned}(t) &= \mathrm{funded}(t) - \mathrm{recognized}(t) \\
\mathrm{withdrawable}(t) &= \mathrm{recognized}(t) - \mathrm{withdrawn}(t)
\end{align*}
\]

Period close at \(T\) emits one unearned-revenue event by evaluation, not by chase: debit unearned and credit revenue by \(\mathrm{recognized}(T) - \mathrm{recognized}(T^-)\). A withdrawal debits the payable and credits treasury. The expense was recognized when it accrued, whether or not anyone withdrew. That matches the bookkeeping reading of a per-second stream: the liability exists because the integral advanced, not because cash moved.

Custody invariant: \(\mathrm{withdrawn} \le \mathrm{earned}\) and, when the client prefunded, \(\mathrm{balance} \ge \mathrm{unearned}\). Tokens leave custody only through \(\mathrm{withdrawable}\). A dispute map acts on the residual only. Earned-against-`scopeHash` is final.

## Self-reference

The fixed point is on the rule that emits the rate, not on the integral.

\[
\mathrm{Gov}_\pi\, L = \mathrm{policyOf} \circ \mathrm{Gov}\, L
\]

\[
\mathrm{IsSelfConstitutingPolicy}\, F \iff \exists\, L,\; \mathrm{Gov}_\pi\, L = \mathrm{policyOf} \circ F
\]

At such an \(L\), the rate inside the integral is the rate the charter states. Amending a fee, a window, an authorization, or a dispute policy is not a new vote:

\[
\llbracket\mathrm{amendPolicy}\rrbracket = \mathrm{policyOf} \circ \mathrm{Gov} = \mathrm{Gov}_\pi
\]

which inherits \(\llbracket\mathrm{amend}\rrbracket = \mathrm{Gov}\) by post-processing \(D\).

Causality is a homomorphism law. If \(L\) and \(L'\) agree on the \(\mathrm{Gov}\)-image of every ballot at time \(\le t\), then \(\mathrm{earned}(L, E, t) = \mathrm{earned}(L', E, t)\). An amendment at \(t^\*\) contributes only on \((t^\*, \mathrm{end}]\). A representation that rewrites recognized revenue below \(t^\*\) is rejected.

Regimes, carried over honestly:

- Arrow-impossibility: no aggregation, so no new engagement rule. Accrual freezes at the last self-constituting witness, or at the zero rate if there has never been one. No silent fee rewrite.
- Arrow-Debreu: unique self-constituting policy, one earned curve per engagement.
- Schelling-Ising: a fork of future rates, one shared frozen past, not a double-spend. For a constant constitution the self-constitution stays single-valued, as in the base design. A fee that genuinely bifurcates needs a non-constant, order-parameter-pinning constitution. That remains the prime extension.

## Admitted operations

Each is forced by the model.

- Fund is a monoid action on \(\mathrm{funded}\), identity on \(L\) and on earned history.
- Settle, whether push, pull, or stream withdraw, adds \(\mathrm{withdrawable}(t)\) to \(\mathrm{withdrawn}\). Same \(\mathrm{settle}'\) on the model.
- Cancel at \(t^\*\) clips the future integral and returns \(\mathrm{unearned}(t^\*)\) to the client. The firm keeps what is recognized.
- Dispute applies \(\mathrm{Dispute}(v)\) to the residual only.
- Amend is \(\mathrm{Gov}\), as before.

Recognition over concatenated periods equals recognition of the concatenation, because \(\mathrm{earned}(t_2) = \mathrm{earned}(t_1) + \mathrm{earned}(t_1, t_2)\). Month-end entries that do not add to the quarter are an abstraction leak. No algebraic type-class morphism is claimed for \(\mathrm{Aggregation}\) itself; the base design retracted that slogan, and this note does not restore it.

What this refuses: a stream id or a cron transfer as the meaning; a second aggregation whose objects are balances; a bifurcating self-constitution for a constant fee; any dispute or amendment that rewrites recognized revenue.
