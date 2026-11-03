"""Guard the non-deploying GHCR image publication rehearsal."""
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = ROOT / ".github" / "workflows" / "release-candidate.yml"


class ReleaseCandidateTests(unittest.TestCase):
    def test_exact_green_main_ci_required_before_publish(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("branches: [main]", workflow)
        self.assertIn("workflow_dispatch:", workflow)
        self.assertIn("github.ref == 'refs/heads/main'", workflow)
        self.assertIn("bash bin/verify-main-ci", workflow)
        self.assertLess(
            workflow.index("bash bin/verify-main-ci"),
            workflow.index("docker build --target runtime"),
        )

    def test_immutable_image_tag_without_prod_deploy(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("ghcr.io/0b-ivan/caw-server:sha-$DEPLOY_SHA", workflow)
        self.assertIn("docker manifest inspect", workflow)
        self.assertIn("docker push \"$image\"", workflow)
        self.assertIn("packages: write", workflow)
        self.assertNotIn("CAW_PRODUCTION_APPROVED", workflow)
        self.assertNotIn("CAW_AUTO_DEPLOY", workflow)
        self.assertNotIn("CAW_SSH_", workflow)
        self.assertNotIn("environment: production", workflow)
        self.assertNotIn("docker compose", workflow)
        self.assertNotIn("deploy-hetzner", workflow)
        self.assertNotIn("rollback-hetzner", workflow)


if __name__ == "__main__":
    unittest.main()
