"""Offline checks of the Hetzner Cloudflare Tunnel deployment safeguards."""

import importlib.util
import json
import os
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "bin" / "hetzner-preflight.py"
spec = importlib.util.spec_from_file_location("hetzner_preflight", SCRIPT)
preflight = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preflight)


class HetznerPreflightTests(unittest.TestCase):
    def make_root(self, root):
        deploy = root / "deploy"
        (deploy / "secrets").mkdir(parents=True)
        (root / "web").mkdir()
        (deploy / ".env").write_text(
            "POSTGRES_PASSWORD=" + "a" * 64
            + "\nPUBLIC_URL=https://caw.obivan.org\nCADDY_ADDRESS=:80\n",
            encoding="utf-8",
        )
        token = deploy / "secrets" / "cloudflared-token"
        token.write_text("t" * 80, encoding="utf-8")
        token.chmod(0o600)
        (root / "web" / "operator.json").write_text(
            json.dumps({"name": "Synthetic Operator", "address": "Test Lane 1",
                        "email": "operator@example.invalid"}), encoding="utf-8"
        )
        (deploy / "Caddyfile.hetzner").write_text(
            "respond @metrics 404\n"
            "header_up X-Forwarded-For {http.request.header.Cf-Connecting-Ip}\n",
            encoding="utf-8",
        )

    def test_valid_synthetic_configuration(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_root(root)
            self.assertEqual(preflight.check_files(root), [])
            self.assertEqual(preflight.check_caddy(root), [])

    def test_missing_operator_and_token_are_blockers(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_root(root)
            (root / "web" / "operator.json").unlink()
            (root / "deploy" / "secrets" / "cloudflared-token").unlink()
            errors = preflight.check_files(root)
            self.assertTrue(any("operator.json" in error for error in errors))
            self.assertTrue(any("cloudflared-token" in error for error in errors))

    def test_bad_secrets_and_token_permissions_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_root(root)
            (root / "deploy" / ".env").write_text(
                "POSTGRES_PASSWORD=replace-me\nPUBLIC_URL=http://caw.obivan.org\n"
                "CADDY_ADDRESS=caw.obivan.org\nTUNNEL_TOKEN=unsafe\n",
                encoding="utf-8",
            )
            (root / "deploy" / "secrets" / "cloudflared-token").chmod(0o644)
            errors = preflight.check_files(root)
            self.assertGreaterEqual(len(errors), 4)

    def test_compose_exposes_no_origin_ports(self):
        model = {
            "services": {
                "caddy": {"ports": []},
                "server": {},
                "cloudflared": {"ports": []},
                "postgres": {},
                "redis": {},
                "prometheus": {"ports": [{"host_ip": "127.0.0.1",
                                          "published": "9090", "target": 9090}]},
            },
            "secrets": {"cloudflared_token": {"file": "local-secret"}},
        }
        self.assertEqual(preflight.check_compose(model), [])
        model["services"]["caddy"]["ports"] = [{"published": "80", "target": 80}]
        self.assertTrue(preflight.check_compose(model))

    def test_public_prometheus_port_is_rejected(self):
        model = {
            "services": {
                "caddy": {}, "server": {}, "cloudflared": {},
                "prometheus": {"ports": [{"host_ip": "0.0.0.0",
                                          "published": "9090", "target": 9090}]},
            },
            "secrets": {"cloudflared_token": {}},
        }
        self.assertTrue(any("Prometheus" in error
                            for error in preflight.check_compose(model)))

    def test_tunnel_origin_requires_unconditional_metrics_denial(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_root(root)
            caddy = root / "deploy" / "Caddyfile.hetzner"
            caddy.write_text("remote_ip private_ranges\n", encoding="utf-8")
            self.assertTrue(preflight.check_caddy(root))


if __name__ == "__main__":
    unittest.main()
