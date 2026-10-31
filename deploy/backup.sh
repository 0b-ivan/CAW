#!/bin/sh
# Daily pg_dump of the statistics into the backups volume, keeping 14 days.
# "once" makes a single dump and exits (used by CI and before updates).
set -eu
dump() {
  file="/backups/caw-$(date -u +%Y-%m-%dT%H%M%SZ).dump"
  pg_dump --host postgres --username caw --format custom --file "$file.part" caw
  mv "$file.part" "$file"
  find /backups -name 'caw-*.dump' -mtime +14 -delete
  echo "backup written: $file"
}
if [ "${1:-}" = once ]; then
  dump
  exit 0
fi
while true; do
  dump || echo "backup failed, next try in an hour"
  sleep 86400
done
