# CAW Hetzner – Produktionsabnahme 02.11.2026

## Nachweis

- **Release:** `dcb97c4c9fea70a4ebf4d8bad6e633228dbf894e`
- **CI auf main:** [Run 41524954436](https://github.com/0b-ivan/CAW/actions/runs/41524954436), erfolgreich: C++/CTest, Format, Compose, Browser, Lasttest, Redis-/Postgres-Restore.
- **Produktiver Rollout:** [Run 41524954463](https://github.com/0b-ivan/CAW/actions/runs/41524954463), Deploy-Job erfolgreich. Git-Checkout und Image-Tag entsprechen dem Release-SHA.
- **Öffentlicher Zugriff:** Derselbe Job bestätigt `PASS: Cloudflare HTTPS and WSS host/state handshake`. `/metrics` bleibt am öffentlichen Origin gesperrt.
- **Redis:** Backup-Worker gesund. Die isolierte RDB-Wiederherstellung lief vor dem Serverwechsel; alle vorhandenen 17 Redis-Spielraum-Snapshots wurden ohne Löschung erhalten.
- **Postgres:** Vor der Umschaltung erzeugter Dump `caw-2026-11-02T211501Z.dump`; DB-Volumes wurden nicht neu angelegt.
- **Alte Partien:** Der alte Prozess meldete zwei verlassene Partien bei null WebSockets; eine Partie lag in Redis, eine nur im Arbeitsspeicher. Ausschließlich der flüchtige, nicht gesicherte Spielstand wurde mit der ausdrücklichen Betreiberfreigabe verworfen.
- **Automatische Wiederholungen:** [Run 41529164100](https://github.com/0b-ivan/CAW/actions/runs/41529164100) und [Run 41533685057](https://github.com/0b-ivan/CAW/actions/runs/41533685057) bestätigen `SKIP: CAW already runs sha-dcb97c4c9fea70a4ebf4d8bad6e633228dbf894e` und erfolgreiche öffentliche HTTPS-/WSS-Handshakes. Keine erneute App-Neuerstellung.

## Abschluss der Sondermigration

Nach dem ersten erfolgreichen Upgrade dürfen die zuvor ausschließlich
für die bekannten alten Checkouts `cc02372*` und `fc59685*`
implementierten Force-Deploy-Ausnahmen nicht weiter im aktuellen
Deploy-Code verbleiben. Diese Bereinigung erfolgt im nachfolgenden PR.
Neue und zurückgesetzte Deployments sind nur bei nachgewiesenem
Spiel-Leerlauf freigegeben; fehlende Legacy-Room-Snapshots blockieren.

## Offen

- Verschlüsselte **Offsite-Backups** von Redis und PostgreSQL inklusive Restore-Test (vom Betreiber vorerst zurückgestellt). Lokale RDB- und PG-Dumps allein schützen nicht vor Hostverlust.
- Reale Touch-Abnahme auf Android/iPhone und finale Prüfung der Betreiber- sowie Drittanbieter-Kartenrechte.
- Eine unterbrechungsfreie Bereitstellung bei aktiven Spielen ist im Single-Instance-Design nicht möglich. Der aktuelle Deploy wartet auf Leerlauf; das kurze Race-Fenster zwischen letzter Prüfung und Neustart bleibt bestehen.
