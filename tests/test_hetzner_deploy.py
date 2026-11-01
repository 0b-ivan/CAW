"""Static and shell-guard tests for the opt-in Hetzner CD pipeline."""
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "bin" / "deploy-hetzner"
WORKFLOW = ROOT / ".github" / "workflows" / "deploy-hetzner.yml"


class DeploymentSafetyTests(unittest.TestCase):
    def test_deployment_script_parses_as_bash(self):
        subprocess.run(["bash", "-n", str(SCRIPT)], check=True)

    def test_invalid_image_tag_is_rejected_before_any_changes(self):
        result = subprocess.run(
            ["bash", str(SCRIPT), "latest", "test-user"],
            cwd=ROOT, capture_output=True, text=True, check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("immutable image tag", result.stderr)

    def test_script_never_downs_database_or_removes_volumes(self):
        content = SCRIPT.read_text()
        self.assertIn("flock -n", content)
        self.assertIn("git merge --ff-only", content)
        self.assertIn("git reset --hard", content)
        self.assertIn("compose exec -T backup /bin/sh /backup.sh once", content)
        self.assertIn("compose pull server", content)
        self.assertIn("up --no-deps --no-build", content)
        self.assertIn("server caddy", content)
        self.assertNotIn("docker compose down", content)
        self.assertNotIn("--volumes", content)
        self.assertNotIn("bin/smoke", content)

    def test_pipeline_gates_real_production_access(self):
        workflow = WORKFLOW.read_text()
        self.assertIn("workflow_run:", workflow)
        self.assertIn("workflow_dispatch:", workflow)
        self.assertIn("vars.CAW_PRODUCTION_APPROVED == 'true'", workflow)
        self.assertIn("vars.CAW_AUTO_DEPLOY == 'true'", workflow)
        self.assertIn("github.event.workflow_run.event == 'push'", workflow)
        self.assertIn("github.event.workflow_run.head_branch == 'main'", workflow)
        self.assertIn("github.event.workflow_run.conclusion == 'success'", workflow)
        self.assertIn("environment: production", workflow)
        self.assertIn("StrictHostKeyChecking=yes", workflow)
        self.assertIn("github.event.workflow_run.head_sha", workflow)
        self.assertIn("ghcr.io/0b-ivan/caw-server:sha-$DEPLOY_SHA", workflow)
        self.assertNotIn("StrictHostKeyChecking=no", workflow)
        self.assertNotIn("bin/smoke", workflow)


if __name__ == "__main__":
    unittest.main()
