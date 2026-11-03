"""Legacy-server migrations fail closed unless every room is inactive."""
import contextlib
import importlib.util
import io
import json
import sys
import unittest
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "bin" / "verify-legacy-rooms-idle.py"
SPEC = importlib.util.spec_from_file_location("caw_legacy_room_check", SCRIPT)
legacy = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(legacy)
REDIS_CONTAINER = "a" * 64


class LegacyRoomIdleTests(unittest.TestCase):
    def check(self, phases, expected=None, broken=False, arguments=None):
        keys = list(phases)
        if expected is None:
            expected = len(keys)

        def fake_redis(_container, *args):
            if args[0] == "--scan":
                return "\n".join(keys) + ("\n" if keys else "")
            if broken:
                return "private-secret-not-valid-json"
            return json.dumps({"state": {"game": {"phase": phases[args[1]]}}})

        output = io.StringIO()
        errors = io.StringIO()
        argv = ["legacy", REDIS_CONTAINER, str(expected)]
        if arguments:
            argv.extend(arguments)
        with mock.patch.object(sys, "argv", argv):
            with mock.patch.object(legacy, "redis_cli", side_effect=fake_redis):
                with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
                    result = legacy.main()
        return result, output.getvalue(), errors.getvalue()

    def test_empty_and_finished_rooms_are_safe_to_migrate(self):
        result, output, error = self.check({})
        self.assertEqual(result, 0, error)
        self.assertIn("0 legacy rooms", output)

        result, output, error = self.check(
            {"session:ABCDEF": 0, "session:BCDEFG": 4}
        )
        self.assertEqual(result, 0, error)
        self.assertIn("PASS: 2 legacy rooms", output)

    def test_active_game_blocks_even_without_a_socket(self):
        for phase in (1, 2, 3):
            with self.subTest(phase=phase):
                result, _, error = self.check({"session:ABCDEF": phase})
                self.assertEqual(result, 75)
                self.assertIn("DEFER:", error)

    def test_missing_extra_or_invalid_room_snapshot_fails_closed(self):
        cases = [
            ({"session:ABCDEF": 0}, 2, False),
            ({"session:ABCDEF": 0, "session:BCDEFG": 4}, 1, False),
            ({"session:ABCDEF": 99}, 1, False),
            ({"session:ABCDEF": True}, 1, False),
            ({"session:INVALID": 0}, 1, False),
            ({"session:ABCDEF": 0}, 1, True),
        ]
        for phases, expected, broken in cases:
            with self.subTest(phases=phases, expected=expected, broken=broken):
                result, _, error = self.check(phases, expected, broken)
                self.assertEqual(result, 1)
                self.assertNotIn("private-secret", error)

    def test_retired_operator_overrides_are_rejected(self):
        for flag in ("--one-abandoned", "--two-abandoned", "--two-volatile"):
            with self.subTest(flag=flag):
                result, _, error = self.check(
                    {"session:ABCDEF": 1}, arguments=[flag]
                )
                self.assertEqual(result, 1)
                self.assertIn("invalid legacy idle-check arguments", error)

    def test_deploy_never_reenables_legacy_force_migration(self):
        deploy = (ROOT / "bin" / "deploy-hetzner").read_text()
        self.assertIn("if (( sockets != 0 )); then", deploy)
        self.assertIn("if (( in_progress == 0 )); then", deploy)
        self.assertIn("verify-offline-games.py", deploy)
        self.assertIn("verify-local-redis-restore", deploy)
        self.assertIn('python3 - "$redis_container" "$sessions"', deploy)
        self.assertIn("if (( sockets != 0 )); then", deploy)
        self.assertNotIn("fc59685e124a00c4666c53947fd2e0f174848b30", deploy)
        self.assertNotIn("cc02372*", deploy)
        self.assertNotIn("--two-volatile", deploy)
        self.assertNotIn("--two-abandoned", deploy)
        self.assertNotIn("--one-abandoned", deploy)


if __name__ == "__main__":
    unittest.main()
