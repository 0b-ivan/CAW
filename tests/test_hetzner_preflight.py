"""Offline checks of CAW's loopback origin for an existing Hetzner Tunnel."""

import importlib.util
import json
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
        deploy.mkdir()
        (root / "web").mkdir()
        (deploy / ".env").write_text(
            "POSTGRES_PASSWORD=" + "a" * 64
            + "\nPUBLIC_URL=https://caw.obivan.org\n"
            + "CADDY_ADDRESS=:80\nCAW_ORIGIN_PORT=18081\n",
            encoding="utf-8",
        )
        (root / "web" / "operator.json").write_text(
            json.dumps({"name": "Synthetic Operator", "address": "Test Lane 1",
                        "email": "operator@example.invalid"}), encoding="utf-8"
        )
        (deploy / "Caddyfile.hetzner").write_text(
            "respond @metrics 404\n"
            "header_up X-Forwarded-For {http.request.header.Cf-Connecting-Ip}\n",
            encoding="utf-8",
        )

    def valid_model(self):
        return {
            "services": {
                "caddy": {"ports": [{"host_ip": "127.0.0.1", "published": "18081",
                                     "target": 80, "protocol": "tcp"}]},
                "server": {},
                "postgres": {},
                "redis": {},
                "prometheus": {"ports": [{"host_ip": "127.0.0.1",
                                          "published": "9090", "target": 9090}]},
            },
        }

    def test_existing_tunnel_needs_no_token_or_second_connector(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_root(root)
            self.assertEqual(preflight.check_files(root), [])
            self.assertEqual(preflight.check_caddy(root), [])
            self.assertEqual(preflight.check_compose(self.valid_model()), [])

    def test_operator_details_are_required_for_go_live(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_root(root)
            (root / "web" / "operator.json").unlink()
            self.assertTrue(any("operator.json" in error
                                for error in preflight.check_files(root)))
            self.assertEqual(preflight.check_files(root, require_operator=False), [])

    def test_bad_password_origin_and_tunnel_token_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_root(root)
            (root / "deploy" / ".env").write_text(
                "POSTGRES_PASSWORD=replace-me\nPUBLIC_URL=http://caw.obivan.org\n"
                "CADDY_ADDRESS=caw.obivan.org\nTUNNEL_TOKEN=unsafe\n",
                encoding="utf-8",
            )
            self.assertGreaterEqual(len(preflight.check_files(root)), 4)

    def test_caddy_must_bind_exactly_one_loopback_port(self):
        model = self.valid_model()
        self.assertEqual(preflight.check_compose(model), [])
        model["services"]["caddy"]["ports"][0]["host_ip"] = "0.0.0.0"
        self.assertTrue(preflight.check_compose(model))
        model["services"]["caddy"]["ports"][0]["host_ip"] = "127.0.0.1"
        model["services"]["caddy"]["ports"][0]["published"] = "8080"
        self.assertTrue(preflight.check_compose(model, expected_port="18081"))

    def test_duplicate_cloudflared_service_is_rejected(self):
        model = self.valid_model()
        model["services"]["cloudflared"] = {"image": "cloudflare/cloudflared"}
        self.assertTrue(any("zweiten cloudflared" in error
                            for error in preflight.check_compose(model)))

    def test_public_prometheus_port_is_rejected(self):
        model = self.valid_model()
        model["services"]["prometheus"]["ports"][0]["host_ip"] = "0.0.0.0"
        self.assertTrue(any("Prometheus" in error
                            for error in preflight.check_compose(model)))

    def test_metrics_denial_must_not_depend_on_client_ip(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_root(root)
            caddy = root / "deploy" / "Caddyfile.hetzner"
            caddy.write_text("remote_ip private_ranges\n", encoding="utf-8")
            self.assertTrue(preflight.check_caddy(root))
            caddy.write_text("# remote_ip private_ranges is unsafe\n"
                             "respond @metrics 404\n"
                             "header_up X-Forwarded-For "
                             "{http.request.header.Cf-Connecting-Ip}\n",
                             encoding="utf-8")
            self.assertEqual(preflight.check_caddy(root), [])


if __name__ == "__main__":
    unittest.main()
