"""Offline games are deployable only when all active states survive in Redis."""
import contextlib
import importlib.util
import io
import json
import sys
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "bin" / "verify-offline-games.py"
SPEC = importlib.util.spec_from_file_location("caw_offline_games", SCRIPT)
offline = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(offline)
CONTAINER = "a" * 64


class OfflineGameTests(unittest.TestCase):
    def check(self, phases, rooms=None, active=1, malformed=False):
        keys = list(phases)
        if rooms is None:
            rooms = len(keys)

        def fake_redis(_container, *args):
            if args[0] == "--scan":
                return "\n".join(keys) + ("\n" if keys else "")
            if malformed:
                return "private-ticket-invalid-json"
            return json.dumps({"state": {"game": {"phase": phases[args[1]]}}})

        output = io.StringIO()
        errors = io.StringIO()
        with mock.patch.object(sys, "argv", [
            "offline", CONTAINER, str(rooms), str(active),
        ]):
            with mock.patch.object(offline, "redis_cli", side_effect=fake_redis):
                with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
                    result = offline.main()
        return result, output.getvalue(), errors.getvalue()

    def test_persisted_disconnected_game_can_be_restarted(self):
        phases = {"session:ABCDEF": 1, "session:BCDEFG": 0}
        result, output, errors = self.check(phases)
        self.assertEqual(result, 0, errors)
        self.assertIn("1 offline unfinished games", output)
        self.assertIn("2 stored rooms preserved", output)

        # Expired inactive rooms can still be counted by the old process.
        result, output, errors = self.check(phases, rooms=3)
        self.assertEqual(result, 0, errors)
        self.assertIn("2 stored rooms preserved", output)

        # Multiple saved games in different unfinished phases are also resumable.
        result, output, errors = self.check(
            {"session:ABCDEF": 1, "session:BCDEFG": 2,
             "session:CDEFGH": 3, "session:DEFGHJ": 4},
            active=3,
        )
        self.assertEqual(result, 0, errors)
        self.assertIn("3 offline unfinished games", output)

    def test_missing_unfinished_game_defers_without_redis_writes(self):
        for phases, rooms, active in (
            ({}, 1, 1),
            ({"session:ABCDEF": 0}, 2, 1),
            ({"session:ABCDEF": 1}, 3, 2),
            ({"session:ABCDEF": 1, "session:BCDEFG": 2}, 3, 1),
        ):
            with self.subTest(phases=phases):
                result, _, errors = self.check(phases, rooms, active)
                self.assertEqual(result, 75, errors)
                self.assertIn("DEFER:", errors)

    def test_corrupt_or_unexpected_redis_state_fails_closed(self):
        cases = [
            ({"session:ABCDEF": 99}, 1, False),
            ({"session:ABCDEF": True}, 1, False),
            ({"session:INVALID": 1}, 1, False),
            ({"session:ABCDEF": 1, "session:BCDEFG": 4}, 1, False),
            ({"session:ABCDEF": 1}, 1, True),
        ]
        for phases, rooms, malformed in cases:
            with self.subTest(phases=phases, malformed=malformed):
                result, _, errors = self.check(phases, rooms, 1, malformed)
                self.assertEqual(result, 1)
                self.assertNotIn("private-ticket", errors)

    def test_invalid_arguments_rejected(self):
        for argv in (
            ["offline", CONTAINER, "1", "0"],
            ["offline", CONTAINER, "1", "2"],
            ["offline", CONTAINER, "-1", "1"],
            ["offline", CONTAINER, "1", "x"],
            ["offline", CONTAINER, "1"],
            ["offline", "not-a-container", "1", "1"],
        ):
            with self.subTest(argv=argv):
                with mock.patch.object(sys, "argv", argv):
                    with contextlib.redirect_stderr(io.StringIO()):
                        self.assertEqual(offline.main(), 1)

    def test_restore_gate_and_socket_checks_remain_before_restart(self):
        script = (ROOT / "bin" / "deploy-hetzner").read_text()
        self.assertIn("if (( sockets != 0 )); then", script)
        self.assertIn("if (( in_progress == 0 )); then", script)
        self.assertIn("caw_actions_total", script)
        self.assertIn('offline_actions="$actions"', script)
        self.assertIn("game activity detected between offline-room checks", script)
        self.assertIn("sleep 60", script)
        self.assertIn("verify-local-redis-restore", script)
        self.assertIn("verify-offline-games.py", script)
        self.assertIn("State.Health.Status", script)
        self.assertLess(script.index("verify-local-redis-restore"),
                        script.index("verify-offline-games.py"))
        self.assertLess(script.index("verify-offline-games.py"),
                        script.index('git merge --ff-only --quiet'))
        self.assertNotIn("FLUSHDB", script)
        self.assertNotIn("FLUSHALL", script)
        self.assertNotIn("DEL session:", script)


if __name__ == "__main__":
    unittest.main()
