# Security policy

## Reporting a vulnerability

**Do not open a public issue for anything you believe is exploitable.**

Report privately through GitHub's **"Report a vulnerability"** button under this
repository's **Security** tab. That opens a private advisory visible only to the
maintainers — it is the preferred route and needs no email address.

Please include what you found, how to reproduce it, and what you think the impact is. A
proof of concept helps but is not required to file.

**What to expect:** acknowledgement within 3 working days, an initial assessment within
10, and a fix or a clear explanation of why it isn't one. We will credit you unless you
prefer otherwise, and we will tell you before publishing anything that names you.

## Supported versions

These packages are pre-1.0. Only the latest released version of each is supported. There
are no backports to earlier tags.

## What is in scope

- A tool reporting a **safe** verdict for a configuration that is genuinely unsafe — the
  most serious class here, because someone may ship on the strength of it.
- `farkas-check` **accepting an invalid certificate**, or any input that causes it to read
  or write out of bounds.
- Any input to any package causing a crash, hang, unbounded memory growth, or arbitrary
  code execution.
- `pqc-migration-mcp` returning data it is documented not to return — see below.
- A packaging or CI defect that could let a third party inject code into a release.

## What is out of scope

- **The failure cases in PQC-MFB are not vulnerabilities.** The benchmark deliberately
  describes 312 ways an unrepaired design breaks. That is the dataset's purpose.
- **`pqc-dos-embedded` demonstrates a memory exhaustion on purpose.** The "naive" arm is
  supposed to run out of memory. That is the demo.
- Findings against a **third-party product**. Nothing in this repository asserts anything
  about a named vendor's implementation; report those to that vendor.
- Missing hardening in code paths documented as out of scope in a package README — for
  instance, these tools perform no cryptography and make no security claim about *your*
  protocol.
- Theoretical weaknesses in the standardized algorithms themselves. Those belong with the
  standards bodies.

## A note on the closed-core boundary

`pqc-migration-mcp` and the PQC-MFB dataset deliberately withhold repair-mechanism data.
If you find a way to extract `repair_mechanism`, `repaired_detail` or `repaired_held`
through any published interface, **that is a valid report** and we would like to hear
about it — it is an information-disclosure defect against a documented boundary, and
there is a test that is supposed to prevent it.

## Please do not

- Run automated scanners against infrastructure you do not own.
- Open a PR that fixes a security issue before reporting it privately — the diff is the
  disclosure.
