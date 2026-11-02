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
    def check(self, phases, expected=None, broken=False, override=False, override_limit=1):
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
            if override_limit == "volatile":
                args.append("--two-volatile")
            else:
                args.append("--one-abandoned" if override_limit == 1 else "--two-abandoned")
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
        self.assertIn("snapshots preserved", output)

        result, _, error = self.check(
            {"session:ABCDEF": 1, "session:BCDEFG": 2}, override=True,
        )
        self.assertEqual(result, 75)
        self.assertIn("DEFER:", error)

        deploy = (ROOT / "bin" / "deploy-hetzner").read_text()
        self.assertIn('if [[ "$previous_sha" == cc02372* ]]', deploy)
        self.assertIn("--two-volatile", deploy)
        self.assertIn("if (( sockets != 0 )); then", deploy)
        self.assertIn("verify-local-redis-restore", deploy)
        self.assertIn("State.Health.Status", deploy)

    def test_two_abandoned_matches_require_both_override_and_healthy_backup_gate(self):
        phases = {
            "session:ABCDEF": 1,
            "session:BCDEFG": 2,
        }
        deferred, _, _ = self.check(phases)
        self.assertEqual(deferred, 75)
        one_only, _, _ = self.check(phases, override=True)
        self.assertEqual(one_only, 75)
        allowed, output, _ = self.check(phases, override=True, override_limit=2)
        self.assertEqual(allowed, 0)
        self.assertIn("2 operator-confirmed abandoned", output)

        too_many, _, message = self.check({
            "session:ABCDEF": 1,
            "session:BCDEFG": 2,
            "session:CDEFGH": 3,
        }, override=True, override_limit=2)
        self.assertEqual(too_many, 75)
        self.assertIn("DEFER:", message)

        # An expired lobby may remain in the old process's room gauge.
        stale_lobby, output, _ = self.check(
            phases, expected=3, override=True, override_limit=2,
        )
        self.assertEqual(stale_lobby, 0)
        self.assertIn("1 expired inactive rooms", output)

        # Missing one of the two playing rooms is never safe to migrate.
        missing_active, _, error = self.check(
            {"session:ABCDEF": 1}, expected=3,
            override=True, override_limit=2,
        )
        self.assertEqual(missing_active, 1)
        self.assertIn("not all active games are persisted", error)

        extra_persisted, _, _ = self.check(
            phases, expected=1, override=True, override_limit=2,
        )
        self.assertEqual(extra_persisted, 1)
        invalid, _, _ = self.check(
            {"session:ABCDEF": 1, "session:BCDEFG": 99}, expected=3,
            override=True, override_limit=2,
        )
        self.assertEqual(invalid, 1)
        old_override, _, _ = self.check(
            {"session:ABCDEF": 1}, expected=2, override=True,
        )
        self.assertEqual(old_override, 1)

        deploy = (ROOT / "bin" / "deploy-hetzner").read_text()
        self.assertIn('if [[ "$previous_sha" == fc59685e124a00c4666c53947fd2e0f174848b30 ]]', deploy)
        self.assertIn("in_progress == 2 && sockets == 0", deploy)
        self.assertIn('git show "$sha:bin/verify-local-redis-restore"', deploy)
        self.assertIn('CAW_REPO_ROOT="$PWD" bash', deploy)
        self.assertLess(deploy.index("verify-local-redis-restore"),
                        deploy.index("--two-volatile"))

    def test_explicit_volatile_override_preserves_every_redis_snapshot(self):
        # Production has exactly two abandoned games in memory, but the old
        # persistence worker did not write all of them to Redis.
        for phases in (
            {},
            {"session:ABCDEF": 1},
            {"session:ABCDEF": 1, "session:BCDEFG": 3},
            {"session:ABCDEF": 0, "session:BCDEFG": 4},
        ):
            with self.subTest(phases=phases):
                result, output, errors = self.check(
                    phases, expected=3, override=True, override_limit="volatile",
                )
                self.assertEqual(result, 0, errors)
                persisted = sum(phase in (1, 2, 3) for phase in phases.values())
                self.assertIn(f"{persisted} persisted", output)
                self.assertIn(f"{2 - persisted} volatile", output)
                self.assertIn("Redis room snapshots preserved", output)

        # Without the explicit new override, a missing active snapshot fails.
        rejected, _, error = self.check(
            {"session:ABCDEF": 1}, expected=3,
            override=True, override_limit=2,
        )
        self.assertEqual(rejected, 1)
        self.assertIn("not all active games are persisted", error)

    def test_volatile_override_still_blocks_unsafe_redis_states(self):
        cases = [
            ({"session:ABCDEF": 1, "session:BCDEFG": 2,
              "session:CDEFGH": 3}, 3, False),
            ({"session:ABCDEF": 0, "session:BCDEFG": 4,
              "session:CDEFGH": 0}, 2, False),
            ({"session:ABCDEF": 99}, 2, False),
            ({"session:INVALID": 0}, 2, False),
            ({"session:ABCDEF": 1}, 2, True),
        ]
        for phases, expected, broken in cases:
            with self.subTest(phases=phases, broken=broken):
                result, _, _ = self.check(
                    phases, expected=expected, broken=broken,
                    override=True, override_limit="volatile",
                )
                self.assertNotEqual(result, 0)

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
