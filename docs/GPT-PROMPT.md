# Prompt für GPT (Start: M0 + M1)

Kopiere alles unterhalb der Linie in GPT.

---

Du arbeitest im Repository `0b-ivan/CAW`. Ziel ist eine Web-App für das
Partyspiel „Cards Against Whatever“: 3–8 Spieler am Handy im Browser, ein
Host steuert, ein optionaler Big Screen (TV/Beamer/Laptop) zeigt Frage,
eingesetzte Antwortsätze während der Juror wählt, Rundensieger und Endstand.
Es ist ein Port eines bestehenden, getesteten Spiels aus der Firmware
`0b-ivan/inx-4x-pro` (Ordner `src/apps_local/cardparty/`, Tests in
`host-tests/cardparty/`).

**Lies zuerst vollständig:** `CLAUDE.md`, `docs/KONZEPT.md`,
`docs/UI-KONZEPT.md`, `docs/ROADMAP.md`, `docs/TODO.md`. Diese Dateien sind verbindlich. Wenn du
etwas anders lösen willst, schlage es vor und ändere zuerst das Konzept, statt
still davon abzuweichen.

**Deine Aufgabe jetzt: nur Meilenstein M0 und M1.** Kein Server-Spielprotokoll,
keine UI über eine leere Startseite hinaus.

M0 – Gerüst:
1. pnpm-Monorepo mit `packages/engine`, `packages/protocol`, `apps/server`,
   `apps/web`; TypeScript strict, ESLint, Prettier, Vitest; Skript
   `pnpm check` (lint + typecheck + test).
2. `compose.yml` mit `caddy`, `app`, `redis` (appendonly yes, maxmemory,
   noeviction), `postgres`; Volumes, Healthchecks; `compose.dev.yml` mit
   Hot Reload; `.env.example`; Multi-Stage-Dockerfile, Runtime als non-root.
3. `apps/server`: Fastify mit `/healthz` (prüft Redis, Postgres und
   Event-Loop-Verzögerung) und `/metrics` (Prometheus-Format).
4. `apps/web`: Vite, die Startseite aus `docs/UI-KONZEPT.md` 4.1 mit den
   Design-Tokens aus Abschnitt 2 als gemeinsame CSS-Datei.
5. GitHub Actions: `pnpm check`, Docker-Build, Compose-Smoke-Test, der
   `/healthz` == 200 prüft.

M1 – Engine (`packages/engine`, ohne IO, ohne Abhängigkeiten, deterministisch
mit Seed, JSON-serialisierbar):
1. Lies die C++-Quellen im X4-Repo: `CardPartyCore.h`, `CardPartyTable.h`,
   `CardPartySessions.h`, `CardPartySentence.h`, `CardPartyDeckSet.h`,
   `CardPartyCategories.h`. Portiere die Regeln, erfinde keine neuen.
2. Abdecken: Phasen, Juror-Rotation, Pick 1/2/3, Draw 2, Hand 10, Rando,
   Ziehstapel ohne Wiederholung, Abwerfen 1×/Runde, faire Rundenzahl,
   Antwort-/Juror-Uhr mit Verlängerung und +Zeit, Auto-Weiter, Pause-Rechte,
   15-s-Warten auf fehlende Spieler, Kick, Ende, „nochmal“-Stimmen,
   Kategorien-Masken, Deck-Set bis 8 Packs, Siegersatz-Komposition.
3. Uhren nehmen die Zeit als Parameter (`now`), nie `Date.now()` in der Engine.
4. Portiere die passenden Tests aus `host-tests/cardparty/` als Vitest-Fälle
   (mindestens: vollständige Partien mit 3 und 8 Spielern, Uhren und Pause,
   Kick in jeder Phase, Shuffle-Verteilung, Deck-Set-Merge). Nenne im
   Commit die Quelldatei.

Harte Regeln:
- Keine Kartentexte aus fremden Quellen ins Repo. Test-Daten sind synthetisch.
- Keine Secrets im Repo. `.env` in `.gitignore`.
- Keine weiteren Datenbanken oder Dienste als im Konzept (kein MongoDB).
- Kleine Commits mit Conventional-Commit-Nachrichten auf Englisch.
- Am Ende muss `pnpm check` grün sein und `docker compose up` eine Seite
  über Caddy liefern. Hake die erledigten Punkte in `docs/TODO.md` ab.

Arbeite nur auf einem Feature-Branch, nicht direkt auf `main`, und öffne einen
Pull Request. Arbeitet ein zweiter Agent am selben Repo, sprecht euch ab:
**nie zwei Agenten gleichzeitig auf demselben Branch.**

Melde am Ende: was fertig ist, was offen ist, welche Entscheidungen du
getroffen hast und warum, und die Ausgabe von `pnpm check`.
