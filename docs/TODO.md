# ToDo

Quelle der Spielregeln: `0b-ivan/inx-4x-pro`, `src/apps_local/cardparty/`
(Engine: `CardPartyCore.h`, `CardPartyTable.h`, `CardPartySessions.h`,
`CardPartySentence.h`, `CardPartyDeckSet.h`, `CardPartyHistory.h`;
Handy-UI: `web/CardPartyPage.html`; Tests: `host-tests/cardparty/`).

## Vor M2 zu entscheiden

- [x] Postgres mit libpqxx gemäß vorrangigem KONZEPT-CPP-BACKEND.md
- [x] Domain: `caw.obivan.org`
- [ ] Zielserver
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
- [x] Prior99 als siebtes, explizit auszuwählendes Deck importiert
  (107 schwarze, 610 weiße Karten; 11 Quellkategorien, 717 Karten)
- [x] Prior99-Quell-Commit, GPL-/CC-Lizenzhinweise und Bearbeitungsschritte
  direkt am Pack dokumentiert, synthetischen Converter-Test ergänzt
- [x] `defaultSelected: false` im Manifest/Server unterstützt und getestet,
  damit das Erwachsenen-Deck nicht in neue Sessions gelangt
- [x] Versioniertes Prior99-Deck offline auf Inhalt/Format und im
  Docker-Smoke auf Ladefähigkeit prüfen
- [ ] Prior99-Original-, Übersetzungs- und Fanbeitragsrechte sowie eine
  redaktionelle Sichtung vor öffentlicher Bereitstellung final klären

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

- [x] Caddy: Security-Header (CSP, Permissions-Policy, HSTS, kein `Server`), `/metrics` nur aus privaten Netzen, `no-cache` mit ETag für die Webdateien; TLS automatisch, sobald `CADDY_ADDRESS` eine Domain ist
- [ ] Öffentlichen Cloudflare-Edge-TLS- und WSS-Test für `caw.obivan.org`
  nach Hetzner-Provisionierung bestehen (kein Origin-TLS durch Caddy)
- [x] Hetzner-Konzept auf bestehenden Server/Tunnel angepasst, Compose-
  Override mit Loopback-only-Caddy-Port und extern gesperrtem `/metrics`
- [x] Preflight- und synthetische Tunnel-Konfigurationstests in CI ergänzt
- [ ] Existierenden Hetzner-Host prüfen: Docker, Compose, freier Port
  `127.0.0.1:18081`, bestehender `cloudflared`-Dienst und CAW-Start
- [ ] Im bestehenden Cloudflare-Tunnel nur den Hostnamen
  `caw.obivan.org` → `http://127.0.0.1:18081` ergänzen und prüfen
- [ ] Verschlüsselte Offsite-Backups für Postgres und Redis inklusive
  Restore-Test konfigurieren; lokale 14-Tage-Dumps allein genügen nicht
- [x] Täglicher `pg_dump` (Dienst `backup`, 14 Tage), `bin/restore`; Sichern und Zurückspielen in eine frische Datenbank läuft in jeder CI
- [x] Monitoring: `deploy/monitoring/` mit Prometheus-Konfiguration und Alarmregeln (Ausfall, Bereitschaft, Latenz, blockierte Schleife, Speicher, Rate-Limit), Profil `monitoring`; `promtool` prüft sie in der CI
- [x] Seiten Lizenzen (mit den geladenen Packs), Datenschutz, Impressum; Fußzeile verlinkt sie
- [ ] Impressum mit echten Betreiberangaben (`web/operator.json`, nicht im Repo)
- [x] Lizenz für den CAW-Anwendungscode: MIT (`LICENSE`); Karten bleiben CC BY-NC-SA 2.0
- [x] Release-Workflow: Tag `v*` → Server-Image nach GHCR, GitHub-Release mit `docs/releases/<tag>.md`
- [ ] Tag `v1.0.0` setzen (nach Impressum, Zielserver und TLS-Prüfung)

## UI-Polish: Spielkarten (PR #12)

- [x] Abweichung zum strikten Flat-Look zuerst in `docs/UI-KONZEPT.md` erlauben und begrenzen
- [x] Startseiten-Logo mit echten, versetzten CSS-Karten statt flachen Buchstaben
- [x] Frage, Hand, Juror-Optionen und Auflösung im kontrastreichen Kartenstil
- [x] Big-Screen-Fragen/Antwortsätze und Auflösung im selben Stil, ohne private Hände
- [x] Browser-Regressionen für Materialität, Auswahl-ARIA und Big-Screen-Stil ergänzen
- [ ] CI-Build, `ctest`, Browser- und Lasttests für PR #12 grün bestätigen
- [x] Zweiter Durchgang nach realen Screenshots: zentrierte Fragekarten,
  Juror-Indikatoren, einspaltige Handy-Lobby und volle Hauptaktionen
- [x] Alle zugeteilten Handkarten ohne Pagination als durchgehendes scrollbareres Raster;
  Scrollposition und Auswahl bleiben beim Antippen bestehen
- [x] Playwright-Tests für die letzte Karte, die interne Scrollfläche und
  den fest sichtbaren Abgabe-Button bei 320×568, 390×844 und 1280×800
- [x] Zusätzliche Browser-Tests für Kartenseiten, persistente Auswahl und getrennte Rollen
- [x] Desktop-Anordnung am MacBook: Frage links, Karten rechts,
  Host-Einladung neben Spielerliste; eigenes responsives Layout ab 900 px
- [x] Playwright prüft nebeneinander liegende Desktop-Elemente bei 1280×800
- [ ] Visueller Desktop-Vergleich auf dem MacBook nach aktueller CSS-Revision
- [ ] Browser-Screenshots bei 320/390 px sowie 16:9/4:3 visuell gegen Mockups abnehmen

- [x] Handkartenzahl standardmäßig 7, Host-Änderung 3–10 nur in der Lobby
- [x] Protokoll, Redis-Snapshots (mit Legacy-10er-Sessions), Sprachtexte und Tests erweitert
- [ ] CI einschließlich `ctest`, Browser und Lasttests mit Hand-Einstellung grün bestätigen
- [x] Goldene Host-Krone als einzelne freigegebene Farb-Ausnahme im UI-Konzept
- [x] Öffentliche Host-Rolle an authentifizierte Spielerplätze binden (ohne Token/Hand)
- [x] Lobby auf Handy/Desktop/TV und Host-ohne-Sitz mit Krone + sichtbarem HOST-Text
- [x] Tests: mitspielender Host, nicht spielender Host, Trennung/Reauthentifizierung
- [ ] PR #12 mit Krone: Build, CTest, Browser-Screenshots und Lasttests grün bestätigen
- [x] Audit-Fix: QR und alle Einladungsaktionen bei 320 px ohne abgeschnittenes Share-Panel
- [x] Erreichbare LAN-/Tunnel-Basisadresse statt localhost im Host-QR und in Copy/Share/Big-Screen-Link
- [x] Desktop-Frage und Antwortkarten vergrößert, Juror-Karten zentriert und lesbarer
- [x] Browser-Assertions für ungültige Loopback-Adresse, korrigierten QR, Buttons und Desktop-Typografie
- [ ] Abschließenden CI-/Screenshot-Abgleich für diese UI-Korrekturen bestätigen


## UI-Polish: kurzes Intro und Öffnen-Anleitung (PR #12)

- [x] Boot-Intro auf ca. 1,7 s gekürzt, indem Shuffle und Zoom entfallen; ruhiges Bewegungstempo, Skip und Reduced Motion bleiben erhalten
- [x] `[ ANLEITUNG ]` direkt auf der Startseite: drei Schritte zum Online-Spielen (Host, Beitritt, Big Screen) ohne Netzwerk-/Servereinrichtung
- [x] DE/EN-Hilfedialog mit zwei Bereichen: SO STARTEST DU und SPIELREGELN (Juror, Pick/Draw, Punkte, Hand, Host-Einstellungen), Escape, Schließen und eigener Scrollfläche
- [x] Playwright-Checks für DE/EN-Spielregeln, fehlende Infrastruktur-Hinweise, Dialog, ruhige Intro-Dauer und 320/390/1280 px ergänzt
- [ ] Finale CI einschließlich Browser-Screenshots für diese Revision bestätigen

## M9 – Eigene Karten im Host-Editor (Feature-Branch)

- [x] Konzept und UI-Layout für sessionspezifische eigene Karten festgelegt
- [x] Host-authentifizierter REST-Endpunkt zum Laden/Speichern eigener Frage- und Antwortkarten
- [x] Gemeinsame Validierung mit CSV-Lader: UTF-8, 300 Bytes, Pick/Draw, höchstens 100 pro Art
- [x] Eigene Packs von öffentlichen/globalen Packs und anderen Spielräumen isoliert
- [x] Private Karten im versionierten Redis-Room-Snapshot; Schema-4-Kompatibilität
- [x] Host-Menü: Karten hinzufügen, bearbeiten, entfernen, speichern, JSON importieren/exportieren (DE/EN)
- [x] Synthetische C++-Unit- und Browser-Regressionstests ergänzt
- [ ] PR-CI einschließlich CTest, Browser-/Lasttest und Docker-Smoke grün bestätigen
- [ ] Nach CI echte Touch-Abnahme auf Android/iPhone und MacBook durchführen

## CI/CD: Hetzner Auto-Deployment (2026-11-01)

- [x] Produktionskonzept: nur grüne Push-CI auf main, manuelle Wiederholung nur nach grünem SHA-Check
- [x] GHCR-Image mit vollständigem, unveränderlichem Commit-Tag
- [x] SSH-Deploy mit geprüftem Hostschlüssel und kurzlebigem GHCR-Token
- [x] Nur CAW mit Compose-Projekt caw aktualisieren; DB/Redis und Tunnel nicht neu starten
- [x] Postgres-Dump vor Änderung, Healthchecks, automatisches Git/Env/Image-Rollback
- [x] Externer WSS-Test und Rollback bei fehlgeschlagenem Handshake
- [x] Syntax- und Sicherheitschecks in der bestehenden GitHub-CI
- [x] GitHub-Actions-Workflow-Syntax mit gepinntem actionlint validieren
- [x] Deployment nach main-Push nur mit exakter grüner main-Push-CI; fehlgeschlagene,
  ausstehende oder durch neueren Commit überholte CI blockieren; Offline-Tests dafür
- [ ] Nach Merge prüfen, dass der neue Deploy-Workflow registriert wird und bei
  deaktivierten Freigabevariablen keine Produktionsjobs startet
- [x] Einrichtung und Secrets/Variablen im Hetzner-Runbook dokumentiert
- [ ] PR-CI grün prüfen und Dry-Run/Smoke auf einer Testinstanz durchführen
- [ ] Echte Betreiberangaben und Drittanbieter-Kartenrechte klären
- [x] Isolierten PostgreSQL-Restore-Test aus dem bestehenden lokalen CAW-Dump
  ergänzen, ohne Produktionsdatenbank oder Container anzutasten
- [ ] Die neue lokale Restore-Probe in CI und danach auf Hetzner erfolgreich
  nachweisen (als caw-deploy, ohne Storage Box)
- [ ] Storage-Box-Offsite-Backups sind auf Betreiberwunsch vorerst zurückgestellt
- [ ] Verschlüsselte Offsite-Backups für Postgres und Redis-AOF mit Restore-Probe einrichten
- [ ] GitHub-Environment production (SSH-Secrets, geprüfte known_hosts) einrichten
- [ ] CAW_PRODUCTION_APPROVED und CAW_AUTO_DEPLOY nach Abnahme bewusst aktivieren
- [ ] Erstes manuelles Deployment und danach Autodeploy mit externem HTTPS/WSS bestätigen
