# Appendix — Archive policy

Status: `IMPLEMENTED`.

The old documentation package contained useful documents, a HAR capture and a PWMB sample. The documentation archive keeps the reusable technical documentation and non-secret MQTT analysis data, but excludes sensitive or binary fixtures.

Excluded from this package:

- `uc.makeronline.com.har` — HAR captures may contain reusable tokens, cookies and signed URLs.
- `cube.pwmb` — binary fixture/sample, useful for tests but not documentation.

When such files are required, keep them in a private test-fixture location and document only their expected metadata/golden values.
