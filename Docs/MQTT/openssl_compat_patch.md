# MQTT OpenSSL Compatibility Patch

## Scope

Fix Qt/Paho MQTT connection failures on OpenSSL 3 hosts where the broker requires legacy TLS security level compatibility.

## Root Cause

The broker accepts TLS only when OpenSSL security level is lowered to `SECLEVEL=0` (or equivalent cipher policy).
Without this compatibility, Qt MQTT reports transport/client error (`mqtt_error=256`) before successful subscribe.

## Applied Fix

- Add `accloud/infra/mqtt/core/OpenSslCompat.h` helper to:
  - create a temporary OpenSSL config with `CipherString = DEFAULT:@SECLEVEL=0`
  - set `OPENSSL_CONF` automatically when insecure TLS compatibility mode is enabled
  - keep existing `OPENSSL_CONF` untouched if already defined
  - support explicit override path with `ACCLOUD_MQTT_OPENSSL_CONF_PATH`
- Apply compatibility bootstrap before Qt network initialization in `accloud/app/main.cpp`.
- Apply the same compatibility bootstrap in live MQTT tests:
  - `accloud/tests/mqtt/test_mqtt_live_broker.cpp`
  - `accloud/tests/mqtt/test_paho_mqttc_live.cpp`
  - `accloud/tests/mqtt/test_paho_mqttpp_live.cpp`

## Validation

- Qt live broker test passes with session path configured.
- Paho C live broker test passes.
- Paho C++ live broker test passes.
