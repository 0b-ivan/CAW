# CAW – Cards Against Whatever (Web)

Partyspiel für 3–8 Handys im Browser, plus optionaler Big Screen (TV, Beamer,
Laptop). Port des CrossPlay-X4-Spiels aus `0b-ivan/inx-4x-pro`
(`src/apps_local/cardparty/`) auf einen eigenen Server in Docker.

Lies vor jeder Arbeit: [docs/KONZEPT.md](docs/KONZEPT.md) (Regeln,
Performance-Budgets), [docs/KONZEPT-CPP-BACKEND.md](docs/KONZEPT-CPP-BACKEND.md)
(Backend in C++, **hat Vorrang** vor KONZEPT.md bei Technik-Fragen),
[docs/UI-KONZEPT.md](docs/UI-KONZEPT.md) (Gestaltung, Ansichten, Big Screen),
[docs/ROADMAP.md](docs/ROADMAP.md), [docs/TODO.md](docs/TODO.md). Was dort
entschieden ist, gilt; wer davon abweichen will, ändert zuerst das Konzept und
begründet es.

**Oberstes Ziel: lesbarer, wartbarer, pragmatischer Code.** Keine cleveren
Tricks, keine unnötigen Abstraktionen. Lieber zehn klare Zeilen als drei
schlaue. Sicherheit so viel wie nötig, nicht so viel wie möglich.

## Stack

- Backend `server/`: C++20, CMake, uWebSockets (HTTP + WebSocket), nlohmann/json,
  libpqxx (Postgres), redis-plus-plus (Redis), doctest (Tests)
- Spielregeln `server/src/game/`: übernommen aus der X4-Firmware, nicht neu geschrieben
- Frontend `web/`: statisches HTML, CSS, JavaScript-Module; kein Framework, kein Build-Schritt
- Docker Compose `deploy/`: `caddy` (TLS, statische Dateien), `server`, `redis`, `postgres`

## Befehle

```sh
cmake -S server -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure      # muss vor jedem Commit grün sein
./bin/format                                     # clang-format auf geänderte Dateien
docker compose -f deploy/compose.dev.yml up     # lokal
docker compose -f deploy/compose.yml up -d      # Produktion
```

Gibt es einen dieser Befehle noch nicht, ist das Teil der aktuellen Aufgabe,
nicht ein Grund, ihn zu überspringen.

## Regeln

- **Spielregeln nicht neu erfinden.** Die Firmware hat sie getestet. Beim
  Portieren die Quelle (Datei im X4-Repo) im Commit nennen und den passenden
  Test mitportieren.
- **Server ist autoritativ.** Clients schicken Absichten, nie Ergebnisse.
  JSON wird nur in `server/src/net/Messages.cpp` gelesen und geschrieben.
- **Ein Thread besitzt alle Sessions** (der uWebSockets-Loop). Der
  Persistenz-Thread bekommt nur Kopien über die Warteschlange. Keine weiteren
  Threads, keine Locks in der Spiellogik.
- **Code-Stil** nach KONZEPT-CPP-BACKEND.md Abschnitt 4 und 11: eine Klasse pro
  Datei, Funktionen kurz, Fehler als `ActionResult` statt Exceptions im
  Spielpfad, kein Template-Zauber, keine Vererbungshierarchien.
- **Heißer Pfad ohne Datenbank.** Aktion → Spielregeln → Frames verschicken →
  Snapshot in die Warteschlange. Postgres nur bei Rundenende, Spielende,
  Fame, Pack-Import.
- **Kein Spieler sieht fremde Hände.** Der Big Screen bekommt nur öffentliche
  Daten. Keine Spieler-IDs, Tickets oder Host-Token an fremde Clients oder ins Log.
- **Fremde Kartentexte nur als geprüfte, lizenzierte Ausnahme:** Die sechs
  unveränderten Original-CSV-Sets unter `assets/cardpacks/cards-vs-humanity/`
  sind gemäß `docs/KONZEPT.md` Abschnitt 8 und benachbarter `ATTRIBUTION.md`
  ausdrücklich erlaubt (CC BY-NC-SA 2.0, nichtkommerziell, ShareAlike).
  Keine weiteren Fremdsets ohne Quellen-/Rechteprüfung. Sonstige Packs
  werden erst durch den M5-Importer in die DB geladen. Testdaten sind
  ausschließlich synthetisch („Testfrage 1 ___“).
- **Performance-Budgets sind Abnahmekriterien.** Eine Änderung, die den
  Lasttest über ein Budget schiebt, ist nicht fertig.
- **Konfiguration nur über Umgebungsvariablen.** Keine Secrets im Repo,
  `.env` ist in `.gitignore`.
- **UI-Texte** über die Übersetzungstabellen (Deutsch und Englisch), nie
  hart im Markup. Gestaltung strikt nach `docs/UI-KONZEPT.md`: monochrom,
  Monospace, Linien statt Kästen, keine Farben oder Effekte.
- **Kleine Commits**, Conventional Commits (`feat:`, `fix:`, `test:`,
  `docs:`, `chore:`), Nachricht auf Englisch, erklärt das Warum.
- Code und Kommentare auf Englisch, Doku für Menschen auf Deutsch.

## Definition of Done

Build ohne Warnungen, `ctest` grün, neue Logik hat Tests, betroffene
Playwright-Fälle grün, Docker-Build grün, Code ist so geschrieben, dass ihn
jemand ohne Erklärung versteht, TODO.md abgehakt, Konzept aktualisiert, wenn sich eine
Entscheidung geändert hat.
