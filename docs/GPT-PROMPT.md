# Prompt für GPT (Start: M0 + M1, Backend in C++)

Kopiere alles unterhalb der Linie in GPT.

---

Du arbeitest im Repository `0b-ivan/CAW`. Ziel ist eine Web-App für das
Partyspiel „Cards Against Whatever“: 3–8 Spieler am Handy im Browser, ein
Host steuert, ein optionaler Big Screen (TV/Beamer/Laptop) zeigt Frage,
eingesetzte Antwortsätze während der Juror wählt, Rundensieger und Endstand.
Es ist ein Port eines bestehenden, getesteten Spiels aus der Firmware
`0b-ivan/inx-4x-pro` (Spielregeln in `src/apps_local/cardparty/`, Tests in
`host-tests/cardparty/`, beides freistehendes C++17).

**Lies zuerst vollständig:** `CLAUDE.md`, `docs/KONZEPT.md`,
`docs/KONZEPT-CPP-BACKEND.md` (hat bei Technik Vorrang), `docs/UI-KONZEPT.md`,
`docs/ROADMAP.md`, `docs/TODO.md`. Diese Dateien sind verbindlich. Wenn du
etwas anders lösen willst, schlage es vor und ändere zuerst das Konzept, statt
still davon abzuweichen.

**Wichtigstes Ziel ist lesbarer, wartbarer, pragmatischer Code.** Keine
Template-Tricks, keine Vererbungshierarchien, keine Coroutinen, keine eigenen
Allokatoren, keine cleveren Abkürzungen. Eine Klasse pro Datei, kurze
Funktionen, klare Namen, Kommentare erklären das Warum. Lieber zehn klare
Zeilen als drei schlaue. Sicherheit pragmatisch, wie im Konzept beschrieben.

**Deine Aufgabe jetzt: nur Meilenstein M0 und M1.** Kein Spielprotokoll über
WebSocket, keine UI über die Startseite hinaus.

M0 – Gerüst (Details in `docs/TODO.md`):
1. Ordner `server/`, `web/`, `deploy/` wie in KONZEPT-CPP-BACKEND.md
   Abschnitt 4.
2. CMake, C++20, `-Wall -Wextra -Werror`. Abhängigkeiten: uWebSockets,
   nlohmann/json, libpqxx, redis-plus-plus, doctest – über Debian-Pakete, wo
   vorhanden, sonst `FetchContent` mit fester Version.
3. `server/src/main.cpp`: Konfiguration aus Umgebungsvariablen, kleiner
   eigener Logger, uWebSockets mit `/healthz` (prüft Redis und Postgres) und
   `/metrics` (von Hand geschriebenes Prometheus-Textformat).
4. `deploy/`: `compose.yml` (caddy, server, redis mit AOF/maxmemory/
   noeviction, postgres; Volumes, Healthchecks), `compose.dev.yml`,
   `Caddyfile` (`web/` statisch, `/api` und `/ws` an den Server),
   `server.Dockerfile` zweistufig mit non-root-Laufzeit, `.env.example`.
5. `web/index.html` + `web/style.css`: Startseite aus UI-KONZEPT 4.1 mit den
   Tokens aus Abschnitt 2. Kein Framework, kein Build-Schritt.
6. `.clang-format`, schlichte `.clang-tidy`, Skript `bin/format`.
7. GitHub Actions: Build + `ctest`, Docker-Build, Compose-Smoke-Test
   (`/healthz` == 200).

M1 – Spielregeln übernehmen (nicht neu schreiben):
1. Kopiere die Header aus `src/apps_local/cardparty/` des X4-Repos nach
   `server/src/game/` (Core, Table, Sessions, Sentence, DeckSet, Categories,
   SdPack, History). Nenne den Quell-Commit im Commit-Text.
2. Stelle die zugehörigen Host-Tests aus `host-tests/cardparty/` auf doctest
   um und bring sie grün, **bevor** du irgendetwas an den Regeln änderst.
3. Entferne nur, was es auf dem X4 gibt und hier nicht (SD-Zugriff,
   `HalStorage`, E-Ink). Aus `kHost` (das X4-Gerät) wird die Host-Rolle der
   Web-Session. Jede Anpassung mit Test.
4. Ergänze Speichern/Laden des Spielzustands als JSON (für Redis) mit einem
   Rundlauf-Test.

Harte Regeln:
- Keine Kartentexte aus fremden Quellen ins Repo. Test-Daten sind synthetisch.
- Keine Secrets im Repo. `.env` in `.gitignore`.
- Keine weiteren Dienste als im Konzept (kein MongoDB, kein Node-Backend).
- Kleine Commits mit Conventional-Commit-Nachrichten auf Englisch.
- Am Ende: Build ohne Warnungen, `ctest` grün, `docker compose -f
  deploy/compose.yml up` liefert die Startseite über Caddy und `/healthz`
  antwortet 200. Hake die erledigten Punkte in `docs/TODO.md` ab.

Arbeite nur auf einem Feature-Branch, nicht direkt auf `main`, und öffne einen
Pull Request. Arbeitet ein zweiter Agent am selben Repo, sprecht euch ab:
**nie zwei Agenten gleichzeitig auf demselben Branch.**

Melde am Ende: was fertig ist, was offen ist, welche Entscheidungen du
getroffen hast und warum, und die Ausgabe von `ctest`.
