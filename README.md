# CAW – Cards Against Whatever (Web)

Partyspiel für 3–8 Handys, mit optionalem Big Screen. M0 bis M6 sind
fertig: Startseite, C++-Server mit Sessions über WebSocket und Redis,
portierte Firmware-Regeln, die Spielseite für Handys und Host, der Big Screen,
die sechs deutschen Cards-vs-Humanity-Sets als wählbare Packs und die
Ruhmeshalle in Postgres.

## Lokal starten

Docker mit Compose v2 wird vorausgesetzt. Im Repository:

```sh
cp .env.example deploy/.env
# POSTGRES_PASSWORD in deploy/.env durch einen zufälligen URL-sicheren Wert ersetzen.
docker compose -f deploy/compose.dev.yml up --build --wait
```

Startseite: http://localhost:8080, Bereitschaft: `/healthz`, Metriken:
`/metrics`. Die statischen Dateien sind als Volume eingebunden; nach einer
Änderung genügt Neuladen. Der Server wird im Entwicklungsprofil als
Debug-Build gebaut; C++-Änderungen benötigen einen erneuten Build.
Lizenz- und Datenschutzseiten folgen mit M8; die Fußzeile enthält noch keine
Links.

Das Produktionsgerüst wird mit
`docker compose -f deploy/compose.yml up --build -d --wait` gestartet.
Für eine echte Domain `CADDY_ADDRESS`, `PUBLIC_URL`, `HTTP_PORT=80` und
`HTTPS_PORT=443` in `deploy/.env` setzen. Caddy übernimmt dann TLS.
Domain, Zertifikate und öffentlicher Betrieb gehören zu M8 und wurden in M0
noch nicht geprüft. Nur Caddy veröffentlicht Ports. Redis und Postgres sind
intern erreichbar. Das Datenbankpasswort muss in einer URL verwendbar sein
(z.B. Hex-Zeichen); `.env` wird nicht eingecheckt.

## Hetzner Cloud / Cloudflare Tunnel (Produktion)

Die Produktion wird über den **bereits bestehenden Cloudflare Tunnel des
Hetzner-Servers** veröffentlicht. Es wird kein weiterer `cloudflared`-Container
und keine neue VM eingerichtet. `deploy/compose.hetzner.yml` bindet Caddy
nur an `127.0.0.1:18081` (konfigurierbar über `CAW_ORIGIN_PORT`); die
bestehende Tunnel-Route erhält zusätzlich `caw.obivan.org`.

**Vollständige Anleitung:** [Hetzner + Cloudflare Tunnel](docs/operations/HETZNER-CLOUDFLARE.md).
Auf einem frisch vorbereiteten Hetzner-Server nach Anlegen des Tunnels
und der privaten Dateien:

```sh
cp deploy/hetzner/env.example deploy/.env
# Passwort (openssl rand -hex 32), Betreiberangaben und Tunnel-Secret einsetzen
python3 bin/hetzner-preflight.py
docker compose -f deploy/compose.yml -f deploy/compose.hetzner.yml up --build -d --wait
python3 bin/hetzner-preflight.py --public
node bin/hetzner-wss-check.mjs
```

**Nicht** `docker compose -f deploy/compose.yml up` als öffentliche
Hetzner-Produktion ausführen: Das Basis-Compose stellt Entwicklungsports
bereit. Unter Tunnel ist `/metrics` nach außen immer 404; intern wird
`server:8080/metrics` weiter von Prometheus abgefragt. Der Tunnel erfordert
keine öffentlichen 80/443 oder A-/AAAA-Records zur Hetzner-Origin-IP.

Bis zu einem öffentlichen Release fehlen die Betreiberangaben, echte
Cloudflare-HTTPS-/WSS-Abnahme und **verschlüsselte externe Backups**
für Postgres und Redis. Die lokalen 14-Tage-Dumps bleiben bestehen.

## Spielen

1. Am Laptop oder Handy http://localhost:8080 öffnen, `[ NEUE SESSION ]`.
   Die Seite wechselt auf `/s/XXXXXX` und zeigt QR-Code, Code und Link.
2. Drei bis acht Handys scannen den QR-Code (oder geben den Code auf der
   Startseite ein) und treten mit Namen bei. Der Host kann mit
   `[ MITSPIELEN ]` selbst einen Platz nehmen.
3. Regeln stellt der Host im Menü `[ ≡ ]` → SPIELREGELN ein, dann `[ START ]`.
4. Optional ein Fernseher oder Laptop als Big Screen: In der Host-Lobby
   `[ BIG SCREEN ]` (oder Menü → BIG SCREEN) zeigt den Link
   `/s/XXXXXX/screen` als QR-Code. Der Big Screen zeigt Frage, Uhr, die
   eingesetzten Sätze und den Endstand, nie eine Hand.

Für Handys im WLAN statt `localhost` die IP des Rechners verwenden. Ein
Reload oder ein kurzer Netzabbruch kostet keinen Platz: Das Sitz-Ticket liegt
im `localStorage` des Browsers. Keine Ansicht scrollt; nur Listen, die nicht
passen, scrollen in sich (`docs/UI-KONZEPT.md` Abschnitt 1).

## Server-Protokoll (M2)

Der C++-Server stellt zwei REST-Routen und den Spiel-WebSocket bereit:

```sh
# Auf einem laufenden Compose-Stack:
curl -sS -H 'Content-Type: application/json' -d '{"language":"de"}' \
  http://localhost:8080/api/sessions
# -> {"code":"XXXXXX","hostToken":"<32 geheime Hex-Zeichen>"}

curl -sS http://localhost:8080/api/sessions/XXXXXX
# -> {"code":"XXXXXX","language":"de","phase":"lobby"}
```

Der private Host-Token wird nur beim Anlegen zurückgegeben, nicht im
öffentlichen GET. Sechsstellige Codes verwenden ein eindeutiges
31-Zeichen-Alphabet; die 128-Bit-Host-Tokens kommen aus Betriebssystemzufall.
Leere oder unerwartete JSON-Felder werden abgelehnt. Ein abgelaufener
Code wird wie ein unbekannter Code behandelt.

Jeder Raum ist ein echter Spieltisch. Gespielt wird über den WebSocket
`ws://…/ws/XXXXXX` (Befehle und Antworten: `docs/KONZEPT-CPP-BACKEND.md`
Abschnitt 7). Ein Spieler tritt mit `{"type":"join","name":"Ada"}` bei, der
Host meldet sich mit `{"type":"host","token":"…"}` an und startet mit
`{"type":"start"}`. Ohne Pack-Dateien (`PACKS_DIR` leer) spielt der Server
mit nummerierten Testkarten.

Laufende Partien werden nach jeder Änderung in Redis gesichert
(`session:XXXXXX`, verfällt nach `SESSION_TTL_HOURS` ohne Aktivität). Nach einem
Neustart holen sich die Handys ihren Sitz mit ihrem Ticket zurück
(`{"type":"resume","token":"…"}`), der Host meldet sich erneut mit dem
Host-Token an. Bei SIGTERM (`docker stop`) schließt der Server alle Sockets und
schreibt ausstehende Stände, bevor er endet.

## Betrieb

- **Domain und TLS:** `caw.obivan.org`. In `deploy/.env`
  `CADDY_ADDRESS=caw.obivan.org`, `PUBLIC_URL=https://caw.obivan.org`,
  `HTTP_PORT=80`, `HTTPS_PORT=443` (Vorlage in `.env.example`). Der DNS-Eintrag
  muss auf den Server zeigen, Ports 80 und 443 offen; Caddy holt das
  Zertifikat selbst.
- **Impressum:** `cp web/operator.example.json web/operator.json` und Name,
  Anschrift, E-Mail eintragen (die Datei ist nicht im Repo). Die Seiten
  Lizenzen und Datenschutz kommen ohne Zutun.
- **Backups:** Der Dienst `backup` sichert täglich nach `backups` (14 Tage).
  Zurückspielen: `bin/restore caw-<zeit>.dump` oder zur Kontrolle erst
  `bin/restore caw-<zeit>.dump caw_check`. Dateien auflisten:
  `docker compose -f deploy/compose.yml exec backup ls /backups`.
- **Monitoring:** `docker compose -f deploy/compose.yml --profile monitoring up -d`
  startet Prometheus mit den Alarmregeln aus `deploy/monitoring/` auf
  `127.0.0.1:9090`. `/metrics` ist von außen gesperrt.
- **Release-Image:** Ein Tag `v*` baut `ghcr.io/0b-ivan/caw-server`. Mit
  `CAW_VERSION=v1.0.0 docker compose -f deploy/compose.yml pull server`
  statt `--build`.

## Bauen und prüfen

Unter Debian 13: `build-essential`, `cmake`, `ninja-build`, `git`,
`ca-certificates`, `libpqxx-dev`, `libhiredis-dev`, `zlib1g-dev`,
`nlohmann-json3-dev`, `doctest-dev`, optional `clang-format`, `clang-tidy`.
Die beiden übrigen Bibliotheken lädt CMake aus festen Quell-Commits.

```sh
cmake -S server -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 2
ctest --test-dir build --output-on-failure
./bin/format
```

Ohne installierte Datenbank-Clients lassen sich Regeln, Snapshots und
Konfiguration mit `-DCAW_BUILD_SERVER=OFF` prüfen. Mit Docker:

```sh
docker build --target build -t caw-build -f deploy/server.Dockerfile .
docker run --rm caw-build ctest --test-dir /app/build --output-on-failure
docker run --rm caw-build ./bin/lint
./bin/smoke
```

`node tests/load/load.mjs http://localhost:8080 1000 3` spielt 1 000 Räume mit
je 8 Bots, Host und Big Screen und prüft die Budgets aus `docs/KONZEPT.md` 5a;
dafür `RATE_LIMIT_PER_MINUTE` hoch setzen, sonst greift das Rate-Limit.
`bin/smoke` erwartet einen laufenden Compose-Stack, prüft Caddy und lässt
Redis und Postgres nacheinander ausfallen und wieder anlaufen. Nur in einer
Testinstanz verwenden. Eigene Ports über `CAW_SMOKE_URL` angeben.
`web/tests/landing.cjs` prüft mit Playwright Deutsch/Englisch, Tastatur,
Tippziele und Überlauf bei 320/390/1280 px. `web/tests/play.cjs` spielt mit
einem Host und drei Handys eine ganze Partie (Reload, Pause, Juror, Endstand,
Rematch, Entfernen, Sprache) und prüft jede Ansicht bei 320/390/1280 px auf
Scrollen und sichtbare Hauptaktion. CI installiert Playwright nur für diese
Tests; die Web-App selbst braucht weder Node noch einen Build.

Die drei Gameplay-Metriken und das Latenz-Histogramm stehen in M0 auf null;
Health-Zähler zählen echte Probes. Es gibt noch keine Spielaktionen oder
Sockets. Bereitschaft prüft beide Datenbanken mit begrenzten Timeouts.
Diese synchronen M0-Probes werden vor dem Echtzeitbetrieb in M2 in dessen
Persistenz-/Health-Verarbeitung integriert.

## Externe Kartenquellen (Vorbereitung M5)

Die sechs deutschen Kartensets von Cards vs Humanity sind als Links,
Kartenzahlen und Lizenzmetadaten erfasst. **Die sechs Original-CSV-Sets sind
unter `assets/cardpacks/cards-vs-humanity/` versioniert**, zusammen mit
`ATTRIBUTION.md` und `SHA256SUMS`. Die Originaldateien sind CC BY-NC-SA 2.0
(nur nichtkommerziell mit Attribution/ShareAlike), unabhängig vom App-Code.
CSV-Downloads und Updates lassen sich zusätzlich lokal in einem ignorierten
Ordner durchführen:

```sh
python3 bin/fetch-cvh-sets.py --list
python3 bin/fetch-cvh-sets.py
python3 -m unittest discover -s tests -p 'test_*cvh*.py'
```

Die Webseite nennt CC BY-NC-SA 2.0. Details und Quellen:
[Cards-vs-Humanity-Quellen](docs/cardpacks/CARDS-VS-HUMANITY.md).
Seit M5 sind die Sets spielbar: Der Server lädt beim Start alle Packs aus
`PACKS_DIR` (im Image `assets/cardpacks`, je Quelle ein `packs.json` mit
Lizenz und Rechteinhaber). Eine neue Session spielt alle Packs ihrer Sprache;
der Host ändert das in der Lobby unter `≡` → KARTEN-PACKS. Rechteinhaber und
Lizenz stehen in Lobby und Endstand. Bei öffentlichem Betrieb sind die
Kartenrechte trotzdem zu prüfen (nichtkommerziell, Namensnennung).

## Lizenz

Der CAW-Code steht unter der [MIT-Lizenz](LICENSE). Die Kartentexte unter
`assets/cardpacks/` behalten ihre eigene Lizenz (CC BY-NC-SA 2.0, nur
nichtkommerziell), die Spielregeln aus der X4-Firmware stehen unter MIT
(`server/src/game/LICENSE.upstream`).

## Herkunft und Konzept

Regeln aus `0b-ivan/inx-4x-pro`:
`b7b6dacd9c996b61e48a68f07b8216d08b8f7f21`. Der erste Port-Commit enthält
unveränderte Regel-Header. Herkunft, Ausnahmen und die ursprünglichen grünen
Testläufe stehen in [UPSTREAM.md](server/src/game/UPSTREAM.md) und
[docs/validation/](docs/validation/). Die MIT-Lizenz des übernommenen Codes
liegt neben den Headern. Mitgeliefert werden nur die sechs lizenzierten
Cards-vs-Humanity-Sets (siehe oben); alle Test-Fixtures sind synthetisch.

- [Konzept](docs/KONZEPT.md)
- [Vorrangiges C++-Backend-Konzept](docs/KONZEPT-CPP-BACKEND.md)
- [UI-Konzept](docs/UI-KONZEPT.md)
- [Roadmap](docs/ROADMAP.md)
- [ToDo](docs/TODO.md)
- [Prüfbericht M0/M1](docs/validation/M0-M1.md)
- [Hinweise für Agenten](CLAUDE.md)
