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

- [ ] `net/Server`: Routen, WebSocket-Verbindungen, Senden mit Backpressure (`getBufferedAmount`, nur neuester Stand)
- [ ] `net/Messages`: JSON ⇄ Befehle/Zustände, Typ- und Längenprüfung, unbekanntes wird abgelehnt
- [ ] `session/SessionRegistry` + `Codes`: `POST /api/sessions` → Code (6 Zeichen, 31er-Alphabet) + Host-Token
- [ ] `session/Session`: `join`, `resume` mit Ticket-Rotation, Übernahme durch neuen Socket, Host-Befehle, Fehlercodes
- [ ] Revision pro Stand; veraltete `submit`/`judge` → Fehler + sofortiger Resync
- [ ] Timer alle 250 ms → `Table::tick`
- [ ] `store/PersistQueue` + `RedisStore`: Snapshot je Änderung (nur neuester je Session), TTL 24 h, Laden beim Start
- [ ] Graceful Shutdown bei SIGTERM
- [ ] Bot-Programm: 8 Bots spielen eine ganze Partie; Reload-Sturm; Neustart mitten in der Runde

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

- [ ] Import-Kommando (`server --import-pack datei`): `XCAWPK2` mit dem Reader aus der Firmware (`CardPartySdPack.h`) und JSONL aus `cardparty_l10n`
- [ ] Prüfungen wie in der Firmware: Format, CRC, Längen, Platzhalter, Kategorien, Lizenzfelder Pflicht
- [ ] Pack = `jsonb`-Dokument pro Version; Liste der Packs für den Host
- [ ] Lizenz und Rechteinhaber jedes aktiven Packs in der UI anzeigen
- [ ] Keine Kartentexte im Repo; Test-Fixtures nur synthetisch

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
