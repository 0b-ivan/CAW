"""Safety guards for the isolated Blue/Green takeover rehearsal."""
import os
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REHEARSAL = ROOT / "bin" / "test-bluegreen-reconnect"
OVERLAY = ROOT / "deploy" / "compose.bluegreen.ci.yml"


class BlueGreenRehearsalTests(unittest.TestCase):
    def test_rehearsal_rejects_any_non_ci_invocation_before_touching_docker(self):
        env = os.environ.copy()
        env.pop("CAW_BLUEGREEN_CI", None)
        env.pop("COMPOSE_PROJECT_NAME", None)
        result = subprocess.run(
            ["bash", str(REHEARSAL)],
            cwd=ROOT,
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("isolated CI project", result.stderr)

    def test_blue_is_stopped_before_green_is_active_or_caddy_switches(self):
        content = REHEARSAL.read_text()
        subprocess.run(["bash", "-n", str(REHEARSAL)], check=True)
        self.assertIn('"caw-ci"', content)
        self.assertIn('stop -t 30 server', content)
        self.assertIn("docker inspect --format", content)
        self.assertIn("http://127.0.0.1:18082/readyz", content)
        self.assertIn("/internal/handoff/quiesce", content)
        self.assertIn('remote_status" != 403', content)
        self.assertIn('ready_status" != 503', content)
        self.assertIn('create_status" != 503', content)
        server = (ROOT / "server" / "src" / "net" / "Server.cpp").read_text()
        self.assertIn("PersistQueue& persist, const bool& quiesced", server)
        self.assertRegex(server, r"&quiesced,\s*request")
        self.assertIn("if (quiesced) {", server)
        self.assertIn("isLoopbackAddress(response->getRemoteAddress())", server)
        self.assertNotIn("response->getRemoteAddressAsText()", server.split("void Server::registerHandoffRoute")[1].split("void Server::registerProbeSocket")[0])
        self.assertLess(content.index("/internal/handoff/quiesce"),
                        content.index("stop -t 30 server"))
        self.assertIn('awk \'$1 == "caw_sessions" { print $2 }\'', content)
        self.assertIn("Green did not restore every room", content)
        self.assertLess(content.index("restored_rooms="),
                        content.index("--wait-timeout 90 caddy"))
        self.assertLess(content.index("stop -t 30 server"),
                        content.index("--wait-timeout 90 server-green"))
        self.assertLess(content.index("--wait-timeout 90 server-green"),
                        content.index("--wait-timeout 90 caddy"))
        for forbidden in ("docker compose down", "--volumes", "FLUSHALL",
                          "DEL session:", "restart redis", "restart postgres"):
            self.assertNotIn(forbidden, content)

    def test_only_test_overlay_activates_green_and_reroutes_caddy(self):
        overlay = OVERLAY.read_text()
        stable = (ROOT / "deploy" / "compose.bluegreen.yml").read_text()
        caddy = (ROOT / "deploy" / "Caddyfile").read_text()
        hetzner = (ROOT / "deploy" / "Caddyfile.hetzner").read_text()
        self.assertIn('CAW_STANDBY: "0"', overlay)
        self.assertIn("x-ci-rehearsal: ${CAW_BLUEGREEN_CI:?", overlay)
        self.assertIn("CAW_UPSTREAM: server-green:8080", overlay)
        self.assertIn("127.0.0.1:18082:8080", overlay)
        self.assertIn('CAW_STANDBY: "1"', stable)
        self.assertNotIn("CAW_UPSTREAM:", stable)
        self.assertIn("reverse_proxy {$CAW_UPSTREAM:server:8080}", caddy)
        self.assertNotIn("/internal/handoff", caddy)
        self.assertNotIn("/internal/handoff", hetzner)
        self.assertIn("reverse_proxy server:8080", hetzner)


if __name__ == "__main__":
    unittest.main()
