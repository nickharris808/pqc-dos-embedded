# Contributing to `pqc-dos-embedded`

Thanks for looking. This is a small, deliberately auditable package — small patches
are easier to accept than large ones.

## Ground rules

1. **Tests must pass.** Run the suite before opening a PR (see the README).
2. **New behaviour needs a test.** Especially a *negative* test — the thing that
   should be refused. Most of the value in this codebase is in what it rejects.
3. **No new runtime dependencies** without discussion. Several of these packages
   advertise zero dependencies, and that is a feature.
4. **Keep numbers honest.** If you change a measured figure, change the artifact
   that produced it, not the prose. Do not hand-edit a benchmark result.

## What we especially want

- Additional transports, algorithms or platforms.
- Portability fixes (other compilers, other Python versions, other operating systems).
- Counterexamples that show a check is weaker than it claims.
- Clearer error messages — if a message misled you, that is a bug worth reporting.

## What is out of scope

This package is intentionally narrow. Requests to add the full protocol envelope,
the repair mechanisms, or the minimum-cover computation are out of scope here: those
are part of a separate closed codebase. Feature requests in that direction are welcome
as conversations, not as PRs.

## Reporting a security issue

Please do **not** open a public issue for anything you believe is exploitable. Use
private disclosure via the repository's security contact, and allow reasonable time
for a fix before publishing.

## Licence and sign-off

Contributions are accepted under the licence in [LICENSE](LICENSE). By opening a PR you
confirm you have the right to contribute the code under that licence.
