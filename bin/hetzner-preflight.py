#!/usr/bin/env python3
"""Preflight for the Hetzner + Cloudflare Tunnel deployment (no secret output)."""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import urlopen


ROOT = Path(__file__).resolve().parents[1]
DOMAIN = "caw.obivan.org"
COMPOSE = ["docker", "compose", "-p", "caw", "-f", "deploy/compose.yml", "-f",
           "deploy/compose.hetzner.yml"]


def configuration(path):
    result = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        key, separator, value = line.partition("=")
        if not separator:
            raise ValueError("Malformed environment entry")
        result[key.strip()] = value.strip().strip('"').strip("'")
    return result


def check_files(root, require_operator=True):
    errors = []
    env_path = root / "deploy" / ".env"
    operator_path = root / "web" / "operator.json"
    if not env_path.is_file():
        errors.append("deploy/.env fehlt (Vorlage: deploy/hetzner/env.example)")
    else:
        settings = configuration(env_path)
        secret = settings.get("POSTGRES_PASSWORD", "")
        if len(secret) < 32 or not re.fullmatch(r"[A-Za-z0-9_~.-]+", secret):
            errors.append("POSTGRES_PASSWORD muss lang und URL-sicher sein")
        if settings.get("PUBLIC_URL") != f"https://{DOMAIN}":
            errors.append("PUBLIC_URL muss https://caw.obivan.org sein")
        if settings.get("CADDY_ADDRESS") != ":80":
            errors.append("CADDY_ADDRESS muss :80 sein (TLS am Cloudflare-Edge)")
        if "CLOUDFLARE_TUNNEL_TOKEN" in settings or "TUNNEL_TOKEN" in settings:
            errors.append("Bestehendes Tunnel-Token nicht in CAW .env kopieren")
        port = settings.get("CAW_ORIGIN_PORT", "18081")
        if not port.isdigit() or not (1024 <= int(port) <= 65535):
            errors.append("CAW_ORIGIN_PORT muss ein freier TCP-Port von 1024 bis 65535 sein")
    if not operator_path.is_file():
        if require_operator:
            errors.append("web/operator.json fehlt (Impressum vor Go-live ergänzen)")
    else:
        try:
            operator = json.loads(operator_path.read_text(encoding="utf-8"))
            if not all(isinstance(operator.get(field), str) and operator[field].strip()
                       for field in ("name", "address", "email")):
                errors.append("Impressum benötigt Name, Anschrift und E-Mail")
            elif "@" not in operator["email"]:
                errors.append("E-Mail im Impressum ist ungültig")
        except (OSError, ValueError):
            errors.append("web/operator.json ist kein gültiges JSON")
    return errors


def check_compose(model, expected_port="18081"):
    errors = []
    services = model.get("services", {})
    caddy = services.get("caddy", {})
    ports = caddy.get("ports", [])
    if len(ports) != 1:
        errors.append("Caddy muss exakt einen Loopback-Port haben")
    else:
        port = ports[0]
        if (port.get("host_ip") != "127.0.0.1" or
                str(port.get("published")) != str(expected_port) or
                port.get("target") != 80):
            errors.append("Caddy-Origin darf nur 127.0.0.1:PORT:80 binden")
    if "cloudflared" in services:
        errors.append("CAW darf keinen zweiten cloudflared-Dienst starten")
    for name in ("server", "redis", "postgres", "backup"):
        if services.get(name, {}).get("ports"):
            errors.append(f"{name} darf keine Host-Ports haben")
    for port in services.get("prometheus", {}).get("ports", []):
        if port.get("host_ip") not in ("127.0.0.1", "::1"):
            errors.append("Prometheus darf nur auf Loopback veröffentlicht werden")
    return errors

def check_caddy(root):
    text = (root / "deploy" / "Caddyfile.hetzner").read_text(encoding="utf-8")
    errors = []
    if "respond @metrics 404" not in text:
        errors.append("Tunnel-Caddy muss /metrics ausnahmslos blockieren")
    active_lines = [line.strip() for line in text.splitlines()
                    if line.strip() and not line.lstrip().startswith("#")]
    if any(line.startswith("remote_ip private_ranges") for line in active_lines):
        errors.append("IP-Range-Ausnahmen für /metrics sind hinter Tunnel unsicher")
    if "header_up X-Forwarded-For {http.request.header.Cf-Connecting-Ip}" not in text:
        errors.append("Echte Client-IP wird für Rate-Limits nicht weitergereicht")
    return errors


def resolved_compose(root):
    command = COMPOSE + ["config", "--format", "json"]
    try:
        result = subprocess.run(command, cwd=root, capture_output=True,
                                check=False, text=True)
    except FileNotFoundError as error:
        raise ValueError("Docker Compose fehlt") from error
    if result.returncode:
        # Docker may echo secrets in diagnostic output. Don't print stdout/stderr.
        raise ValueError("Docker Compose kann Produktionskonfiguration nicht laden")
    return json.loads(result.stdout)


def public_probe(url, allowed):
    try:
        with urlopen(url, timeout=12) as response:
            code = response.status
            body = response.read(4096)
    except HTTPError as error:
        code = error.code
        body = b""
    except URLError:
        return f"Öffentliche Verbindung zu {url} nicht erreichbar"
    if code not in allowed:
        return f"{url} liefert HTTP {code}, erwartet {sorted(allowed)}"
    if url.endswith("/healthz") and b"ok" not in body:
        return "Healthcheck gibt keine Bereitschaft zurück"
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--public", action="store_true",
                        help="Check edge TLS and safe public routes after DNS/Tunnel setup")
    parser.add_argument("--prepare", action="store_true",
                        help="Check local configuration before operator details are ready")
    args = parser.parse_args()

    errors = check_files(ROOT, require_operator=(not args.prepare or args.public)) + check_caddy(ROOT)
    try:
        settings = configuration(ROOT / "deploy" / ".env")
        expected_port = settings.get("CAW_ORIGIN_PORT", "18081")
        errors += check_compose(resolved_compose(ROOT), expected_port)
    except (ValueError, json.JSONDecodeError) as error:
        errors.append(str(error))

    if args.public:
        for path, expected in (("/", {200}), ("/healthz", {200}),
                               ("/metrics", {404}), ("/api/sessions/ZZZZZZ", {404})):
            result = public_probe(f"https://{DOMAIN}{path}", expected)
            if result:
                errors.append(result)

    if errors:
        for error in errors:
            print("FAIL:", error)
        return 1
    print("OK: CAW-Compose und Loopback-Origin; Host-Tunnel separat prüfen")
    if args.prepare:
        print("HINWEIS: Impressum vor Livebetrieb separat prüfen")
    if args.public:
        print("OK: öffentliches HTTPS, Healthcheck und /metrics-Sperre")
        print("WSS muss zusätzlich mit node bin/hetzner-wss-check.mjs geprüft werden")
    return 0


if __name__ == "__main__":
    sys.exit(main())
