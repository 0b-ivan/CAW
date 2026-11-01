"""Ensure legacy Redis snapshots unblock only safely finished or lobby rooms."""
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
    def check(self, phases, expected=None, broken=False, override=False):
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
        args = ["legacy", REDIS_CONTAINER, str(expected)]
        if override:
            args.append("--one-abandoned")
        with mock.patch.object(sys, "argv", args):
            with mock.patch.object(legacy, "redis_cli", side_effect=fake_redis):
                with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
                    result = legacy.main()
        return result, output.getvalue(), errors.getvalue()

    def test_only_lobbies_or_finished_games_allow_deployment(self):
        code, output, errors = self.check({"session:ABCDEF": 0, "session:BCDEFG": 4})
        self.assertEqual(code, 0, errors)
        self.assertIn("PASS: 2 legacy rooms", output)

    def test_active_game_blocks_even_without_a_socket(self):
        for phase in (1, 2, 3):
            with self.subTest(phase=phase):
                result, _, error = self.check({"session:ABCDEF": phase})
                self.assertEqual(result, 75)
                self.assertIn("DEFER:", error)

    def test_missing_or_invalid_room_snapshot_fails_closed(self):
        mismatch, _, _ = self.check({"session:ABCDEF": 0}, expected=2)
        self.assertEqual(mismatch, 1)
        bad_phase, _, _ = self.check({"session:ABCDEF": 99})
        self.assertEqual(bad_phase, 1)
        invalid, _, error = self.check({"session:ABCDEF": 0}, broken=True)
        self.assertEqual(invalid, 1)
        self.assertNotIn("private-secret", error)


    def test_operator_exception_is_limited_to_one_legacy_match(self):
        result, output, _ = self.check({"session:ABCDEF": 1}, override=True)
        self.assertEqual(result, 0)
        self.assertIn("snapshot preserved", output)

        result, _, error = self.check(
            {"session:ABCDEF": 1, "session:BCDEFG": 2}, override=True,
        )
        self.assertEqual(result, 75)
        self.assertIn("DEFER:", error)

        deploy = (ROOT / "bin" / "deploy-hetzner").read_text()
        self.assertIn('if [[ "$previous_sha" == cc02372* ]]', deploy)
        self.assertIn("--one-abandoned", deploy)
        self.assertIn("if (( sockets != 0 )); then", deploy)

    def test_no_sessions_is_idle(self):
        result, output, _ = self.check({})
        self.assertEqual(result, 0)
        self.assertIn("0 legacy rooms", output)

    def test_deploy_uses_snapshot_check_only_on_legacy_server(self):
        deploy = (ROOT / "bin" / "deploy-hetzner").read_text()
        self.assertIn('git show "$sha:bin/verify-legacy-rooms-idle.py"', deploy)
        self.assertIn('python3 - "$redis_container" "$sessions"', deploy)
        self.assertIn("if (( sockets != 0 )); then", deploy)


if __name__ == "__main__":
    unittest.main()
