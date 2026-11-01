"""Safety and shell-parse checks for encrypted CAW Storage Box backups."""
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BACKUP = ROOT / "bin" / "caw-offsite-backup"
RESTORE = ROOT / "bin" / "caw-offsite-restore-check"
SERVICE = ROOT / "deploy" / "systemd" / "caw-offsite-backup.service"
TIMER = ROOT / "deploy" / "systemd" / "caw-offsite-backup.timer"


class StorageBoxSafetyTests(unittest.TestCase):
    def test_bash_syntax(self):
        for script in (BACKUP, RESTORE):
            with self.subTest(script=script.name):
                subprocess.run(["bash", "-n", str(script)], check=True)

    def test_no_secrets_in_repo_or_production_compose_changes(self):
        backup = BACKUP.read_text()
        restore = RESTORE.read_text()
        self.assertIn("$HOME/.config/caw/offsite.env", backup)
        self.assertIn("$RESTIC_PASSWORD_FILE", backup)
        self.assertIn("restic backup", backup)
        self.assertIn("restic forget", backup)
        self.assertIn("--group-by host,tags", backup)
        self.assertIn("redis-cli CONFIG SET", backup)
        self.assertIn("auto-aof-rewrite-percentage 0", backup)
        self.assertIn("rewrite_disabled=0", backup)
        self.assertIn("trap cleanup EXIT", backup)
        self.assertIn("appendonly.aof.manifest", backup)
        for script in (backup, restore):
            self.assertNotIn("docker compose down", script)
            self.assertNotIn("docker compose restart", script)
            self.assertNotIn("--volumes", script)
            self.assertNotIn("StrictHostKeyChecking=no", script)

    def test_restore_is_isolated_and_checks_both_databases(self):
        restore = RESTORE.read_text()
        self.assertIn("restic restore latest", restore)
        self.assertGreaterEqual(restore.count("--network none"), 2)
        self.assertIn("postgres:17-alpine", restore)
        self.assertIn("redis:7-alpine", restore)
        self.assertIn("--exit-on-error", restore)
        self.assertIn("caw_restore", restore)
        self.assertIn("redis-cli -s /tmp/caw-restore.sock ping", restore)
        self.assertNotIn("caw-postgres-1", restore)
        self.assertNotIn("caw-redis-1", restore)

    def test_timer_does_not_run_as_root(self):
        service = SERVICE.read_text()
        timer = TIMER.read_text()
        self.assertIn("User=caw-deploy", service)
        self.assertIn("Group=caw-deploy", service)
        self.assertIn("ExecStart=/bin/bash /opt/caw/bin/caw-offsite-backup", service)
        self.assertIn("OnCalendar=", timer)
        self.assertIn("Persistent=true", timer)


if __name__ == "__main__":
    unittest.main()
