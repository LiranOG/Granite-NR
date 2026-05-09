# Security Policy

## Supported Versions

The following versions of GRANITE are currently receiving security updates:

| Version        | Supported          |
| -------------- | ------------------ |
| v0.6.7 (main)  | :white_check_mark: |
| < v0.6.7       | :x:                |

## Reporting a Vulnerability

We take the security of GRANITE seriously. If you believe you have found a security vulnerability
in this repository, please report it to us responsibly.

**Do NOT open a public GitHub Issue for security vulnerabilities.**

### How to Report

1. **GitHub Private Vulnerability Reporting (Preferred)**
   Use GitHub's built-in private reporting feature:
   [Report a vulnerability](../../security/advisories/new)

2. **Email**
   If you prefer email, contact the maintainer directly via the contact information
   listed on the repository profile.

### What to Include

Please provide as much of the following information as possible:

- Type of issue (e.g., buffer overflow, SQL injection, memory corruption, etc.)
- Full paths of source file(s) related to the issue
- The location of the affected source code (tag/branch/commit or direct URL)
- Any special configuration required to reproduce the issue
- Step-by-step instructions to reproduce the issue
- Proof-of-concept or exploit code (if possible)
- Impact of the issue, including how an attacker might exploit it

### Response Timeline

- **Acknowledgement**: Within **48 hours** of receiving your report
- **Status update**: Within **7 days** with an assessment and expected resolution timeline
- **Resolution**: We aim to patch confirmed vulnerabilities within **30 days**

You will be credited in the security advisory (unless you prefer to remain anonymous).

## Security Best Practices for Users

- Always use the latest version from the `main` branch or latest tagged release
- Do not expose simulation output files or configuration files containing sensitive paths publicly
- When running GRANITE in a containerized environment, follow least-privilege principles
- Review the [CONTRIBUTING.md](CONTRIBUTING.md) before submitting patches that touch
  numerical kernels or I/O routines

## Scope

This security policy applies to the GRANITE numerical relativity engine source code,
build system, CI/CD workflows, and official container images.
Third-party submodules or dependencies are governed by their own respective security policies.
