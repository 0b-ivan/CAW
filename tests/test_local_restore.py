"""Guard the read-only CAW local-backup restore probe."""
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "bin" / "verify-local-restore"


class LocalRestoreSafetyTests(unittest.TestCase):
    def test_shell_parses(self):
        subprocess.run(["bash", "-n", str(SCRIPT)], check=True)

    def test_restore_uses_ephemeral_isolated_database(self):
        code = SCRIPT.read_text(encoding="utf-8")
        self.assertIn("docker cp", code)
        self.assertIn("docker run --detach --rm --network none", code)
        self.assertIn("--tmpfs /var/lib/postgresql/data", code)
        self.assertIn("postgres:17-alpine", code)
        self.assertIn("pg_restore --username postgres", code)
        self.assertIn("--exit-on-error --no-owner --no-acl", code)
        self.assertIn("docker rm --force", code)
        self.assertIn("trap cleanup EXIT", code)
        self.assertIn("TABLE_NAME" if False else "table_name IN ('players','winning_lines')", code)

    def test_restore_does_not_modify_production_services(self):
        code = SCRIPT.read_text(encoding="utf-8")
        self.assertNotIn("docker compose down", code)
        self.assertNotIn("docker compose up", code)
        self.assertNotIn("docker compose restart", code)
        self.assertNotIn("dropdb", code)
        self.assertNotIn("git reset", code)
        self.assertNotIn("docker exec caw-postgres-1", code)
        self.assertNotIn("docker exec caw-redis-1", code)


if __name__ == "__main__":
    unittest.main()
