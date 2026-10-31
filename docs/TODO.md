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

- [x] Statisches HTML/CSS/JS auf Basis von `CardPartyPage.html`, Gestaltung nach `docs/UI-KONZEPT.md` (Tokens, Aufbau, Ansichten 4.1–4.10)
- [x] Start: NEUE SESSION, CODE EINGEBEN (Code wird beim Server geprüft)
- [x] Spielseite `/s/{code}` (Caddy schreibt auf `play.html` um)
- [x] Spieler: Beitreten, Hand, Pick-Reihenfolge, Abwerfen, Juror-Wahl, Auflösung, Pause, Endstand, „nochmal“
- [x] Host-Bereich: Start, Regeln, Spieler (entfernen), Pause/+Zeit, Aufdecken, Abbrechen, Ende, Rematch, Mitspielen
- [x] Teilen: URL kopieren, QR (eigener Encoder, gegen die Firmware-Bibliothek geprüft), System-Share-Dialog
- [x] Reconnect mit Ticket aus `localStorage`; klare Zustände bei abgelaufener Session, entfernt, anderer Tab
- [x] Playwright: 320/390/1280 px, Deutsch/Englisch, kein Scrollen der Seite in jeder Ansicht, Hauptaktion sichtbar
- Verschoben: Kategorien und Packs nach M5, Lobby sperren nach M7, Siegersätze der Partie im Endstand nach M6

## M4 – Big Screen

- [x] `/s/{code}/screen`: eigener Socket `/ws/{code}/screen`, `screen`-Frames ohne `me` und Hand, keine Befehle
- [x] Lobby: großer QR + URL + Code + Spieler; Antworten: Frage + Fortschritt + Uhr
- [x] Juror-Phase: alle Antworten als eingesetzte Sätze, nummeriert, anonym, ab 5 zweispaltig
- [x] Auflösung: Siegersatz hervorgehoben, Gewinner, Punktestand; Ende: Sieger-Typografie und Balken
- [x] Vollbild-Taste (blendet sich aus), Text wird eingepasst, 16:9 und 4:3 ohne Scrollen
- [x] Big-Screen-Link als QR im Menü und in der Host-Lobby (`[ BIG SCREEN ]`), für alle Geräte
- [x] Playwright: Big Screen bei 1920×1080 und 1024×768 in jeder Phase, nie ein fremder Frame
- Verschoben: Siegersätze der Partie im Ende nach M6, Lizenzzeile der Packs nach M5

## M5 – Kartenpacks

- [x] Sechs lizenzierte Cards-vs-Humanity-Original-CSV-Sets samt Attribution und SHA-256-Prüfsummen versionieren
- [x] CSV-Header `typ,text`, Typ `b`/`w`, Kartenanzahlen und UTF-8 prüfen (585 schwarze, 1179 weiße Karten)
- [x] CSV-Leser (`packs/CsvPack`) mit mehrzeiligen Feldern, Platzhalter `___` → Pick, Pick 3 → Draw 2; alle 1.764 Karten laden
- [x] `packs.json` je Quelle: Titel, Sprache, Lizenz, Rechteinhaber, Quelle sind Pflicht; Image lädt `assets/cardpacks` beim Start (`PACKS_DIR`)
- [x] Prüfungen wie in der Firmware: Format, Längen (≤ 300 Byte), UTF-8, keine Steuerzeichen, höchstens 3 Lücken, Lizenzfelder Pflicht; kaputte Packs werden übersprungen und geloggt
- [x] `GET /api/packs` (Liste für den Host); Host-Befehl `packs` in der Lobby, 1–8 Packs, gleiche Karten nur einmal
- [x] Lizenz und Rechteinhaber der aktiven Packs: Lobby und Endstand am Handy, Lobby und Ende am Big Screen
- [x] Snapshot (Schema 2) speichert die Packs; nach einem Neustart wird derselbe Stapel gebaut
- [ ] Vor öffentlicher Bereitstellung Nutzungszweck, Namensnennung und mögliche Drittbeitragsrechte final prüfen
- Verschoben: Packs als `jsonb` in Postgres nach M6 (kommt mit der Datenbankanbindung; bis dahin sind die Dateien im Image die Quelle),
  `XCAWPK2`- und JSONL-Import aus der Firmware offen (die Dateien liegen nicht im Repo), Kategorien entfallen für die CSV-Sets (Packs sind die Auswahl)
- [x] Fremde Originalkarten ausschließlich in freigegebenem Lizenz-/Quellverzeichnis; Test-Fixtures nur synthetisch

## M6 – Persistenz und Wall of Fame

- [x] Spieler-ID pro Gerät (16 Hex, `localStorage`), Name·Wort mit den 64 Wörtern der Firmware; doppelte IDs und Wörter am Tisch werden umgangen (`assignId` der Firmware)
- [x] Runden- und Spielende nach Postgres über den Persistenz-Thread: Siege, Runden, Spiele, Siegersätze; Rando zählt nicht
- [x] Ruhmeshalle im Menü, 8 Spieler pro Seite, dazu die letzten Siegersätze; `GET /api/fame` aus einem Cache, nie aus der Datenbank im Spiel-Thread, keine Spieler-IDs
- [x] Siegersätze der Partie im Endstand am Handy und im Wechsel am Big Screen
- [x] Aufräumen bei jedem Speichern: Spieler nach 180 Tagen ohne Spiel, nur die neuesten 500 Siegersätze; keine IPs, keine Tickets in Postgres

## M7 – Performance und Härtung

- [x] Lasttest `tests/load/load.mjs` in der CI: 1 000 Sessions × (8 Spieler + Host + Big Screen), echte Runden, Budgets aus KONZEPT 5a als Abbruchkriterium
- [x] Heißer Pfad gemessen und entschlackt: öffentlicher Teil einmal pro Änderung serialisiert, Pack-Credits nur in Lobby und Ende, Snapshots mit 20 ms Zeitbudget pro Tick statt nach jeder Aktion
- [x] `/metrics` echt: Sockets, Aktionen, Latenz-Histogramm, längster Tick, Frame-Größen, Speicher, Rate-Limit-Treffer
- [x] Rate-Limits im Server pro Client-Adresse (Caddy kann es ohne Plugin nicht): Code-Abfrage und Sockets 60/min, Anlegen 6/min; `RATE_LIMIT_PER_MINUTE`
- [x] Lobby sperren (Host), damit Fremde mit dem Code nicht mehr beitreten
- [x] Graceful Shutdown: Sockets schließen, alle offenen Snapshots sichern, Clients reconnecten (seit M2, jetzt mit Snapshot-Budget)

## M8 – Betrieb und Release

- [ ] Caddy mit TLS für eure Domain, Security-Header, HSTS
- [ ] Tägliches `pg_dump`, Restore einmal geübt
- [ ] Monitoring: `/metrics` abgreifen, Alarm bei Fehlerquote/Latenz
- [ ] Impressum/Datenschutz, Lizenzseite für Packs
- [ ] Tag `v1.0.0`, Release-Notes, Images in GHCR
