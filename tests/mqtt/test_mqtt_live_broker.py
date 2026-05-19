#!/usr/bin/env python3
import base64
import hashlib
import json
import os
import socket
import ssl
import struct
import sys
from pathlib import Path

from cryptography import x509
from cryptography.hazmat.primitives.asymmetric import padding


BROKER_HOST = "mqtt-universe.anycubic.com"
BROKER_PORT = 8883
KEEPALIVE = 1200


def md5_hex(value: str) -> str:
    return hashlib.md5(value.encode("utf-8")).hexdigest()


def session_path() -> Path:
    return Path(__file__).resolve().parents[2] / "session.json"


def load_session_tokens() -> dict:
    p = session_path()
    if not p.exists():
        return {}
    try:
        raw = json.loads(p.read_text(encoding="utf-8"))
    except Exception:
        return {}
    tokens = raw.get("tokens")
    return tokens if isinstance(tokens, dict) else {}


def resolve_credentials_inputs() -> tuple[str, str]:
    tokens = load_session_tokens()
    email = os.getenv("ACCLOUD_MQTT_EMAIL") or os.getenv("AC_EMAIL") or tokens.get("email", "")
    token = (os.getenv("ACCLOUD_MQTT_AUTH_TOKEN")
             or os.getenv("AC_TOKEN")
             or tokens.get("auth_token", "")
             or tokens.get("token", ""))
    return str(email).strip(), str(token).strip()


def resolve_tls_path(env_key: str, preferred: str, legacy: str) -> Path:
    env = os.getenv(env_key, "").strip()
    if env:
        return Path(env)
    root = Path(__file__).resolve().parents[2]
    preferred_path = root / "resources" / "mqtt" / "tls" / preferred
    if preferred_path.exists():
        return preferred_path
    return root / "resources" / "mqtt" / "tls" / legacy


def load_public_key_from_cert(cert_path: Path):
    cert = x509.load_pem_x509_certificate(cert_path.read_bytes())
    return cert.public_key()


def build_slicer_credentials(email: str, token: str, ca_cert_path: Path) -> tuple[str, str, str]:
    client_id = md5_hex(email + "pcf")
    encrypted = load_public_key_from_cert(ca_cert_path).encrypt(
        token.encode("utf-8"), padding.PKCS1v15()
    )
    mqtt_token = base64.b64encode(encrypted).decode("ascii")
    signature = md5_hex(client_id + mqtt_token + client_id)
    username = f"user|pcf|{email}|{signature}"
    return client_id, username, mqtt_token


def encode_varint(value: int) -> bytes:
    out = bytearray()
    while True:
        byte = value % 128
        value //= 128
        if value > 0:
            byte |= 0x80
        out.append(byte)
        if value == 0:
            break
    return bytes(out)


def mqtt_utf8(s: str) -> bytes:
    b = s.encode("utf-8")
    return struct.pack("!H", len(b)) + b


def build_connect_packet(client_id: str, username: str, password: str) -> bytes:
    variable_header = mqtt_utf8("MQTT") + bytes([4, 0xC2]) + struct.pack("!H", KEEPALIVE)
    payload = mqtt_utf8(client_id) + mqtt_utf8(username) + mqtt_utf8(password)
    remaining = len(variable_header) + len(payload)
    return bytes([0x10]) + encode_varint(remaining) + variable_header + payload


def build_subscribe_packet(topic: str, packet_id: int = 1) -> bytes:
    payload = mqtt_utf8(topic) + bytes([0])  # QoS 0
    variable_header = struct.pack("!H", packet_id)
    remaining = len(variable_header) + len(payload)
    return bytes([0x82]) + encode_varint(remaining) + variable_header + payload


def recv_exact(sock: ssl.SSLSocket, n: int) -> bytes:
    out = bytearray()
    while len(out) < n:
        chunk = sock.recv(n - len(out))
        if not chunk:
            raise RuntimeError("socket_closed")
        out.extend(chunk)
    return bytes(out)


def recv_mqtt_packet(sock: ssl.SSLSocket) -> tuple[int, bytes]:
    first = recv_exact(sock, 1)[0]
    mul = 1
    remaining = 0
    while True:
        b = recv_exact(sock, 1)[0]
        remaining += (b & 0x7F) * mul
        if (b & 0x80) == 0:
            break
        mul *= 128
    payload = recv_exact(sock, remaining) if remaining > 0 else b""
    return first, payload


def main() -> int:
    email, token = resolve_credentials_inputs()
    if not email or not token:
        print("[FAIL] Missing email/token inputs (env or session.json)", file=sys.stderr)
        return 2

    ca_path = resolve_tls_path("ACCLOUD_MQTT_TLS_CA_PATH",
                               "anycubic_mqtt_tls_ca.crt",
                               "anycubic_mqqt_tls_ca.crt")
    cert_path = resolve_tls_path("ACCLOUD_MQTT_TLS_CLIENT_CERT_PATH",
                                 "anycubic_mqtt_tls_client.crt",
                                 "anycubic_mqqt_tls_client.crt")
    key_path = resolve_tls_path("ACCLOUD_MQTT_TLS_CLIENT_KEY_PATH",
                                "anycubic_mqtt_tls_client.key",
                                "anycubic_mqqt_tls_client.key")
    for p in (ca_path, cert_path, key_path):
        if not p.exists():
            print(f"[FAIL] Missing TLS file: {p}", file=sys.stderr)
            return 2

    client_id, username, password = build_slicer_credentials(email, token, ca_path)
    user_id = md5_hex(token)  # fallback only for topic build when user_id unknown
    session_tokens = load_session_tokens()
    jwt_user_id = session_tokens.get("user_id") or session_tokens.get("uid")
    uid = str(jwt_user_id).strip() if jwt_user_id else ""
    if not uid:
        uid = session_tokens.get("id", "")
    topic = None
    if uid:
        topic = f"anycubic/anycubicCloud/v1/server/app/{uid}/{md5_hex(uid)}/slice/report"

    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.minimum_version = ssl.TLSVersion.TLSv1_2
    ctx.maximum_version = ssl.TLSVersion.TLSv1_2
    try:
        ctx.set_ciphers("ALL:@SECLEVEL=0")
    except ssl.SSLError:
        pass
    ctx.load_cert_chain(str(cert_path), str(key_path))
    ctx.load_verify_locations(str(ca_path))
    allow_insecure = os.getenv("ACCLOUD_MQTT_TLS_ALLOW_INSECURE", "1").strip().lower() not in ("0", "false", "no")
    if allow_insecure:
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
    else:
        ctx.check_hostname = True
        ctx.verify_mode = ssl.CERT_REQUIRED

    sock = socket.create_connection((BROKER_HOST, BROKER_PORT), timeout=15)
    ssock = ctx.wrap_socket(sock, server_hostname=BROKER_HOST if not allow_insecure else None)
    try:
        ssock.sendall(build_connect_packet(client_id, username, password))
        packet_type, payload = recv_mqtt_packet(ssock)
        if (packet_type >> 4) != 2 or len(payload) != 2:
            print(f"[FAIL] Invalid CONNACK packet type={packet_type} len={len(payload)}", file=sys.stderr)
            return 1
        connack_rc = payload[1]
        if connack_rc != 0:
            print(f"[FAIL] Broker rejected CONNECT rc={connack_rc}", file=sys.stderr)
            return 1

        if topic:
            ssock.sendall(build_subscribe_packet(topic, packet_id=1))
            sub_type, sub_payload = recv_mqtt_packet(ssock)
            if (sub_type >> 4) != 9:
                print(f"[FAIL] Expected SUBACK, got type={sub_type >> 4}", file=sys.stderr)
                return 1
            if len(sub_payload) < 3:
                print("[FAIL] Invalid SUBACK payload length", file=sys.stderr)
                return 1
            qos = sub_payload[2]
            if qos == 0x80:
                print("[FAIL] SUBACK failure (0x80)", file=sys.stderr)
                return 1

        ssock.sendall(b"\xE0\x00")  # DISCONNECT
        print("[PASS] Live MQTT broker CONNECT/SUBSCRIBE validated with SLICER+mTLS")
        return 0
    finally:
        try:
            ssock.close()
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())
