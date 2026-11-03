"""Verify the isolated reverse switch never runs on the production Compose stack."""
import os
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ROLLBACK = ROOT / "bin" / "test-bluegreen-rollback"


class BlueGreenRollbackTests(unittest.TestCase):
    def test_rejects_invocation_without_isolated_ci_identity(self):
        env = os.environ.copy()
        env.pop("CAW_BLUEGREEN_CI", None)
        env.pop("COMPOSE_PROJECT_NAME", None)
        result = subprocess.run(["bash", str(ROLLBACK)], cwd=ROOT, env=env,
                                text=True, capture_output=True, check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("isolated CI project", result.stderr)

    def test_green_is_stopped_before_blue_resumes_and_caddy_follows(self):
        text = ROLLBACK.read_text()
        subprocess.run(["bash", "-n", str(ROLLBACK)], check=True)
        self.assertIn('active_sockets', text)
        self.assertIn("for attempt in $(seq 1 40)", text)
        self.assertIn("refuse to stop Green while clients remain connected", text)
        self.assertLess(text.index("stop -t 30 server-green"),
                        text.index("--wait-timeout 90 server"))
        self.assertLess(text.index("--wait-timeout 90 server"),
                        text.index("--wait-timeout 90 caddy"))
        self.assertLess(text.index("restored_rooms="),
                        text.index("--wait-timeout 90 caddy"))
        self.assertIn("Green-created room on Blue", text)
        self.assertIn("/internal/handoff/quiesce", text)
        self.assertLess(text.index("/internal/handoff/quiesce"),
                        text.index("stop -t 30 server-green"))
        self.assertNotIn("docker compose down", text)
        self.assertNotIn("--volumes", text)
        self.assertNotIn("FLUSHALL", text)
        self.assertNotIn("restart redis", text)
        self.assertNotIn("restart postgres", text)


if __name__ == "__main__":
    unittest.main()
