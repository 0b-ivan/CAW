#!/bin/sh
# Consistent daily Redis snapshots; leave AOF and the live Redis volume alone.
set -eu
umask 077

directory=/backups/redis
mkdir -p "$directory"
chmod 700 "$directory"

snapshot() {
  stamp="$(date -u +%Y-%m-%dT%H%M%SZ)"
  target="$directory/caw-$stamp.rdb"
  partial="$target.part.$$"

  # --rdb streams a consistent snapshot through Redis replication protocol.
  # Only publish a file when the transfer has completed.
  if ! timeout 180 redis-cli -h redis --rdb "$partial"; then
    rm -f "$partial"
    echo "FAIL: Redis snapshot transfer failed" >&2
    return 1
  fi
  if [ ! -s "$partial" ]; then
    rm -f "$partial"
    echo "FAIL: Redis snapshot is empty" >&2
    return 1
  fi

  mv -f "$partial" "$target"
  find "$directory" -maxdepth 1 -type f -name 'caw-*.rdb' -mtime +13 -delete
  find "$directory" -maxdepth 1 -type f -name 'caw-*.rdb.part.*' -mtime +1 -delete
  echo "PASS: Redis snapshot saved to $target"
}

if [ "${1:-}" = once ]; then
  snapshot
  exit 0
fi

while true; do
  if snapshot; then
    sleep 86400
  else
    echo "WARN: Redis backup retry scheduled in one hour" >&2
    sleep 3600
  fi
done
