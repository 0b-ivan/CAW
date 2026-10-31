# ToDo

Quelle der Spielregeln: `0b-ivan/inx-4x-pro`, `src/apps_local/cardparty/`
(Engine: `CardPartyCore.h`, `CardPartyTable.h`, `CardPartySessions.h`,
`CardPartySentence.h`, `CardPartyDeckSet.h`, `CardPartyHistory.h`;
Handy-UI: `web/CardPartyPage.html`; Tests: `host-tests/cardparty/`).

## Vor M2 zu entscheiden

- [x] Postgres mit libpqxx gemäß vorrangigem KONZEPT-CPP-BACKEND.md
- [ ] Domain und Zielserver
- [ ] Big Screen nur Anzeige oder auch Juror-Wahl per Tastatur

Prüfbelege für M0/M1: [validation/M0-M1.md](validation/M0-M1.md).

## M0 – Gerüst und Infrastruktur

- [x] Ordner `server/`, `web/`, `deploy/` wie in KONZEPT-CPP-BACKEND.md Abschnitt 4
- [x] CMake (C++20, `-Wall -Wextra -Werror`), Abhängigkeiten über Debian-Pakete bzw. `FetchContent` mit fester Version
- [x] doctest eingebunden, `ctest` läuft; `.clang-format`, `.clang-tidy` (wenige Checks), `bin/format`
- [x] `server/src/main.cpp`: Config aus Umgebung, Logger, uWebSockets mit `/healthz` und `/metrics`
- [x] `deploy/compose.yml`: caddy, server, redis (AOF, maxmemory, noeviction), postgres; Volumes; Healthchecks
- [x] `deploy/compose.dev.yml`, `.env.example`; `.env` in `.gitignore`
- [x] `deploy/server.Dockerfile` zweistufig, Laufzeit als non-root
- [x] `deploy/Caddyfile`: `web/` statisch, `/api` und `/ws` an den Server
- [x] `web/index.html` + `web/style.css` (Startseite, Tokens aus UI-KONZEPT)
- [x] GitHub Actions: Build + `ctest`, Docker-Build, Compose-Smoke (`/healthz` == 200)

## M1 – Spielregeln aus der Firmware übernehmen

- [x] Header aus `src/apps_local/cardparty/` nach `server/src/game/` kopieren (Quell-Commit im Commit-Text)
- [x] Zugehörige Host-Tests aus `host-tests/cardparty/` auf doctest umstellen und grün bekommen, **bevor** etwas geändert wird
- [x] X4-Reste entfernen (SD, HalStorage, E-Ink); `kHost` → Host-Rolle der Web-Session
- [x] Abgedeckt und getestet: Phasen, Juror-Rotation, Pick 1/2/3, Draw 2, Abwerfen, faire Rundenzahl, Uhren, Pause, 15-s-Warten, Kick, Ende, „nochmal“, Kategorien, Deck-Set (8 Packs), Siegersatz
- [x] Zustand als JSON speichern und laden (für Redis), mit Rundlauf-Test

## M2 – Echtzeit-Server und Sessions

- [x] M2-Grundlage: kryptografische 6-Zeichen-Codes, 128-Bit-Host-Tokens,
  event-loop-eigene Registry und striktes POST/GET-JSON mit CTest/Compose-Smoke
- [x] Room-Registry hält pro Code eine echte `Session` (Game, Sitze, Table);
  Aktivität verlängert die Lebensdauer
- [x] `net/Server`: WebSocket `/ws/{code}` (404 bei unbekanntem Code), Senden mit
  Backpressure (uWebSockets verwirft über 64 KB, beim `drain` wird der neueste Stand nachgeschickt)
- [x] `net/Protocol`: JSON ⇄ Befehle/Zustände, Typ- und Längenprüfung, unbekannte Typen und Felder werden abgelehnt
- [x] `session/SessionRegistry` + `Codes`: `POST /api/sessions` → Code (6 Zeichen, 31er-Alphabet) + Host-Token
- [x] `session/Session`: `join`, `resume` mit Ticket-Rotation, Übernahme durch neuen Socket, Host-Befehle, Fehlercodes
- [x] Revision pro Stand; veraltete `submit`/`judge` → Fehler + sofortiger Resync
- [x] Timer alle 250 ms → `Table::tick`
- [x] Host-Befehl `settings`: Runden (nur in der Lobby), Antwort-/Juror-Uhr, Verlängerung,
  Auto-Weiter, Rando – dieselben Auswahlwerte wie der X4; sichtbar in jedem Zustand
- [x] `store/PersistQueue` + `RedisStore`: Snapshot je Änderung (nur neuester je Raum) im
  Hintergrund-Thread, `session:{code}` mit TTL, Laden beim Start; alle Sitze starten getrennt
  und kommen per Ticket zurück. Ohne Redis läuft das Spiel weiter, nur ohne Speichern.
- [x] Graceful Shutdown bei SIGTERM/SIGINT: Sockets schließen, Warteschlange leeren, Exit 0
- [x] Bot-Programm `tests/bots/play.mjs` in der CI: 8 Bots spielen 8 Runden über Caddy,
  Reload-Sturm in Runde 3, `docker compose restart server` mitten in Runde 5, Fortsetzung
  mit den alten Tickets bis zum Endstand; keine fremde Hand in irgendeinem Frame

## M3 – Spieler- und Host-UI

- [ ] Statisches HTML/CSS/JS auf Basis von `CardPartyPage.html`, Gestaltung nach `docs/UI-KONZEPT.md` (Tokens, Aufbau, Ansichten 4.1–4.10)
- [ ] Start: NEUE SESSION, CODE EINGEBEN
- [ ] Spieler: Beitreten, Hand, Pick-Reihenfolge, Abwerfen, Juror-Wahl, Auflösung, Pause, Endstand, „nochmal“
- [ ] Host-Bereich: Start, Regeln, Kategorien, Packs, Spieler (entfernen), Pause/+Zeit, Ende, Rematch, Lobby sperren
- [ ] Teilen: URL kopieren, QR, System-Share-Dialog
- [ ] Reconnect mit Ticket aus `localStorage`; klare Zustände bei abgelaufener Session
- [ ] Playwright: 320/390/1280 px, Deutsch/Englisch, kein horizontales Scrollen

## M4 – Big Screen

- [ ] `/s/{code}/screen`: eigene Frames, nur öffentliche Daten
- [ ] Lobby: großer QR + URL + Code + Spieler; Antworten: Frage + Fortschritt + Uhr
- [ ] Juror-Phase: alle Antworten als eingesetzte Sätze, nummeriert, anonym
- [ ] Auflösung: Siegersatz hervorgehoben, Gewinner, Punktestand; Ende: Zeremonie
- [ ] Vollbild-Taste, Lesbarkeit aus 4 m (große Schrift, hoher Kontrast), 16:9 und 4:3
- [ ] Host kann Big-Screen-Link als QR zeigen

## M5 – Kartenpacks

- [x] Sechs lizenzierte Cards-vs-Humanity-Original-CSV-Sets samt Attribution und SHA-256-Prüfsummen versionieren
- [x] CSV-Header `typ,text`, Typ `b`/`w`, Kartenanzahlen und UTF-8 prüfen (585 schwarze, 1179 weiße Karten)
- [ ] Fremde Platzhalter/Satzkomposition vollständig prüfen und CSV-Import-Adapter für M5 bauen
- [ ] Vor öffentlicher Bereitstellung Nutzungszweck, Namensnennung und mögliche Drittbeitragsrechte final prüfen

- [ ] Import-Kommando (`server --import-pack datei`): `XCAWPK2` mit dem Reader aus der Firmware (`CardPartySdPack.h`) und JSONL aus `cardparty_l10n`
- [ ] Prüfungen wie in der Firmware: Format, CRC, Längen, Platzhalter, Kategorien, Lizenzfelder Pflicht
- [ ] Pack = `jsonb`-Dokument pro Version; Liste der Packs für den Host
- [ ] Lizenz und Rechteinhaber jedes aktiven Packs in der UI anzeigen
- [x] Fremde Originalkarten ausschließlich in freigegebenem Lizenz-/Quellverzeichnis; Test-Fixtures nur synthetisch

## M6 – Persistenz und Wall of Fame

- [ ] Spieler-ID pro Gerät, Name·Wort, Kollisionen vermeiden
- [ ] Runden- und Spielende nach Postgres: Siege, Runden, Siegersätze
- [ ] Fame-Ansicht seitenweise; keine Spieler-IDs an fremde Clients
- [ ] Aufräumjob: Sessions 30 Tage, keine IPs speichern

## M7 – Performance und Härtung

- [ ] Lasttest mit dem Bot-Programm: 1 000 Sessions × 8 Spieler, Budgets aus KONZEPT 5a
- [ ] Profiling des heißen Pfads; Frame-Größen messen
- [ ] Einfache Rate-Limits in Caddy (Join, Code-Abfrage)
- [ ] Graceful Shutdown: Sockets schließen, Redis-Flush, Clients reconnecten

## M8 – Betrieb und Release

- [ ] Caddy mit TLS für eure Domain, Security-Header, HSTS
- [ ] Tägliches `pg_dump`, Restore einmal geübt
- [ ] Monitoring: `/metrics` abgreifen, Alarm bei Fehlerquote/Latenz
- [ ] Impressum/Datenschutz, Lizenzseite für Packs
- [ ] Tag `v1.0.0`, Release-Notes, Images in GHCR
