#!/usr/bin/env python3
"""Check legacy CAW game phases without exposing private Redis room snapshots."""
import json
import re
import subprocess
import sys


KEY_PATTERN = re.compile(r"session:[A-HJKMNP-Z2-9]{6}")


def redis_cli(container: str, *args: str) -> str:
    result = subprocess.run(
        ["docker", "exec", container, "redis-cli", "--raw", *args],
        check=True,
        capture_output=True,
        text=True,
        timeout=12,
    )
    return result.stdout


def main() -> int:
    if len(sys.argv) not in (3, 4) or not sys.argv[2].isdigit():
        print("FAIL: invalid legacy idle-check arguments", file=sys.stderr)
        return 1

    # Only deploy-hetzner may supply these scoped, operator-approved limits.
    abandoned_limit = 0
    if len(sys.argv) == 4:
        abandoned_limit = {"--one-abandoned": 1, "--two-abandoned": 2}.get(
            sys.argv[3], -1)
        if abandoned_limit < 0:
            print("FAIL: invalid migration override", file=sys.stderr)
            return 1
    container = sys.argv[1]
    expected_rooms = int(sys.argv[2])
    if not re.fullmatch(r"[0-9a-f]{12,64}", container):
        print("FAIL: invalid Redis container ID", file=sys.stderr)
        return 1

    try:
        keys = set(redis_cli(container, "--scan", "--pattern", "session:*").splitlines())
        # Only the two-match operator exception may tolerate expired rooms
        # still counted by the old process. It must find both active games.
        count_drift = expected_rooms - len(keys)
        if (count_drift < 0 or
                (count_drift != 0 and abandoned_limit != 2) or
                any(not KEY_PATTERN.fullmatch(key) for key in keys)):
            print(
                f"FAIL: Redis room count mismatch "
                f"(server={expected_rooms}, persisted={len(keys)})",
                file=sys.stderr,
            )
            return 1

        active = 0
        for key in sorted(keys):
            snapshot = json.loads(redis_cli(container, "GET", key))
            phase = snapshot["state"]["game"]["phase"]
            if type(phase) is not int or phase not in (0, 1, 2, 3, 4):
                raise ValueError("Invalid game phase")
            if phase in (1, 2, 3):
                active += 1
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired, ValueError,
            KeyError, TypeError) as error:
        # Snapshot values contain tickets/host tokens; never print them.
        print("FAIL: could not verify all legacy Redis room phases", file=sys.stderr)
        return 1

    if abandoned_limit == 2:
        # The server metric already confirmed exactly two active games.
        # Both must survive in Redis even if old, inactive rooms have expired.
        if active == 2:
            print(
                f"PASS: 2 operator-confirmed abandoned legacy matches; "
                f"snapshots preserved ({count_drift} expired inactive rooms)"
            )
            return 0
        if active < 2:
            print("FAIL: not all active games are persisted in Redis", file=sys.stderr)
            return 1
    if active == 1 and abandoned_limit == 1:
        print("PASS: 1 operator-confirmed abandoned legacy match; snapshots preserved")
        return 0
    if active:
        print(f"DEFER: Redis contains {active} unfinished matches; no rollout",
              file=sys.stderr)
        return 75

    print(f"PASS: {expected_rooms} legacy rooms are lobbies or finished games")
    return 0


if __name__ == "__main__":
    sys.exit(main())
