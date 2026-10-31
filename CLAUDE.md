# CAW – Cards Against Whatever (Web)

Partyspiel für 3–8 Handys im Browser, plus optionaler Big Screen (TV, Beamer,
Laptop). Port des CrossPlay-X4-Spiels aus `0b-ivan/inx-4x-pro`
(`src/apps_local/cardparty/`) auf einen eigenen Server in Docker.

Lies vor jeder Arbeit: [docs/KONZEPT.md](docs/KONZEPT.md) (Architektur, Regeln,
Performance-Budgets), [docs/UI-KONZEPT.md](docs/UI-KONZEPT.md) (Gestaltung,
Ansichten, Big Screen), [docs/ROADMAP.md](docs/ROADMAP.md),
[docs/TODO.md](docs/TODO.md). Was dort entschieden ist, gilt; wer davon
abweichen will, ändert zuerst das Konzept und begründet es.

## Stack

- Node.js 22, TypeScript (strict), pnpm-Workspaces
- `packages/engine`: Spielregeln, **keine IO, keine Abhängigkeiten**, deterministisch mit Seed
- `packages/protocol`: Nachrichtentypen + zod-Schemas, von Server und Web geteilt
- `apps/server`: Fastify, `ws`, Redis (laufende Sessions), Postgres über Drizzle (dauerhafte Daten)
- `apps/web`: Vite + TypeScript, kein großes UI-Framework; Ansichten Start, Spieler/Host, Big Screen
- Docker Compose: `caddy`, `app`, `redis`, `postgres`

## Befehle

```sh
pnpm install
pnpm check            # lint + typecheck + test, muss vor jedem Commit grün sein
pnpm test             # Vitest
pnpm e2e              # Playwright (Handy-Breiten 320/390, Desktop 1280)
pnpm load             # Lasttest gegen die Budgets aus KONZEPT 5a
docker compose -f compose.dev.yml up    # lokal mit Hot Reload
docker compose up -d                     # Produktion
```

Gibt es einen dieser Befehle noch nicht, ist das Teil der aktuellen Aufgabe,
nicht ein Grund, ihn zu überspringen.

## Regeln

- **Spielregeln nicht neu erfinden.** Die Firmware hat sie getestet. Beim
  Portieren die Quelle (Datei im X4-Repo) im Commit nennen und den passenden
  Test mitportieren.
- **Server ist autoritativ.** Clients schicken Absichten, nie Ergebnisse. Jede
  Nachricht wird per zod geprüft; unbekannte Felder werden abgelehnt.
- **Heißer Pfad ohne Datenbank.** Aktion → Engine → Frames verschicken → dann
  Redis. Postgres nur bei Rundenende, Spielende, Fame, Pack-Import.
- **Kein Spieler sieht fremde Hände.** Der Big Screen bekommt nur öffentliche
  Daten. Keine Spieler-IDs, Tickets oder Host-Token an fremde Clients oder ins Log.
- **Keine Kartentexte im Repo.** Packs kommen per Import in die DB. Test-Daten
  sind synthetisch („Testfrage 1 ___“).
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

`pnpm check` grün, neue Logik hat Tests, betroffene Playwright-Fälle grün,
Docker-Build grün, TODO.md abgehakt, Konzept aktualisiert, wenn sich eine
Entscheidung geändert hat.
