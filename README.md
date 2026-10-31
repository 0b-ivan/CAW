# CAW – Cards Against Whatever (Web)

Partyspiel für 3–8 Handys, mit optionalem Big Screen. M0 und M1 sind
implementiert: statische Startseite, C++-HTTP-Gerüst und portierte,
getestete Firmware-Regeln mit internem JSON-Spielzustand. Spielen im
Browser, WebSocket-Protokoll und Session-Verwaltung beginnen erst mit M2.

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
Session-Aktionen auf der Startseite sind bis M2 deaktiviert. Lizenz- und
Datenschutzseiten folgen mit M8; die Fußzeile enthält noch keine Links.

Das Produktionsgerüst wird mit
`docker compose -f deploy/compose.yml up --build -d --wait` gestartet.
Für eine echte Domain `CADDY_ADDRESS`, `PUBLIC_URL`, `HTTP_PORT=80` und
`HTTPS_PORT=443` in `deploy/.env` setzen. Caddy übernimmt dann TLS.
Domain, Zertifikate und öffentlicher Betrieb gehören zu M8 und wurden in M0
noch nicht geprüft. Nur Caddy veröffentlicht Ports. Redis und Postgres sind
intern erreichbar. Das Datenbankpasswort muss in einer URL verwendbar sein
(z.B. Hex-Zeichen); `.env` wird nicht eingecheckt.

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

`bin/smoke` erwartet einen laufenden Compose-Stack, prüft Caddy und lässt
Redis und Postgres nacheinander ausfallen und wieder anlaufen. Nur in einer
Testinstanz verwenden. Eigene Ports über `CAW_SMOKE_URL` angeben.
`web/tests/landing.cjs` prüft mit Playwright Deutsch/Englisch, Tastatur,
Tippziele und Überlauf bei 320/390/1280 px. CI installiert Playwright nur
für diesen Test; die Web-App selbst braucht weder Node noch einen Build.

Die drei Gameplay-Metriken und das Latenz-Histogramm stehen in M0 auf null;
Health-Zähler zählen echte Probes. Es gibt noch keine Spielaktionen oder
Sockets. Bereitschaft prüft beide Datenbanken mit begrenzten Timeouts.
Diese synchronen M0-Probes werden vor dem Echtzeitbetrieb in M2 in dessen
Persistenz-/Health-Verarbeitung integriert.

## Herkunft und Konzept

Regeln aus `0b-ivan/inx-4x-pro`:
`b7b6dacd9c996b61e48a68f07b8216d08b8f7f21`. Der erste Port-Commit enthält
unveränderte Regel-Header. Herkunft, Ausnahmen und die ursprünglichen grünen
Testläufe stehen in [UPSTREAM.md](server/src/game/UPSTREAM.md) und
[docs/validation/](docs/validation/). Die MIT-Lizenz des übernommenen Codes
liegt neben den Headern. Es werden keine Kartenpacks mitgeliefert; alle
Fixtures sind synthetisch.

- [Konzept](docs/KONZEPT.md)
- [Vorrangiges C++-Backend-Konzept](docs/KONZEPT-CPP-BACKEND.md)
- [UI-Konzept](docs/UI-KONZEPT.md)
- [Roadmap](docs/ROADMAP.md)
- [ToDo](docs/TODO.md)
- [Prüfbericht M0/M1](docs/validation/M0-M1.md)
- [Hinweise für Agenten](CLAUDE.md)
