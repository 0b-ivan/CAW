"""Validate Redis backup isolation, retention and restore boundaries."""
import re
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BACKUP = ROOT / "deploy" / "redis-backup.sh"
RESTORE = ROOT / "bin" / "verify-local-redis-restore"
COMPOSE = ROOT / "deploy" / "compose.yml"
DEPLOY = ROOT / "bin" / "deploy-hetzner"


class RedisBackupTests(unittest.TestCase):
    def test_scripts_parse(self):
        subprocess.run(["sh", "-n", str(BACKUP)], check=True)
        subprocess.run(["bash", "-n", str(RESTORE)], check=True)

    def test_backup_is_atomic_and_private(self):
        script = BACKUP.read_text()
        self.assertIn("umask 077", script)
        self.assertIn("redis-cli -h redis --rdb", script)
        self.assertIn('if ! timeout 180 redis-cli', script)
        self.assertIn('mv -f "$partial" "$target"', script)
        self.assertIn("-mtime +13 -delete", script)
        self.assertIn("sleep 86400", script)
        self.assertIn("sleep 3600", script)
        self.assertNotIn("redis-cli SAVE", script)
        self.assertNotIn("redis-cli BGSAVE", script)

    def test_sidecar_isolated_from_live_data_and_docker(self):
        compose = COMPOSE.read_text()
        section = compose.split("  redis-backup:\n", 1)[1].split("\n  postgres:", 1)[0]
        self.assertIn("image: redis:7-alpine", section)
        self.assertIn("backups:/backups", section)
        self.assertIn("./redis-backup.sh:/redis-backup.sh:ro", section)
        self.assertIn("service_healthy", section)
        self.assertIn("caw-*.rdb", section)
        for forbidden in ("redisdata:", "pgdata:", "docker.sock", "ports:", "privileged:"):
            self.assertNotIn(forbidden, section)

    def test_restore_is_network_isolated_read_only(self):
        script = RESTORE.read_text()
        self.assertIn("docker cp", script)
        self.assertIn("--network none", script)
        self.assertIn("readonly", script)
        self.assertIn("--port 0", script)
        self.assertIn("--unixsocket /tmp/caw-restore.sock", script)
        self.assertIn("--appendonly no", script)
        self.assertIn("CAW_RESTORE_EXPECT_KEY", script)
        self.assertIn("CAW_RESTORE_EXPECT_VALUE", script)
        self.assertNotIn("redisdata:", script)
        self.assertNotIn("docker compose restart", script)
        self.assertNotIn("docker compose down", script)
        self.assertNotIn("redis-cli FLUSH", script)

    def test_bootstrap_does_not_update_app_or_touch_live_volumes(self):
        script = (ROOT / "bin" / "bootstrap-redis-backup").read_text()
        subprocess.run(["bash", "-n", str(ROOT / "bin" / "bootstrap-redis-backup")],
                       check=True)
        self.assertIn('flock -n 9', script)
        self.assertIn('git show "$sha:deploy/redis-backup.sh"', script)
        self.assertIn(".git/caw-redis-backup-bootstrap.yml", script)
        self.assertIn("--no-deps --no-build --detach --wait --wait-timeout 180 redis-backup", script)
        self.assertIn('SKIP: Redis backup worker already healthy', script)
        self.assertNotIn("git merge --ff-only", script)
        self.assertNotIn("git reset --hard", script)
        self.assertNotIn("server caddy", script)
        self.assertNotIn("docker compose down", script)
        self.assertNotIn("redisdata:/data", script)
        self.assertNotIn("/var/run/docker.sock", script)

    def test_automatic_backup_activation_precedes_game_idle_gate(self):
        workflow = (ROOT / ".github" / "workflows" / "deploy-hetzner.yml").read_text()
        self.assertIn("Start Redis backups independently of game deployment", workflow)
        self.assertIn("bin/bootstrap-redis-backup", workflow)
        self.assertLess(
            workflow.index("Start Redis backups independently"),
            workflow.index("Deploy only when no CAW games"),
        )
        restore = RESTORE.read_text()
        self.assertIn(".git/caw-redis-backup-bootstrap.yml", restore)
        audit = (ROOT / ".github" / "workflows" / "redis-backup-audit.yml").read_text()
        self.assertIn("git show origin/main:bin/verify-local-redis-restore", audit)
        self.assertIn("label=com.docker.compose.service=redis-backup", audit)

    def test_production_installs_only_worker_without_recreating_redis(self):
        script = DEPLOY.read_text()
        self.assertIn("180 redis-backup", script)
        self.assertIn("180 server caddy", script)
        self.assertLess(script.index("180 redis-backup"), script.rindex("180 server caddy"))
        self.assertNotIn("docker compose down", script)
        self.assertNotIn("server caddy redis", script)
        self.assertNotIn("restart redis", script)


if __name__ == "__main__":
    unittest.main()
