#!/usr/bin/env python3
"""
Collect endpoint responses for Anycubic Cloud Workbench.

Sources used to define the endpoint coverage:
- Docs/docs_unifies_core_web_cloud_sync.md
- Docs/docs_unifies_core_web_cloud_sync.md

Authentication source:
- session.json (same token structure as existing project code).
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import re
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


WORKBENCH_BASE_URL = "https://cloud-universe.anycubic.com"
ACCOUNT_BASE_URL = "https://uc.makeronline.com"


DEFAULT_PUBLIC_APP_ID = "f9b3528877c94d5c9c5af32245db46ef"
DEFAULT_PUBLIC_APP_SECRET = "0cf75926606049a3937f56b0373b99fb"
DEFAULT_PUBLIC_VERSION = "1.0.0"
DEFAULT_PUBLIC_DEVICE_TYPE = "web"
DEFAULT_PUBLIC_IS_CN = "2"
DEFAULT_REGION = "global"
DEFAULT_DEVICE_ID = "manager-anycubic-cloud-dev"
DEFAULT_USER_AGENT = "manager-anycubic-cloud/0.1.0"
DEFAULT_CLIENT_VERSION = "0.1.0"


SENSITIVE_KEY_FRAGMENTS = (
    "authorization",
    "token",
    "signature",
    "credential",
    "secret",
    "cookie",
    "password",
)

CHINESE_TRANSLATIONS = {
    "请求被接受": "Request accepted",
    "操作成功": "Operation successful",
    "连接成功": "Connection successful",
    "用户不存在": "User does not exist",
}


def _env_first(*keys: str, fallback: str) -> str:
    for key in keys:
        val = os.getenv(key, "").strip()
        if val:
            return val
    return fallback


def _strip_bearer(value: str) -> str:
    value = value.strip()
    if value.lower().startswith("bearer "):
        return value[7:].strip()
    return value


def _is_sensitive_key(key: str) -> bool:
    lowered = key.lower()
    return any(fragment in lowered for fragment in SENSITIVE_KEY_FRAGMENTS)


def _sanitize_text(value: str) -> str:
    if not value:
        return value
    out = value
    out = re.sub(r"[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]", "", out)
    out = re.sub(
        r"https?://[^\s\"']+\?[^\"'\s]*(?:X-Amz-|signature=)[^\"'\s]*",
        "<signed-url-redacted>",
        out,
    )
    out = re.sub(r"(?i)bearer\s+[A-Za-z0-9\-\._~\+\/]+=*", "Bearer <redacted>", out)
    out = re.sub(
        r'(?i)("(?:(?:access|id|refresh)_token|token|authorization|signature)"\s*:\s*")[^"]+(")',
        r"\1<redacted>\2",
        out,
    )
    out = re.sub(
        r"(?i)(access_token|id_token|refresh_token|token|signature)=([^&\s]+)",
        r"\1=<redacted>",
        out,
    )
    return out


def _sanitize_url(url: str) -> str:
    parsed = urllib.parse.urlsplit(url)
    if not parsed.query:
        return url
    pairs = urllib.parse.parse_qsl(parsed.query, keep_blank_values=True)
    if any(key.lower().startswith("x-amz-") for key, _ in pairs):
        return urllib.parse.urlunsplit((parsed.scheme, parsed.netloc, parsed.path, "<redacted>", parsed.fragment))
    safe_pairs: list[tuple[str, str]] = []
    for key, value in pairs:
        if _is_sensitive_key(key):
            safe_pairs.append((key, "<redacted>"))
        else:
            safe_pairs.append((key, value))
    safe_query = urllib.parse.urlencode(safe_pairs)
    return urllib.parse.urlunsplit((parsed.scheme, parsed.netloc, parsed.path, safe_query, parsed.fragment))


def _sanitize_obj(obj: Any) -> Any:
    if isinstance(obj, dict):
        out: dict[str, Any] = {}
        for key, value in obj.items():
            if _is_sensitive_key(key):
                out[key] = "<redacted>"
            else:
                out[key] = _sanitize_obj(value)
        return out
    if isinstance(obj, list):
        return [_sanitize_obj(item) for item in obj]
    if isinstance(obj, str):
        return _sanitize_text(obj)
    return obj


def _translate_chinese_text(value: str) -> str:
    out = value
    for zh, en in CHINESE_TRANSLATIONS.items():
        if zh in out:
            out = out.replace(zh, f"{zh} ({en})")
    return out


def _translate_obj(obj: Any) -> Any:
    if isinstance(obj, dict):
        return {k: _translate_obj(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_translate_obj(v) for v in obj]
    if isinstance(obj, str):
        return _translate_chinese_text(obj)
    return obj


def _sanitize_headers(headers: dict[str, str]) -> dict[str, str]:
    sanitized: dict[str, str] = {}
    for key, value in headers.items():
        if _is_sensitive_key(key):
            sanitized[key] = "<redacted>"
        else:
            sanitized[key] = _sanitize_text(value)
    return sanitized


def _load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as fh:
        data = json.load(fh)
    if not isinstance(data, dict):
        raise ValueError(f"{path} must contain a JSON object at root")
    return data


def _resolve_session_path(explicit: str | None) -> Path:
    if explicit:
        return Path(explicit)
    from_env = os.getenv("ACCLOUD_SESSION_PATH", "").strip()
    if from_env:
        return Path(from_env)
    candidate = Path("accloud/session.json")
    if candidate.exists():
        return candidate
    return Path("session.json")


def load_session_tokens(session_path: Path) -> dict[str, str]:
    root = _load_json(session_path)
    raw: dict[str, str] = {}

    tokens = root.get("tokens")
    if isinstance(tokens, dict):
        for key, value in tokens.items():
            if isinstance(value, (str, int, float, bool)):
                raw[key] = str(value)

    headers = root.get("headers")
    if isinstance(headers, dict):
        for key, value in headers.items():
            if isinstance(value, (str, int, float, bool)):
                raw[key] = str(value)
                lowered = key.lower()
                if lowered in ("x-access-token", "x-auth-token") and "token" not in raw:
                    raw["token"] = str(value)

    for key in ("Authorization", "authorization", "access_token", "id_token", "token"):
        if key in root and isinstance(root[key], (str, int, float, bool)):
            raw[key] = str(root[key])

    access_token = raw.get("access_token", "").strip()
    token = raw.get("token", "").strip()
    id_token = raw.get("id_token", "").strip()
    authorization = raw.get("Authorization", raw.get("authorization", "")).strip()

    if not access_token and token:
        access_token = token
    if access_token:
        access_token = _strip_bearer(access_token)
    if not access_token and authorization:
        access_token = _strip_bearer(authorization)

    if not token and access_token:
        token = access_token
    if token:
        token = _strip_bearer(token)

    if not id_token and access_token:
        id_token = access_token

    normalized: dict[str, str] = {}
    if access_token:
        normalized["access_token"] = access_token
    if id_token:
        normalized["id_token"] = id_token
    if token:
        normalized["token"] = token
    return normalized


def make_workbench_headers(access_token: str, xx_token: str | None) -> dict[str, str]:
    app_id = _env_first("ACCLOUD_PUBLIC_APP_ID", "ACCLOUD_WORKBENCH_APP_ID", fallback=DEFAULT_PUBLIC_APP_ID)
    app_secret = _env_first(
        "ACCLOUD_PUBLIC_APP_SECRET",
        "ACCLOUD_WORKBENCH_APP_SECRET",
        fallback=DEFAULT_PUBLIC_APP_SECRET,
    )
    version = _env_first("ACCLOUD_PUBLIC_VERSION", "ACCLOUD_WORKBENCH_VERSION", fallback=DEFAULT_PUBLIC_VERSION)
    device_type = _env_first("ACCLOUD_PUBLIC_DEVICE_TYPE", fallback=DEFAULT_PUBLIC_DEVICE_TYPE)
    is_cn = _env_first("ACCLOUD_PUBLIC_IS_CN", fallback=DEFAULT_PUBLIC_IS_CN)
    region = _env_first("ACCLOUD_REGION", "ACCLOUD_CLIENT_REGION", fallback=DEFAULT_REGION)
    device_id = _env_first("ACCLOUD_DEVICE_ID", "ACCLOUD_CLIENT_DEVICE_ID", fallback=DEFAULT_DEVICE_ID)
    user_agent = _env_first("ACCLOUD_USER_AGENT", "ACCLOUD_CLIENT_USER_AGENT", fallback=DEFAULT_USER_AGENT)
    client_version = _env_first("ACCLOUD_CLIENT_VERSION", fallback=DEFAULT_CLIENT_VERSION)

    ts_ms = str(int(time.time() * 1000))
    nonce = str(uuid.uuid4())
    sig_raw = f"{app_id}{ts_ms}{version}{app_secret}{nonce}{app_id}".encode("utf-8")
    signature = hashlib.md5(sig_raw).hexdigest()

    headers = {
        "User-Agent": user_agent,
        "X-Client-Version": client_version,
        "X-Region": region,
        "X-Device-Id": device_id,
        "X-Request-Id": nonce,
        "Authorization": f"Bearer {access_token}",
        "XX-Device-Type": device_type,
        "XX-IS-CN": is_cn,
        "XX-Version": version,
        "XX-Nonce": nonce,
        "XX-Timestamp": ts_ms,
        "XX-Signature": signature,
    }
    if xx_token:
        headers["XX-Token"] = xx_token
    return headers


def _to_url(path: str, query: dict[str, Any] | None = None) -> str:
    if path.startswith("http://") or path.startswith("https://"):
        base = path
    else:
        base = WORKBENCH_BASE_URL + path
    if not query:
        return base
    pairs: list[tuple[str, str]] = []
    for key, value in query.items():
        if value is None:
            continue
        if isinstance(value, (list, tuple)):
            for item in value:
                pairs.append((key, str(item)))
        else:
            pairs.append((key, str(value)))
    joiner = "&" if urllib.parse.urlsplit(base).query else "?"
    return base + joiner + urllib.parse.urlencode(pairs)


def _json_dumps(data: Any) -> str:
    return json.dumps(data, ensure_ascii=False, indent=2, sort_keys=False)


def _truncate(text: str, max_chars: int) -> str:
    if len(text) <= max_chars:
        return text
    omitted = len(text) - max_chars
    return text[:max_chars] + f"\n... <truncated {omitted} chars>"


@dataclass
class EndpointResult:
    name: str
    method: str
    url: str
    source_refs: list[str]
    request_headers: dict[str, str] = field(default_factory=dict)
    request_body: str | None = None
    status: int | None = None
    elapsed_ms: int | None = None
    response_headers: dict[str, str] = field(default_factory=dict)
    response_text: str = ""
    response_json: Any | None = None
    response_json_raw: Any | None = None
    business_code: Any | None = None
    ok: bool = False
    skipped: bool = False
    skip_reason: str = ""
    error: str = ""
    notes: list[str] = field(default_factory=list)

    def to_json_dict(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "method": self.method,
            "url": self.url,
            "source_refs": self.source_refs,
            "request_headers": self.request_headers,
            "request_body": self.request_body,
            "status": self.status,
            "elapsed_ms": self.elapsed_ms,
            "response_headers": self.response_headers,
            "response_text": self.response_text,
            "response_json": self.response_json,
            "business_code": self.business_code,
            "ok": self.ok,
            "skipped": self.skipped,
            "skip_reason": self.skip_reason,
            "error": self.error,
            "notes": self.notes,
        }


class EndpointCollector:
    def __init__(self, access_token: str, xx_token: str | None, timeout: float, response_limit: int):
        self.access_token = access_token
        self.xx_token = xx_token
        self.timeout = timeout
        self.response_limit = response_limit
        self.results: list[EndpointResult] = []

    def _do_request(
        self,
        *,
        name: str,
        source_refs: list[str],
        method: str,
        url: str,
        headers: dict[str, str],
        body: bytes | None,
        request_body_for_report: str | None,
    ) -> EndpointResult:
        result = EndpointResult(
            name=name,
            method=method,
            url=_sanitize_url(url),
            source_refs=source_refs,
            request_headers=_sanitize_headers(headers),
            request_body=_sanitize_text(request_body_for_report or ""),
        )
        req = urllib.request.Request(url=url, data=body, method=method, headers=headers)

        started = time.time()
        raw_body = b""
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                raw_body = resp.read()
                result.status = resp.getcode()
                result.response_headers = _sanitize_headers(dict(resp.headers.items()))
        except urllib.error.HTTPError as exc:
            result.status = exc.code
            result.response_headers = _sanitize_headers(dict(exc.headers.items()))
            raw_body = exc.read()
            result.error = f"HTTPError: {exc.reason}"
        except urllib.error.URLError as exc:
            result.error = f"URLError: {exc.reason}"
        except Exception as exc:  # noqa: BLE001
            result.error = f"{type(exc).__name__}: {exc}"
        finally:
            result.elapsed_ms = int((time.time() - started) * 1000)

        decoded_text_raw = raw_body.decode("utf-8", errors="replace")
        result.response_text = _truncate(_translate_chinese_text(_sanitize_text(decoded_text_raw)), self.response_limit)
        try:
            parsed_raw = json.loads(decoded_text_raw) if decoded_text_raw.strip() else None
            result.response_json_raw = parsed_raw
            result.response_json = _translate_obj(_sanitize_obj(parsed_raw))
            if isinstance(parsed_raw, dict) and "code" in parsed_raw:
                result.business_code = parsed_raw.get("code")
        except json.JSONDecodeError:
            result.response_json = None
            result.response_json_raw = None

        http_ok = result.status is not None and 200 <= result.status < 300
        if result.business_code is None:
            result.ok = http_ok and not result.error
        else:
            result.ok = http_ok and result.business_code == 1 and not result.error

        self.results.append(result)
        return result

    def skip(self, name: str, source_refs: list[str], method: str, url: str, reason: str) -> EndpointResult:
        result = EndpointResult(
            name=name,
            method=method,
            url=_sanitize_url(url),
            source_refs=source_refs,
            skipped=True,
            skip_reason=reason,
            ok=False,
        )
        self.results.append(result)
        return result

    def call_workbench(
        self,
        *,
        name: str,
        source_refs: list[str],
        method: str,
        path: str,
        json_body: Any | None = None,
        form_body: dict[str, Any] | None = None,
        query: dict[str, Any] | None = None,
        include_auth: bool = True,
        include_xx_headers: bool = True,
        extra_headers: dict[str, str] | None = None,
    ) -> EndpointResult:
        url = _to_url(path, query=query)
        headers: dict[str, str] = {}
        if include_auth:
            if include_xx_headers:
                headers.update(make_workbench_headers(self.access_token, self.xx_token))
            else:
                headers["Authorization"] = f"Bearer {self.access_token}"
        if extra_headers:
            headers.update(extra_headers)

        body: bytes | None = None
        request_body_for_report: str | None = None
        if json_body is not None:
            payload = json.dumps(json_body).encode("utf-8")
            headers["Content-Type"] = "application/json"
            body = payload
            request_body_for_report = _json_dumps(json_body)
        elif form_body is not None:
            encoded = urllib.parse.urlencode({k: str(v) for k, v in form_body.items()}).encode("utf-8")
            headers["Content-Type"] = "application/x-www-form-urlencoded"
            body = encoded
            request_body_for_report = urllib.parse.urlencode({k: str(v) for k, v in form_body.items()})

        return self._do_request(
            name=name,
            source_refs=source_refs,
            method=method.upper(),
            url=url,
            headers=headers,
            body=body,
            request_body_for_report=request_body_for_report,
        )

    def call_absolute(
        self,
        *,
        name: str,
        source_refs: list[str],
        method: str,
        url: str,
        body: bytes | None = None,
        headers: dict[str, str] | None = None,
        request_body_for_report: str | None = None,
    ) -> EndpointResult:
        return self._do_request(
            name=name,
            source_refs=source_refs,
            method=method.upper(),
            url=url,
            headers=headers or {},
            body=body,
            request_body_for_report=request_body_for_report,
        )


def _extract_first_id(payload: Any, keys: tuple[str, ...] = ("id",)) -> str | None:
    if isinstance(payload, dict):
        for key in keys:
            if key in payload:
                val = payload[key]
                if isinstance(val, (int, float, str)):
                    return str(val)
        data = payload.get("data")
        if isinstance(data, dict):
            return _extract_first_id(data, keys=keys)
        if isinstance(data, list):
            for item in data:
                val = _extract_first_id(item, keys=keys)
                if val:
                    return val
    return None


def _extract_first_gcode_id(payload: Any) -> str | None:
    if isinstance(payload, dict):
        if "gcode_id" in payload and isinstance(payload["gcode_id"], (int, float, str)):
            return str(payload["gcode_id"])
        data = payload.get("data")
        if isinstance(data, dict):
            return _extract_first_gcode_id(data)
        if isinstance(data, list):
            for item in data:
                val = _extract_first_gcode_id(item)
                if val:
                    return val
    return None


def _find_file_entry_by_id(payload: Any, file_id: str) -> dict[str, Any] | None:
    if not isinstance(payload, dict):
        return None
    data = payload.get("data")
    if not isinstance(data, list):
        return None
    for item in data:
        if isinstance(item, dict) and str(item.get("id")) == file_id:
            return item
    return None


def _is_non_zero_id(value: str | None) -> bool:
    if value is None:
        return False
    normalized = str(value).strip()
    return normalized not in ("", "0", "0.0", "None", "null")


def _extract_download_url(payload: Any) -> str | None:
    if isinstance(payload, dict):
        data = payload.get("data")
        if isinstance(data, str) and data.startswith("http"):
            return data
        if isinstance(data, dict):
            for key in ("url", "download_url", "preSignUrl", "presign_url"):
                value = data.get(key)
                if isinstance(value, str) and value.startswith("http"):
                    return value
    return None


def _extract_lock_info(payload: Any) -> tuple[str | None, str | None]:
    if not isinstance(payload, dict):
        return None, None
    data = payload.get("data")
    if not isinstance(data, dict):
        return None, None
    lock_id: str | None = None
    for key in ("id", "lock_id", "user_lock_space_id"):
        if key in data and isinstance(data[key], (str, int, float)):
            lock_id = str(data[key])
            break
    presign_url: str | None = None
    for key in ("preSignUrl", "presign_url", "url"):
        if key in data and isinstance(data[key], str) and data[key].startswith("http"):
            presign_url = data[key]
            break
    return lock_id, presign_url


def _sha256_file(path: Path) -> str:
    sha = hashlib.sha256()
    with path.open("rb") as fh:
        while True:
            chunk = fh.read(65536)
            if not chunk:
                break
            sha.update(chunk)
    return sha.hexdigest()


def _endpoint_sources(section: str) -> list[str]:
    return [
        f"Docs/docs_unifies_core_web_cloud_sync.md::{section}",
        "Docs/docs_unifies_core_web_cloud_sync.md",
    ]


def collect(args: argparse.Namespace) -> dict[str, Any]:
    session_path = _resolve_session_path(args.session_path)
    if not session_path.exists():
        raise FileNotFoundError(f"Session file introuvable: {session_path}")
    tokens = load_session_tokens(session_path)
    access_token = tokens.get("access_token", "")
    xx_token = tokens.get("token")
    if not access_token:
        raise RuntimeError(f"session.json invalide ({session_path}): access_token absent")

    cube_path = Path(args.cube_path)
    if not cube_path.exists():
        raise FileNotFoundError(f"Fichier cube introuvable: {cube_path}")

    collector = EndpointCollector(
        access_token=access_token,
        xx_token=xx_token,
        timeout=args.timeout,
        response_limit=args.response_limit,
    )

    # Auth / session (session.json only; OAuth browser endpoints skipped)
    collector.skip(
        name="oauth_authorize",
        source_refs=_endpoint_sources("1.1 OAuth authorize"),
        method="GET",
        url=f"{ACCOUNT_BASE_URL}/login/oauth/authorize",
        reason="Necessite flux navigateur OAuth (hors usage session.json).",
    )
    collector.skip(
        name="oauth_logout",
        source_refs=_endpoint_sources("1.2 OAuth logout"),
        method="GET",
        url=f"{ACCOUNT_BASE_URL}/api/logout",
        reason="Endpoint web base sur cookies; ignore en mode auth via session.json.",
    )
    collector.skip(
        name="oauth_token_exchange",
        source_refs=_endpoint_sources("1.3 Exchange OAuth code -> tokens"),
        method="GET",
        url=_to_url("/p/p/workbench/api/v3/public/getoauthToken?code=<oauth_code>"),
        reason="Code OAuth non disponible en mode session.json.",
    )

    collector.call_workbench(
        name="login_with_access_token",
        source_refs=_endpoint_sources("1.4 Login applicatif via access token"),
        method="POST",
        path="/p/p/workbench/api/v3/public/loginWithAccessToken",
        json_body={"access_token": access_token, "device_type": "web"},
    )
    collector.call_workbench(
        name="store_quota",
        source_refs=_endpoint_sources("3.1 Store quota"),
        method="POST",
        path="/p/p/workbench/api/work/index/getUserStore",
        json_body={},
    )

    # Profil utilisateur
    collector.call_workbench(
        name="user_info",
        source_refs=_endpoint_sources("2.1 User info"),
        method="GET",
        path="/p/p/workbench/api/user/profile/userInfo",
    )
    collector.call_workbench(
        name="device_param_get_list",
        source_refs=_endpoint_sources("2.2 Device params list"),
        method="POST",
        path="/p/p/workbench/api/user/device_param/getList",
        json_body={"page": 1, "limit": 20},
    )

    # Fichiers cloud
    files_res = collector.call_workbench(
        name="files",
        source_refs=_endpoint_sources("4.1 Listing fichiers (variante A)"),
        method="POST",
        path="/p/p/workbench/api/work/index/files",
        json_body={"page": 1, "limit": 20},
    )
    user_files_res = collector.call_workbench(
        name="user_files",
        source_refs=_endpoint_sources("4.2 Listing fichiers (variante B)"),
        method="POST",
        path="/p/p/workbench/api/work/index/userFiles",
        json_body={"page": 1, "limit": 20},
    )

    def payload_of(res: EndpointResult) -> Any:
        return res.response_json_raw if res.response_json_raw is not None else res.response_json

    existing_file_id = _extract_first_id(payload_of(files_res)) or _extract_first_id(payload_of(user_files_res))
    existing_gcode_id = _extract_first_gcode_id(payload_of(files_res)) or _extract_first_gcode_id(
        payload_of(user_files_res)
    )
    forced_file_id = str(args.gcode_file_id).strip() if args.gcode_file_id else None
    forced_gcode_id: str | None = None
    if forced_file_id:
        forced_entry = _find_file_entry_by_id(payload_of(files_res), forced_file_id) or _find_file_entry_by_id(
            payload_of(user_files_res), forced_file_id
        )
        if isinstance(forced_entry, dict):
            candidate = forced_entry.get("gcode_id")
            if isinstance(candidate, (str, int, float)):
                forced_gcode_id = str(candidate)
        if not _is_non_zero_id(forced_gcode_id):
            forced_status = collector.call_workbench(
                name="upload_status_forced_file",
                source_refs=_endpoint_sources("4.6 Statut post-upload"),
                method="POST",
                path="/p/p/workbench/api/work/index/getUploadStatus",
                json_body={"id": int(forced_file_id) if forced_file_id.isdigit() else forced_file_id},
            )
            forced_gcode_id = _extract_first_gcode_id(payload_of(forced_status))

    uploaded_lock_id: str | None = None
    uploaded_file_id: str | None = None
    uploaded_gcode_id: str | None = None
    renamed_uploaded_name: str | None = None
    downloaded_copy_path: Path | None = None
    downloaded_sha256: str | None = None
    local_sha256 = _sha256_file(cube_path)

    lock_res = collector.call_workbench(
        name="upload_lock_storage",
        source_refs=_endpoint_sources("6.1 Lock storage space"),
        method="POST",
        path="/p/p/workbench/api/v2/cloud_storage/lockStorageSpace",
        json_body={
            "name": cube_path.name,
            "size": cube_path.stat().st_size,
            "is_temp_file": 0,
        },
    )
    uploaded_lock_id, presign_url = _extract_lock_info(payload_of(lock_res))

    if not uploaded_lock_id or not presign_url:
        collector.skip(
            name="upload_put_presigned",
            source_refs=_endpoint_sources("6.2 Upload binaire direct"),
            method="PUT",
            url="<preSignUrl>",
            reason="lockStorageSpace n'a pas retourne id + preSignUrl utilisables.",
        )
        collector.skip(
            name="upload_register_file",
            source_refs=_endpoint_sources("6.3 Register uploaded file"),
            method="POST",
            url=_to_url("/p/p/workbench/api/v2/profile/newUploadFile"),
            reason="Upload binaire non lance.",
        )
    else:
        with cube_path.open("rb") as fh:
            file_bytes = fh.read()
        collector.call_absolute(
            name="upload_put_presigned",
            source_refs=_endpoint_sources("6.2 Upload binaire direct"),
            method="PUT",
            url=presign_url,
            body=file_bytes,
            headers={"Content-Type": "application/octet-stream"},
            request_body_for_report=f"<binary {len(file_bytes)} bytes>",
        )

        register_res = collector.call_workbench(
            name="upload_register_file",
            source_refs=_endpoint_sources("6.3 Register uploaded file"),
            method="POST",
            path="/p/p/workbench/api/v2/profile/newUploadFile",
            json_body={"user_lock_space_id": int(uploaded_lock_id) if uploaded_lock_id.isdigit() else uploaded_lock_id},
        )
        uploaded_file_id = _extract_first_id(payload_of(register_res))

        if uploaded_file_id:
            upload_status_res = collector.call_workbench(
                name="upload_status",
                source_refs=_endpoint_sources("4.6 Statut post-upload"),
                method="POST",
                path="/p/p/workbench/api/work/index/getUploadStatus",
                json_body={"id": int(uploaded_file_id) if uploaded_file_id.isdigit() else uploaded_file_id},
            )
            uploaded_gcode_id = _extract_first_gcode_id(payload_of(upload_status_res))

            collector.call_workbench(
                name="rename_uploaded_file",
                source_refs=_endpoint_sources("4.5 Renommage fichier"),
                method="POST",
                path="/p/p/workbench/api/work/index/renameFile",
                json_body={
                    "id": int(uploaded_file_id) if uploaded_file_id.isdigit() else uploaded_file_id,
                    "name": f"{cube_path.stem}_api_probe{cube_path.suffix}",
                },
            )
            renamed_uploaded_name = f"{cube_path.stem}_api_probe{cube_path.suffix}"

            dl_res = collector.call_workbench(
                name="download_url_for_uploaded_file",
                source_refs=_endpoint_sources("4.3 URL de telechargement signee"),
                method="POST",
                path="/p/p/workbench/api/work/index/getDowdLoadUrl",
                json_body={"id": int(uploaded_file_id) if uploaded_file_id.isdigit() else uploaded_file_id},
            )
            signed_download_url = _extract_download_url(payload_of(dl_res))
            if signed_download_url:
                downloaded_copy_path = Path(args.download_path)
                downloaded_copy_path.parent.mkdir(parents=True, exist_ok=True)
                download_res = collector.call_absolute(
                    name="download_signed_url_get",
                    source_refs=_endpoint_sources("4.3 URL de telechargement signee"),
                    method="GET",
                    url=signed_download_url,
                    body=None,
                    headers={},
                    request_body_for_report=None,
                )
                if download_res.status and 200 <= download_res.status < 300:
                    # The generic call stores text; for binary integrity, fetch a second time and save bytes.
                    try:
                        req = urllib.request.Request(signed_download_url, method="GET")
                        with urllib.request.urlopen(req, timeout=args.timeout) as resp:
                            payload = resp.read()
                        with downloaded_copy_path.open("wb") as out:
                            out.write(payload)
                        downloaded_sha256 = hashlib.sha256(payload).hexdigest()
                    except Exception as exc:  # noqa: BLE001
                        download_res.notes.append(f"Binary save failed: {type(exc).__name__}: {exc}")
            else:
                collector.skip(
                    name="download_signed_url_get",
                    source_refs=_endpoint_sources("4.3 URL de telechargement signee"),
                    method="GET",
                    url="<signed_download_url>",
                    reason="getDowdLoadUrl n'a pas retourne d'URL signee exploitable.",
                )

            collector.call_workbench(
                name="delete_uploaded_file",
                source_refs=_endpoint_sources("4.4 Suppression fichier"),
                method="POST",
                path="/p/p/workbench/api/work/index/delFiles",
                json_body={"idArr": [int(uploaded_file_id) if uploaded_file_id.isdigit() else uploaded_file_id]},
            )
        else:
            collector.skip(
                name="upload_status",
                source_refs=_endpoint_sources("4.6 Statut post-upload"),
                method="POST",
                url=_to_url("/p/p/workbench/api/work/index/getUploadStatus"),
                reason="newUploadFile n'a pas retourne de file_id.",
            )
            collector.skip(
                name="rename_uploaded_file",
                source_refs=_endpoint_sources("4.5 Renommage fichier"),
                method="POST",
                url=_to_url("/p/p/workbench/api/work/index/renameFile"),
                reason="newUploadFile n'a pas retourne de file_id.",
            )
            collector.skip(
                name="download_url_for_uploaded_file",
                source_refs=_endpoint_sources("4.3 URL de telechargement signee"),
                method="POST",
                url=_to_url("/p/p/workbench/api/work/index/getDowdLoadUrl"),
                reason="newUploadFile n'a pas retourne de file_id.",
            )
            collector.skip(
                name="download_signed_url_get",
                source_refs=_endpoint_sources("4.3 URL de telechargement signee"),
                method="GET",
                url="<signed_download_url>",
                reason="Aucune URL signee disponible.",
            )
            collector.skip(
                name="delete_uploaded_file",
                source_refs=_endpoint_sources("4.4 Suppression fichier"),
                method="POST",
                url=_to_url("/p/p/workbench/api/work/index/delFiles"),
                reason="Aucun file_id uploade a supprimer.",
            )

    if uploaded_lock_id:
        collector.call_workbench(
            name="upload_unlock_storage",
            source_refs=_endpoint_sources("6.4 Unlock storage space"),
            method="POST",
            path="/p/p/workbench/api/v2/cloud_storage/unlockStorageSpace",
            json_body={"id": int(uploaded_lock_id) if uploaded_lock_id.isdigit() else uploaded_lock_id, "is_delete_cos": 0},
        )
    else:
        collector.skip(
            name="upload_unlock_storage",
            source_refs=_endpoint_sources("6.4 Unlock storage space"),
            method="POST",
            url=_to_url("/p/p/workbench/api/v2/cloud_storage/unlockStorageSpace"),
            reason="Aucun lock id obtenu.",
        )

    # GCode info: prioritize the uploaded file, fallback to existing listing.
    gcode_id_for_info = None
    for candidate in (forced_gcode_id, uploaded_gcode_id, existing_gcode_id):
        if _is_non_zero_id(candidate):
            gcode_id_for_info = candidate
            break
    if gcode_id_for_info:
        collector.call_workbench(
            name="gcode_info",
            source_refs=_endpoint_sources("5.1 Gcode info"),
            method="GET",
            path="/p/p/workbench/api/api/work/gcode/info",
            query={"id": gcode_id_for_info},
        )
    else:
        collector.skip(
            name="gcode_info",
            source_refs=_endpoint_sources("5.1 Gcode info"),
            method="GET",
            url=_to_url("/p/p/workbench/api/api/work/gcode/info?id=<gcode_id>"),
            reason="Aucun gcode_id disponible (listing/upload).",
        )

    # Printers / projects
    printers_res = collector.call_workbench(
        name="printers_list",
        source_refs=_endpoint_sources("8.1 Liste imprimantes"),
        method="GET",
        path="/p/p/workbench/api/work/printer/getPrinters",
    )
    printer_id = _extract_first_id(payload_of(printers_res))

    if printer_id:
        if args.include_legacy_endpoints:
            collector.call_workbench(
                name="printer_info_legacy",
                source_refs=_endpoint_sources("8.2 Info imprimante (endpoint historique)"),
                method="POST",
                path="/p/p/workbench/api/work/printer/Info",
                json_body={"id": int(printer_id) if printer_id.isdigit() else printer_id},
            )
        collector.call_workbench(
            name="printer_info_v2",
            source_refs=_endpoint_sources("8.3 Info imprimante v2"),
            method="GET",
            path="/p/p/workbench/api/v2/printer/info",
            query={"id": printer_id},
        )
    else:
        if args.include_legacy_endpoints:
            collector.skip(
                name="printer_info_legacy",
                source_refs=_endpoint_sources("8.2 Info imprimante (endpoint historique)"),
                method="POST",
                url=_to_url("/p/p/workbench/api/work/printer/Info"),
                reason="Aucun printer_id trouve dans getPrinters.",
            )
        collector.skip(
            name="printer_info_v2",
            source_refs=_endpoint_sources("8.3 Info imprimante v2"),
            method="GET",
            url=_to_url("/p/p/workbench/api/v2/printer/info?id=<printer_id>"),
            reason="Aucun printer_id trouve dans getPrinters.",
        )

    collector.call_workbench(
        name="printers_status_bulk",
        source_refs=_endpoint_sources("8.4 Status imprimantes (bulk)"),
        method="GET",
        path="/p/p/workbench/api/v2/printer/printersStatus",
    )
    collector.call_workbench(
        name="printers_status_by_ext_pwmb",
        source_refs=_endpoint_sources("8.5 Compatibilite par extension (file_ext)"),
        method="GET",
        path="/p/p/workbench/api/v2/printer/printersStatus",
        query={"file_ext": "pwmb"},
    )

    file_id_for_compat = uploaded_file_id or existing_file_id
    if file_id_for_compat:
        collector.call_workbench(
            name="printers_status_by_file_id",
            source_refs=_endpoint_sources("8.6 Compatibilite par identifiant (file_id)"),
            method="GET",
            path="/p/p/workbench/api/v2/printer/printersStatus",
            query={"file_id": file_id_for_compat},
        )
    else:
        collector.skip(
            name="printers_status_by_file_id",
            source_refs=_endpoint_sources("8.6 Compatibilite par identifiant (file_id)"),
            method="GET",
            url=_to_url("/p/p/workbench/api/v2/printer/printersStatus?file_id=<id>"),
            reason="Aucun file_id disponible.",
        )

    projects_query: dict[str, Any] = {"limit": 10, "page": 1}
    if printer_id:
        projects_query["printer_id"] = printer_id
    projects_res = collector.call_workbench(
        name="projects_list",
        source_refs=_endpoint_sources("7.2 Liste projets (jobs)"),
        method="GET",
        path="/p/p/workbench/api/work/project/getProjects",
        query=projects_query,
    )
    project_id = _extract_first_id(payload_of(projects_res))

    if project_id:
        collector.call_workbench(
            name="project_info_v2",
            source_refs=_endpoint_sources("7.3 Detail projet"),
            method="GET",
            path="/p/p/workbench/api/v2/project/info",
            query={"id": project_id},
        )
        collector.call_workbench(
            name="project_report",
            source_refs=_endpoint_sources("7.3 Detail projet"),
            method="GET",
            path="/p/p/workbench/api/work/project/report",
            query={"id": project_id},
        )
    else:
        collector.skip(
            name="project_info_v2",
            source_refs=_endpoint_sources("7.3 Detail projet"),
            method="GET",
            url=_to_url("/p/p/workbench/api/v2/project/info?id=<project_id>"),
            reason="Aucun project_id trouve dans getProjects.",
        )
        collector.skip(
            name="project_report",
            source_refs=_endpoint_sources("7.3 Detail projet"),
            method="GET",
            url=_to_url("/p/p/workbench/api/work/project/report?id=<task_id>"),
            reason="Aucun project_id/task_id disponible.",
        )

    if args.allow_send_order and printer_id and file_id_for_compat:
        collector.call_workbench(
            name="send_print_order",
            source_refs=_endpoint_sources("7.1 Send print order"),
            method="POST",
            path="/p/p/workbench/api/work/operation/sendOrder",
            form_body={
                "printer_id": printer_id,
                "project_id": 0,
                "order_id": 1,
                "is_delete_file": 0,
                "data": json.dumps(
                    {
                        "file_id": str(file_id_for_compat),
                        "matrix": "",
                        "filetype": 0,
                        "project_type": 1,
                        "template_id": -2074360784,
                    },
                    ensure_ascii=False,
                ),
            },
        )
    else:
        collector.skip(
            name="send_print_order",
            source_refs=_endpoint_sources("7.1 Send print order"),
            method="POST",
            url=_to_url("/p/p/workbench/api/work/operation/sendOrder"),
            reason="Desactive par defaut (action intrusive). Utiliser --allow-send-order.",
        )

    # Messages / telemetry / reason catalog
    collector.call_workbench(
        name="message_count",
        source_refs=[
            "Docs/docs_unifies_core_web_cloud_sync.md::2) Endpoints presents dans end_points.md mais non utilises directement par la v2 Python",
            "Docs/docs_unifies_core_web_cloud_sync.md",
        ],
        method="GET",
        path="/p/p/workbench/api/v2/message/getMessageCount",
    )
    collector.call_workbench(
        name="messages_list",
        source_refs=_endpoint_sources("9.1 Liste messages"),
        method="POST",
        path="/p/p/workbench/api/v2/message/getMessages",
        json_body={"page": 1, "limit": 20},
    )
    collector.call_workbench(
        name="project_print_history",
        source_refs=[
            "Docs/docs_unifies_core_web_cloud_sync.md::2) Endpoints presents dans end_points.md mais non utilises directement par la v2 Python",
            "Docs/docs_unifies_core_web_cloud_sync.md",
        ],
        method="GET",
        path="/p/p/workbench/api/v2/project/printHistory",
        query={"page": 1, "limit": 10},
    )
    collector.call_workbench(
        name="buried_report",
        source_refs=_endpoint_sources("10.1 Buried report"),
        method="POST",
        path="/j/p/buried/buried/report",
        json_body={},
        include_xx_headers=False,
    )
    collector.call_workbench(
        name="reason_catalog",
        source_refs=_endpoint_sources("11.1 Reasons catalog"),
        method="GET",
        path="/p/p/workbench/api/portal/index/reason",
    )

    summary = {
        "generated_at_utc": dt.datetime.now(dt.UTC).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
        "session_path": str(session_path),
        "cube_path": str(cube_path),
        "download_path": str(args.download_path),
        "gcode_file_id_arg": str(args.gcode_file_id) if args.gcode_file_id else None,
        "include_legacy_endpoints": bool(args.include_legacy_endpoints),
        "upload_sha256": local_sha256,
        "download_sha256": downloaded_sha256,
        "download_matches_upload": bool(downloaded_sha256 and downloaded_sha256 == local_sha256),
        "uploaded_lock_id": uploaded_lock_id,
        "uploaded_file_id": uploaded_file_id,
        "renamed_uploaded_name": renamed_uploaded_name,
    }

    return {
        "summary": _sanitize_obj(summary),
        "results": [res.to_json_dict() for res in collector.results],
    }


def render_markdown(report: dict[str, Any]) -> str:
    summary = report["summary"]
    results = report["results"]
    total = len(results)
    ok_count = sum(1 for r in results if r.get("ok"))
    skipped = [r for r in results if r.get("skipped")]
    failed = [r for r in results if not r.get("ok") and not r.get("skipped")]

    lines: list[str] = []
    lines.append("# Capture des retours endpoints Anycubic")
    lines.append("")
    lines.append("## Contexte")
    lines.append(f"- Genere le: `{summary['generated_at_utc']}`")
    lines.append("- Sources endpoints: `Docs/docs_unifies_core_web_cloud_sync.md`, `Docs/docs_unifies_core_web_cloud_sync.md`")
    lines.append(f"- Session utilisee: `{summary['session_path']}`")
    lines.append(f"- Fichier test upload: `{summary['cube_path']}`")
    lines.append(f"- Fichier telecharge: `{summary['download_path']}`")
    lines.append(f"- SHA256 upload: `{summary['upload_sha256']}`")
    lines.append(f"- SHA256 download: `{summary['download_sha256']}`")
    lines.append(f"- Integrite upload/download: `{summary['download_matches_upload']}`")
    lines.append("")
    lines.append("## Resume")
    lines.append(f"- Endpoints traites: `{total}`")
    lines.append(f"- Succes: `{ok_count}`")
    lines.append(f"- Echecs: `{len(failed)}`")
    lines.append(f"- Skippes: `{len(skipped)}`")
    lines.append("- Filtre rapport: `succes uniquement + send_print_order`")
    lines.append("")

    if failed:
        lines.append("## Echecs")
        for item in failed:
            lines.append(
                f"- `{item['name']}` `{item['method']}` `{item['url']}` -> "
                f"status `{item.get('status')}` error `{item.get('error', '')}`"
            )
        lines.append("")

    if skipped:
        lines.append("## Skips")
        for item in skipped:
            lines.append(f"- `{item['name']}`: {item.get('skip_reason', '')}")
        lines.append("")

    lines.append("## Details")
    for idx, item in enumerate(results, start=1):
        status = "SKIPPED" if item.get("skipped") else ("OK" if item.get("ok") else "FAILED")
        lines.append(f"### {idx}. {item['name']} - {status}")
        lines.append(f"- Endpoint: `{item['method']} {item['url']}`")
        lines.append(f"- Source: `{', '.join(item.get('source_refs', []))}`")
        if item.get("status") is not None:
            lines.append(f"- HTTP status: `{item['status']}`")
        if item.get("business_code") is not None:
            lines.append(f"- Business code: `{item['business_code']}`")
        if item.get("elapsed_ms") is not None:
            lines.append(f"- Duree: `{item['elapsed_ms']} ms`")
        if item.get("skip_reason"):
            lines.append(f"- Raison du skip: {item['skip_reason']}")
        if item.get("error"):
            lines.append(f"- Erreur: `{item['error']}`")
        if item.get("request_body"):
            lines.append("- Request body:")
            lines.append("```json")
            lines.append(_truncate(item["request_body"], 4000))
            lines.append("```")
        response_json = item.get("response_json")
        if response_json is not None:
            lines.append("- Response JSON:")
            lines.append("```json")
            lines.append(_truncate(_json_dumps(response_json), 12000))
            lines.append("```")
        elif item.get("response_text"):
            lines.append("- Response texte:")
            lines.append("```text")
            lines.append(_truncate(item["response_text"], 6000))
            lines.append("```")
        lines.append("")

    return "\n".join(lines).rstrip() + "\n"


def filter_results_for_export(report: dict[str, Any]) -> dict[str, Any]:
    kept = [
        r
        for r in report.get("results", [])
        if r.get("ok") or r.get("name") == "send_print_order"
    ]
    out = {
        "summary": dict(report.get("summary", {})),
        "results": kept,
    }
    out["summary"]["filtered_for_user"] = "success_only_plus_send_print_order"
    out["summary"]["filtered_result_count"] = len(kept)
    return out


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Execute les endpoints Anycubic documentes dans Docs/docs_unifies_core_web_cloud_sync.md + "
            "Docs/docs_unifies_core_web_cloud_sync.md et produit un rapport."
        )
    )
    parser.add_argument("--session-path", default=None, help="Chemin vers session.json.")
    parser.add_argument(
        "--cube-path",
        default="Docs/photon_files/cube.pwmb",
        help="Fichier binaire utilise pour le workflow upload/download/delete.",
    )
    parser.add_argument(
        "--download-path",
        default="/tmp/anycubic_cube_downloaded.pwmb",
        help="Chemin de sauvegarde du fichier telecharge.",
    )
    parser.add_argument(
        "--report-file",
        default="Docs/endpoints_capture_report.md",
        help="Fichier markdown de documentation des retours.",
    )
    parser.add_argument(
        "--json-file",
        default="Docs/endpoints_capture_report.json",
        help="Fichier JSON brut/sanitise des retours.",
    )
    parser.add_argument("--timeout", type=float, default=30.0, help="Timeout HTTP en secondes.")
    parser.add_argument(
        "--response-limit",
        type=int,
        default=20000,
        help="Nombre maximal de caracteres stockes par reponse texte.",
    )
    parser.add_argument(
        "--gcode-file-id",
        default=None,
        help="Force la verification gcode_info via un fichier cloud existant (file id).",
    )
    parser.add_argument(
        "--include-legacy-endpoints",
        action="store_true",
        help="Reactive explicitement les endpoints legacy (desactives par defaut).",
    )
    parser.add_argument(
        "--allow-send-order",
        action="store_true",
        help="Autorise l'appel sendOrder (impression distante). Desactive par defaut.",
    )
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    raw_report = collect(args)
    report = filter_results_for_export(raw_report)
    markdown = render_markdown(report)

    report_path = Path(args.report_file)
    json_path = Path(args.json_file)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.parent.mkdir(parents=True, exist_ok=True)

    with json_path.open("w", encoding="utf-8") as fh:
        json.dump(report, fh, ensure_ascii=False, indent=2)
        fh.write("\n")

    with report_path.open("w", encoding="utf-8") as fh:
        fh.write(markdown)

    print(f"[ok] Report markdown: {report_path}")
    print(f"[ok] Report JSON: {json_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
