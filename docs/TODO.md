# ToDo

Quelle der Spielregeln: `0b-ivan/inx-4x-pro`, `src/apps_local/cardparty/`
(Engine: `CardPartyCore.h`, `CardPartyTable.h`, `CardPartySessions.h`,
`CardPartySentence.h`, `CardPartyDeckSet.h`, `CardPartyHistory.h`;
Handy-UI: `web/CardPartyPage.html`; Tests: `host-tests/cardparty/`).

## Vor M2 zu entscheiden

- [ ] Postgres oder MySQL (Vorschlag Postgres, siehe KONZEPT 11)
- [ ] Domain und Zielserver
- [ ] Big Screen nur Anzeige oder auch Juror-Wahl per Tastatur

## M0 – Gerüst und Infrastruktur

- [ ] pnpm-Monorepo: `packages/engine`, `packages/protocol`, `apps/server`, `apps/web`
- [ ] TypeScript strict, ESLint, Prettier, Vitest; `pnpm check` = lint + typecheck + test
- [ ] `compose.yml`: caddy, app, redis (AOF, maxmemory, noeviction), postgres; Volumes; Healthchecks
- [ ] `compose.dev.yml` mit Hot Reload; `.env.example`; `.env` in `.gitignore`
- [ ] Multi-Stage-Dockerfile für `app` (Build → schlankes Runtime-Image, non-root)
- [ ] `/healthz` (Redis, Postgres, Event-Loop-Verzögerung), `/metrics`
- [ ] GitHub Actions: `pnpm check`, Docker-Build, Compose-Smoke (`/healthz` == 200)

## M1 – Engine portieren

- [ ] Deck, Kategorien-Maske, Ziehstapel ohne Wiederholung, Rando
- [ ] Phasen Lobby → Playing → Judging → Reveal → Finished; Juror-Rotation
- [ ] Pick 1/2/3, Draw 2, Abwerfen (1 Karte/Runde), faire Rundenzahl
- [ ] Table: Antwort-Uhr + einmalige Verlängerung, +Zeit, Juror-Uhr, Auto-Weiter, Pause-Rechte, 15-s-Warten, „ohne ihn weiter“
- [ ] Kick, Ende für alle, „nochmal“-Stimmen, Rematch
- [ ] Siegersatz zusammensetzen (Platzhalter `___`, `[blank]`, `{blank}`), UTF-8-sicher, Längenlimit
- [ ] Deck-Set: bis 8 Packs mischen, gleichnamige Kategorien zusammenführen, Duplikate einmal, max. 16 Kategorien / 4096 Karten
- [ ] Engine ist serialisierbar (JSON rein/raus, deterministisch mit Seed)
- [ ] Tests aus `host-tests/cardparty` als Vitest-Fälle portieren (Spielablauf, Uhren, Kick, Shuffle-Verteilung, Deck-Set)

## M2 – Echtzeit-Server und Sessions

- [ ] Fastify + `ws`; ein Session-Objekt pro Code, Aktion → Engine → Frames
- [ ] Session anlegen (`POST /api/sessions`): Code (6 Zeichen, 31er-Alphabet), Host-Token
- [ ] `join`, `resume` mit Ticket-Rotation, Übernahme des Sitzes durch neuen Socket
- [ ] Revision pro Stand; veraltete `submit`/`judge` → Fehler + sofortiger Resync
- [ ] Host-Befehle nur mit Host-Token; Fehlercodes (`session_full`, `game_running`, …)
- [ ] Redis Write-through pro Aktion (gebündelt pro Tick), TTL 24 h; Wiederherstellung beim Start
- [ ] Backpressure: hängender Socket bekommt nur den neuesten Stand
- [ ] `Origin`-Prüfung, Nachrichtengröße begrenzt, zod-Validierung, keine unbekannten Felder
- [ ] Bot-Client für Tests: 8 Bots spielen eine ganze Partie; Reload-Sturm; Neustart mitten in der Runde

## M3 – Spieler- und Host-UI

- [ ] Gestaltung nach `docs/UI-KONZEPT.md` (Tokens, Aufbau, Ansichten 4.1–4.10)
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

- [ ] Import-Kommando im Container: `XCAWPK2`-Datei (SD-Format) und JSONL aus `cardparty_l10n`
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

- [ ] Lasttest (k6 oder Bot-Schwarm): 1 000 Sessions × 8 Spieler, Budgets aus KONZEPT 5a
- [ ] Profiling des heißen Pfads; Frame-Größen messen
- [ ] Rate-Limits pro IP (Join, Code-Abfrage, Nachrichten/s)
- [ ] Graceful Shutdown: Sockets schließen, Redis-Flush, Clients reconnecten

## M8 – Betrieb und Release

- [ ] Caddy mit TLS für eure Domain, Security-Header, HSTS
- [ ] Tägliches `pg_dump`, Restore einmal geübt
- [ ] Monitoring: `/metrics` abgreifen, Alarm bei Fehlerquote/Latenz
- [ ] Impressum/Datenschutz, Lizenzseite für Packs
- [ ] Tag `v1.0.0`, Release-Notes, Images in GHCR
