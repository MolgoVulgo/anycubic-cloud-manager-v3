# Appendix — Archive policy

Status: `IMPLEMENTED`.

The distributable source archive contains code, build configuration, packaging and maintained documentation. Local tests are excluded from the archive. Public reference data is limited to small synthetic fixtures that can be reviewed in a normal diff.

## Excluded material

Never include:

- HAR captures, sessions, tokens, cookies or signed URLs;
- TLS private keys or local environment files;
- raw MQTT broker captures or complete user activity histories;
- persistent real printer, task, message or filename identifiers;
- private binary fixtures such as redistributability-unknown PWMB files;
- build outputs, runtime logs, caches and local databases.

When private evidence is required for investigation, keep it outside the distributable repository and document only the redacted conclusion, aggregate statistics or synthetic reproduction.

## Public fixtures

The files under [`../reference-data/`](../reference-data/README.md) are synthetic. They explain parser and workflow vocabulary but are not evidence of universal broker behaviour.

`tools/check_documentation_contract.py` remains available as a separate repository guard for maintained documentation and frozen MQTT/SSL contracts.

## Web review source archive

Edit `ARCHIVE_NAME` near the top of `make-a.sh`, then run `./make-a.sh`. The script creates the archive at the repository root without running the documentation contract.

`acm.zip` remains a manually supplied project base. Agents must not regenerate or replace it automatically.
