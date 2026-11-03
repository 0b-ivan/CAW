"""Static and shell-guard tests for the automatic Hetzner CD pipeline."""
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
        self.assertIn('docker pull "ghcr.io/0b-ivan/caw-server:$tag"', content)
        self.assertIn("up --no-deps --no-build", content)
        self.assertIn("server caddy", content)
        self.assertIn("requireIdle", content)
        self.assertIn("caw_active_games", content)
        self.assertIn("caw_sockets", content)
        self.assertIn("caw_sessions", content)
        self.assertIn("return 75", content)
        self.assertLess(content.index("requireIdle\n"), content.index('git merge --ff-only --quiet'))

        self.assertNotIn("docker compose down", content)
        self.assertNotIn("--volumes", content)
        self.assertNotIn("bin/smoke", content)
        rollback = ROLLBACK.read_text()
        self.assertIn("flock -n", rollback)
        self.assertIn("git reset --hard", rollback)
        self.assertIn("up --no-deps --no-build", rollback)
        self.assertNotIn("--volumes", rollback)

    def test_old_operator_exception_cannot_bypass_game_idle_gate(self):
        script = SCRIPT.read_text()
        self.assertIn("if (( sockets != 0 )); then", script)
        self.assertIn("if (( in_progress == 0 )); then", script)
        self.assertIn("verify-offline-games.py", script)
        self.assertIn("verify-local-redis-restore", script)
        self.assertIn("caw_actions_total", script)
        self.assertIn("sleep 60", script)
        self.assertIn("return 75", script)
        self.assertIn('python3 - "$redis_container" "$sessions"', script)
        self.assertNotIn("fc59685e124a00c4666c53947fd2e0f174848b30", script)
        self.assertNotIn("cc02372*", script)
        self.assertNotIn("--one-abandoned", script)
        self.assertNotIn("--two-abandoned", script)
        self.assertNotIn("--two-volatile", script)
        self.assertNotIn("FLUSHALL", script)
        self.assertNotIn("DEL session:", script)

    def test_no_op_and_deferred_status_do_not_trigger_rollback(self):
        script = SCRIPT.read_text()
        workflow = WORKFLOW.read_text()
        # Two skip paths: desired SHA already deployed or superseded.
        self.assertEqual(script.count("exit 79"), 2)
        self.assertIn("SKIP: CAW already runs $tag", script)
        self.assertIn("SKIP: newer main commit is already checked out", script)
        self.assertIn('case "$result" in', workflow)
        for result in ("0)", "75)", "79)"):
            self.assertIn(result, workflow)
        self.assertIn('echo "changed=true" >> "$GITHUB_OUTPUT"', workflow)
        self.assertIn('echo "deferred=true" >> "$GITHUB_OUTPUT"', workflow)
        self.assertIn('echo "unchanged=true" >> "$GITHUB_OUTPUT"', workflow)
        self.assertIn("CAW: NOT DEPLOYED (deferred)", workflow)
        self.assertIn("CAW: UNCHANGED", workflow)
        self.assertIn("CAW: DEPLOYED", workflow)
        self.assertIn(
            "if: failure() && steps.rollout.outputs.changed == 'true' "
            "&& steps.wss.outcome == 'failure'",
            workflow,
        )

    def test_deferred_rollout_uses_read_only_public_health_probe(self):
        workflow = WORKFLOW.read_text()
        self.assertIn(
            "Verify public HTTPS without creating a room when deferred", workflow
        )
        self.assertIn("if: steps.rollout.outputs.deferred == 'true'", workflow)
        self.assertIn("https://caw.obivan.org/healthz | grep -qx ok", workflow)
        self.assertIn("https://caw.obivan.org/metrics", workflow)
        self.assertIn('test "$metrics_status" = 404', workflow)
        self.assertIn(
            "if: steps.rollout.outputs.changed == 'true'", workflow
        )
        self.assertIn(
            "if: steps.rollout.outputs.unchanged == 'true'", workflow
        )
        # Newly installed releases test host/state; unchanged ones never create rooms.
        self.assertIn("node bin/hetzner-wss-check.mjs", workflow)
        self.assertIn("node bin/hetzner-wss-probe.mjs", workflow)
        self.assertIn("Verify existing release via room-free public WSS probe", workflow)

    def test_session_free_probe_creates_no_redis_rooms(self):
        probe = (ROOT / "bin" / "hetzner-wss-probe.mjs").read_text()
        server = (ROOT / "server" / "src" / "net" / "Server.cpp").read_text()
        ci = (ROOT / ".github" / "workflows" / "ci.yml").read_text()

        self.assertIn("new URL('/ws/probe', origin)", probe)
        self.assertIn("'WSS probe timed out'", probe)
        self.assertIn("session-free WSS probe", probe)
        self.assertNotIn("POST", probe)
        self.assertNotIn("/api/sessions", probe)
        self.assertNotIn("hostToken", probe)
        self.assertIn('app.ws<SocketData>("/ws/probe"', server)
        self.assertLess(server.index("registerProbeSocket(app)"),
                        server.index("registerGameSocket(app, false)"))
        self.assertIn('if (!admitted(roomLimit_, response, request)) return;', server)
        self.assertIn("node bin/hetzner-wss-probe.mjs http://localhost:8080", ci)
        self.assertIn('test "$before" = "$after"', ci)

    def test_pipeline_gates_real_production_access(self):
        workflow = WORKFLOW.read_text()
        self.assertIn("push:\n    branches: [main]", workflow)
        self.assertNotIn("workflow_run:", workflow)
        self.assertIn("workflow_dispatch:", workflow)
        self.assertNotIn("vars.CAW_PRODUCTION_APPROVED", workflow)
        self.assertNotIn("vars.CAW_AUTO_DEPLOY", workflow)
        self.assertIn("github.repository == '0b-ivan/CAW'", workflow)
        self.assertIn("github.event_name == 'push'", workflow)
        self.assertIn("github.event_name == 'schedule'", workflow)
        self.assertIn("*/30 * * * *", workflow)
        self.assertIn("git show '$DEPLOY_SHA:bin/deploy-hetzner'", workflow)
        self.assertIn("deferred=true", workflow)
        self.assertIn("steps.rollout.outputs.changed == 'true'", workflow)
        self.assertIn("steps.rollout.outputs.unchanged == 'true'", workflow)
        self.assertIn("node bin/hetzner-wss-probe.mjs", workflow)

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
