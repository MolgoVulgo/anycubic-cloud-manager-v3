# Public reference data

This directory contains small, synthetic fixtures for documentation and regression checks.

The samples are designed to explain the MQTT print workflow without exposing a real account, printer, task, filename, timestamp, message identifier or capture chronology.

Rules:

- only synthetic or aggregate data may be committed here;
- no HAR, session file, signed URL, token, cookie or TLS private key;
- no raw broker capture or complete user workflow history;
- identifiers must use explicit `demo` values;
- files must remain small enough for review in a normal diff.

The current MQTT sample is not evidence of broker behaviour by itself. Runtime code, controlled observations and the active MQTT documentation remain authoritative.
