#!/usr/bin/env python3
"""Verify that disconnected, unfinished games have valid Redis snapshots."""
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
    if (len(sys.argv) != 4 or
            not sys.argv[2].isdigit() or not sys.argv[3].isdigit()):
        print("FAIL: invalid offline-room check arguments", file=sys.stderr)
        return 1

    container, room_count, active_count = sys.argv[1:]
    if not re.fullmatch(r"[0-9a-f]{12,64}", container):
        print("FAIL: invalid Redis container ID", file=sys.stderr)
        return 1
    expected_rooms = int(room_count)
    expected_active = int(active_count)
    if not 0 < expected_active <= expected_rooms:
        print("FAIL: invalid offline-room counts", file=sys.stderr)
        return 1

    try:
        keys = set(redis_cli(container, "--scan", "--pattern", "session:*").splitlines())
        # Expired inactive rooms may still be counted by the running server.
        if len(keys) > expected_rooms or any(
                not KEY_PATTERN.fullmatch(key) for key in keys):
            print("FAIL: invalid Redis room key count or code", file=sys.stderr)
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
            KeyError, TypeError):
        # Do not expose private hands, card text or authorization tokens.
        print("FAIL: could not verify Redis room snapshots", file=sys.stderr)
        return 1

    if active != expected_active:
        print(
            f"DEFER: {expected_active} unfinished games in memory, "
            f"{active} persisted; no rollout",
            file=sys.stderr,
        )
        return 75

    print(
        f"PASS: {active} offline unfinished games have Redis snapshots; "
        f"{len(keys)} stored rooms preserved"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
