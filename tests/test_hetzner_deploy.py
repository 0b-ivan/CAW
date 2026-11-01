"""Static and shell-guard tests for the opt-in Hetzner CD pipeline."""
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "bin" / "deploy-hetzner"
ROLLBACK = ROOT / "bin" / "rollback-hetzner"
CI_GATE = ROOT / "bin" / "verify-main-ci"
TEST_SHA = "a" * 40
WORKFLOW = ROOT / ".github" / "workflows" / "deploy-hetzner.yml"


class DeploymentSafetyTests(unittest.TestCase):
    def test_deployment_script_parses_as_bash(self):
        subprocess.run(["bash", "-n", str(SCRIPT)], check=True)
        subprocess.run(["bash", "-n", str(ROLLBACK)], check=True)
        subprocess.run(["bash", "-n", str(CI_GATE)], check=True)

    def test_invalid_image_tag_is_rejected_before_any_changes(self):
        result = subprocess.run(
            ["bash", str(SCRIPT), "latest", "test-user"],
            cwd=ROOT, capture_output=True, text=True, check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("immutable image tag", result.stderr)

    def test_rollback_rejects_bad_commit_without_changes(self):
        result = subprocess.run(
            ["bash", str(ROLLBACK), "not-a-commit"],
            cwd=ROOT, capture_output=True, text=True, check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("invalid expected deployment SHA", result.stderr)

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
        rollback = ROLLBACK.read_text()
        self.assertIn("flock -n", rollback)
        self.assertIn("git reset --hard", rollback)
        self.assertIn("up --no-deps --no-build", rollback)
        self.assertNotIn("--volumes", rollback)

    def test_pipeline_gates_real_production_access(self):
        workflow = WORKFLOW.read_text()
        self.assertIn("push:\n    branches: [main]", workflow)
        self.assertNotIn("workflow_run:", workflow)
        self.assertIn("workflow_dispatch:", workflow)
        self.assertIn("vars.CAW_PRODUCTION_APPROVED == 'true'", workflow)
        self.assertIn("vars.CAW_AUTO_DEPLOY == 'true'", workflow)
        self.assertIn("github.ref == 'refs/heads/main'", workflow)
        self.assertIn('bash bin/verify-main-ci "$DEPLOY_SHA"', workflow)
        self.assertIn("environment: production", workflow)
        self.assertIn("StrictHostKeyChecking=yes", workflow)
        self.assertIn("steps.wss.outcome == 'failure'", workflow)
        self.assertIn("bin/rollback-hetzner", workflow)
        self.assertIn('DEPLOY_SHA: ${{ github.sha }}', workflow)
        self.assertIn("ghcr.io/0b-ivan/caw-server:sha-$DEPLOY_SHA", workflow)
        self.assertNotIn("StrictHostKeyChecking=no", workflow)
        self.assertNotIn("bin/smoke", workflow)


    def gate(self, runs, requested_sha=TEST_SHA, current_sha=TEST_SHA):
        """Run the real gate with a local fake of the GitHub API, never the network."""
        with tempfile.TemporaryDirectory() as directory:
            fake = Path(directory) / "gh"
            fake.write_text(
                "#!/usr/bin/env python3\n"
                "import os, sys\n"
                "if sys.argv[1] != 'api': sys.exit(2)\n"
                "if sys.argv[2].endswith('/commits/main'):\n"
                "    print(os.environ['CAW_TEST_MAIN_SHA'])\n"
                "elif '/actions/workflows/ci.yml/runs?' in sys.argv[2]:\n"
                "    print(os.environ['CAW_TEST_RUNS'])\n"
                "else: sys.exit(3)\n"
            )
            fake.chmod(0o755)
            env = os.environ.copy()
            env.update({
                "PATH": directory + os.pathsep + env["PATH"],
                "GITHUB_REPOSITORY": "0b-ivan/CAW",
                "CAW_CI_WAIT_ATTEMPTS": "1",
                "CAW_CI_WAIT_SECONDS": "0",
                "CAW_TEST_MAIN_SHA": current_sha,
                "CAW_TEST_RUNS": json.dumps({"workflow_runs": runs}),
            })
            return subprocess.run(
                ["bash", str(CI_GATE), requested_sha],
                cwd=ROOT, env=env, capture_output=True, text=True, check=False,
            )

    @staticmethod
    def workflow_run(**changes):
        run = {
            "name": "C++ build and Compose smoke",
            "head_sha": TEST_SHA,
            "head_branch": "main",
            "event": "push",
            "status": "completed",
            "conclusion": "success",
        }
        run.update(changes)
        return run

    def test_ci_gate_requires_the_exact_successful_main_push(self):
        result = self.gate([self.workflow_run()])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("PASS: full main push CI", result.stdout)

        # A passing PR check does not authorize production.
        for changed in [
            {"event": "pull_request"},
            {"head_branch": "feature"},
            {"head_sha": "b" * 40},
            {"status": "in_progress", "conclusion": None},
        ]:
            result = self.gate([self.workflow_run(**changed)])
            self.assertNotEqual(result.returncode, 0, changed)
        result = self.gate([self.workflow_run(conclusion="failure")])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("failed", result.stderr)

    def test_ci_gate_rejects_superseded_or_invalid_commit(self):
        stale = self.gate([self.workflow_run()], current_sha="b" * 40)
        self.assertNotEqual(stale.returncode, 0)
        self.assertIn("not the current main HEAD", stale.stderr)
        malformed = self.gate([self.workflow_run()], requested_sha="not-a-sha")
        self.assertNotEqual(malformed.returncode, 0)
        self.assertIn("Invalid deployment commit SHA", malformed.stderr)


if __name__ == "__main__":
    unittest.main()
