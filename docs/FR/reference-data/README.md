# Reference data

This directory contains non-secret analysis artifacts used by the documentation.

Included:

- MQTT print workflow report;
- global MQTT analysis JSON;
- consolidated MQTT JSONL analysis;
- CSV segments between `start` events with different `taskid` values.

Excluded by policy:

- HAR captures;
- session files;
- TLS private keys;
- binary PWMB fixtures;
- full signed URLs.

Those excluded files can be useful locally for tests or research, but they are not appropriate for a public documentation archive.
