# Cards Against Whatever – Web-App: Konzept

Stand: 2026-10-31 · Status: Entwurf, noch kein Code · Gestaltung und
Ansichten im Detail: [UI-KONZEPT.md](UI-KONZEPT.md)

> **Das Backend wird in C++ gebaut.** Wo dieses Dokument Node.js, TypeScript,
> Fastify, zod, Drizzle oder Vite nennt, gilt stattdessen
> [KONZEPT-CPP-BACKEND.md](KONZEPT-CPP-BACKEND.md). Regeln, Rollen, Ablauf,
> Datenmodell und Performance-Budgets bleiben wie hier beschrieben.

## 1. Ziel

Das Partyspiel aus der X4-Pro-Firmware (CrossPlay, `src/apps_local/cardparty/`
in `0b-ivan/inx-4x-pro`) als eigenständige Web-App, die auf einem eigenen
Server in Docker läuft. Spielregeln, Ablauf und Handy-UI bleiben wie auf dem
X4. Neu sind drei Dinge:

1. **Kein X4 nötig.** Wer eine Session anlegt, ist **Host** und übernimmt alles,
   was bisher der X4 entschieden hat (Start, Regeln, Pause, Spieler entfernen,
   Ende, Rematch). „Der Host ist immer König“ gilt weiter.
2. **Big Screen.** Eine optionale Ansicht für Fernseher, Beamer oder einen
   Laptop im Vollbild: Frage, Antwortsätze während der Juror entscheidet,
   Rundensieger, Endstand. Nur Anzeige, keine Bedienung.
3. **Einladen per Link und QR.** Die Session hat eine kurze URL; Host und Big
   Screen zeigen sie als QR-Code.

Nicht-Ziele (v1): Konten und Passwörter, öffentliche Lobby-Liste, Matchmaking
über das Internet, Bezahlfunktionen, native Apps, eigene Kartentexte im Repo.

## 2. Rollen und Ansichten

| Rolle | Gerät | URL | Kann |
|---|---|---|---|
| Host | Handy oder Laptop | `/s/{code}` mit Host-Token | spielen (optional), alles steuern |
| Spieler | Handy | `/s/{code}` | beitreten, Karten spielen, als Juror wählen, abwerfen, pausieren, „nochmal“ |
| Big Screen | TV, Beamer, Laptop | `/s/{code}/screen` | nur anzeigen |
| Startseite | beliebig | `/` | Session anlegen, Code eingeben |

- **Host spielt mit oder leitet nur.** Einstellung beim Anlegen, im Spiel
  änderbar. Ein Host, der nicht mitspielt, ist kein Juror und bekommt keine Hand.
- **Host-Wechsel:** Verlässt der Host die Session länger als 60 s, kann er per
  Host-Link zurück. Optional: Host übergibt aktiv an einen anderen Spieler.
- **Big Screen ohne Konto:** Er braucht nur den Session-Code und zeigt nur, was
  ohnehin öffentlich am Tisch ist. Er sieht nie eine Hand. Der Host kann den
  Big Screen auch als QR auf dem Handy anzeigen lassen und ihn so am Laptop
  öffnen; umgekehrt zeigt der Big Screen in der Lobby den Beitritts-QR.

## 3. Ablauf

```
Startseite → [NEUE SESSION] → Regeln (oder Standard) → Lobby
   Host bekommt:  https://caw.example.org/s/K7M2QX   + QR   + Host-Link (privat)
   Spieler:       Link/QR öffnen → Name → Lobby
   Big Screen:    /s/K7M2QX/screen → Lobby mit QR und Spielerliste
Lobby (≥3 Spieler) → Host: START
Runde:  Frage → Antworten (Pick 1–3) → Juror wählt → Auflösung → nächste Runde
Ende:   Endstand, Krone, „nochmal?“-Stimmen → Host: REMATCH oder ENDE
```

Big-Screen-Inhalte je Phase:

| Phase | Big Screen zeigt |
|---|---|
| Lobby | Titel, großer QR, URL, Code, Spielerliste mit Name·Wort, „x von mind. 3“ |
| Antworten | Frage groß, Runde x/y, Juror, Uhr, „4/6 abgegeben“ (keine Namen, wer fehlt) |
| Juror wählt | Frage mit **jeder Antwort eingesetzt** als ganzer Satz, nummeriert, anonym |
| Auflösung | Siegersatz mit hervorgehobenen Antworten, Name des Gewinners, Punktestand |
| Pause / Warten | Banner „PAUSE“ bzw. „WARTET AUF {Name}“ |
| Ende | Zeremonie: Krone, Endstand als Balken, Siegersätze der Partie |

## 4. Spielregeln (übernommen aus der Firmware)

Alles hier existiert bereits und ist dort getestet; die Web-App portiert es,
statt es neu zu erfinden.

- 3–8 Spieler, Hand 10 Karten, Juror rotiert, Pick 1/2/3, Draw 2 bei Pick 3.
- Antworten anonym bis zur Auflösung, Rando Cardrissian optional.
- Rundenzahl wird auf ein Vielfaches der Spielerzahl gerundet.
- Echter Ziehstapel, keine Frage wiederholt sich pro Session.
- Pro Runde eine Karte abwerfen und nachziehen.
- Antwort-Uhr mit einmaliger Verlängerung und Host-`+ZEIT`, optionale
  Juror-Uhr (Ablauf: keine Punkte), Auto-Weiter.
- Pause: setzen kann jeder, aufheben nur wer pausiert hat oder der Host.
- Fehlt ein benötigter Spieler 15 s: Tisch wartet, Host entscheidet
  „ohne ihn weiter“.
- Kick (darf in der nächsten Lobby zurück), Ende für alle, „nochmal“-Stimmen.
- Kategorien (Erwachsene standardmäßig aus), bis zu 8 Packs gemischt,
  höchstens 16 Kategorien und 4096 Karten je Art.
- Wall of Fame: Spiele, Siege, Runden, letzte Siegersätze pro Spieler-ID.

## 5. Architektur

```
            Internet / LAN
                 │ HTTPS + WSS
            ┌────▼─────┐
            │  Caddy   │  TLS (Let's Encrypt), Kompression, Security-Header
            └────┬─────┘
                 │ HTTP + WebSocket
            ┌────▼──────────────────────────────┐
            │  app (Node.js 22, TypeScript)     │
            │  ├─ HTTP: statische Web-UI, REST  │
            │  ├─ WebSocket: Spielprotokoll     │
            │  └─ Engine: reine Spiellogik      │
            └────┬───────────────────┬──────────┘
                 │ RESP              │ SQL
            ┌────▼─────┐        ┌────▼─────┐
            │  Redis   │        │ Postgres │
            │ laufende │        │ Spieler, │
            │ Sessions │        │ Fame,    │
            │ (TTL)    │        │ Packs,   │
            └──────────┘        │ Karten   │
                                └──────────┘
```

**Monorepo (pnpm workspaces):**

| Paket | Inhalt | Abhängigkeiten |
|---|---|---|
| `packages/engine` | Spielregeln als reine Funktionen/Klassen, keine IO | keine |
| `packages/protocol` | Nachrichten-Typen und Validierung (zod), Client und Server teilen sie | zod |
| `apps/server` | Fastify, `ws`, Session-Verwaltung, DB, Uhren | engine, protocol, drizzle |
| `apps/web` | Vite + TypeScript, Ansichten: Start, Spieler, Host, Big Screen | protocol |

**Warum so:**

- **Redis hält die laufenden Sessions.** Die App rechnet jede Aktion im
  Speicher (eine Partie ist ein paar KB) und schreibt den neuen Stand danach
  sofort nach Redis (`session:{code}`, TTL 24 h, AOF an). Ein Neustart oder
  Deploy der App stellt jede Session exakt wieder her; die Handys verbinden
  sich mit ihrem Ticket neu (wie auf dem X4). Abgelaufene Sessions räumt Redis
  selbst weg.
- **Pro Session genau ein Besitzer-Prozess.** v1 läuft mit einer App-Instanz.
  Für mehrere Instanzen: Lease pro Session in Redis (`SET NX PX`) plus Redis
  Pub/Sub, damit Sockets auf anderen Instanzen die Frames bekommen. Das Design
  lässt das zu, gebaut wird es erst bei Bedarf.
- **Postgres ist die dauerhafte Ablage:** Spieler-Identitäten, Wall of Fame,
  Rundenverlauf, Packs und Karten. Kein Schreibzugriff pro Klick, sondern bei
  Rundenende, Spielende und Pack-Import.
- **Ein Pack ist ein Dokument, aber keine eigene Datenbank.** Ein Pack wird als
  Ganzes importiert, ist pro Version unveränderlich und wird als Ganzes in den
  Speicher geladen (wie der X4 es von SD lädt). Es liegt deshalb als ein
  `jsonb`-Dokument in Postgres (`packs.content`). Eine eigene Dokument-DB
  (MongoDB, DocumentDB) brächte flexible Schemas und Sharding, die hier
  niemand braucht, und kostete einen weiteren Dienst samt Backup. Karten
  werden während des Spiels nie aus der DB abgefragt.
- **Engine ohne IO** heißt: dieselben Regeln laufen im Test, im Server und
  später bei Bedarf auch offline im Browser.

## 5a. Performance

Ziel ist ein Tisch, der sich sofort anfühlt, auch mit schwachem Handy-Netz,
und ein Server, der viele Partien gleichzeitig trägt. Die Zahlen sind
Abnahmekriterien, gemessen im CI-Lasttest, nicht Wünsche.

| Messgröße | Ziel |
|---|---|
| Server: Aktion empfangen → `state` an alle Sitze verschickt | p99 < 10 ms |
| Ende-zu-Ende im LAN (Tippen → anderes Handy zeigt es) | p95 < 100 ms |
| Last: 1 000 Sessions × 8 Spieler + Big Screen auf 2 vCPU / 2 GB | p99 < 50 ms, kein Fehler |
| Speicher pro laufender Session (App) | < 64 KB |
| Erstes Laden der Spieler-Seite (gzip/brotli, ohne Fonts) | < 60 KB JS+CSS, < 1 s auf 4G |
| `state`-Frame typisch / Obergrenze | < 2 KB / 16 KB |
| Kaltstart des App-Containers inkl. Wiederherstellung aus Redis | < 3 s |

Wie das erreicht wird:

- **Heißer Pfad ohne Datenbank.** Eine Aktion wird im Speicher gegen die
  Engine geprüft, angewendet und verschickt. Danach (nicht davor) geht der
  neue Stand per Pipeline nach Redis; Schreibvorgänge einer Session werden pro
  Event-Loop-Tick zusammengefasst und in Reihenfolge geschrieben. Postgres wird
  nur bei Runden- und Spielende, Fame und Pack-Import angefasst.
- **Ein Frame pro Sitz und Änderung.** Öffentlicher Teil einmal serialisiert,
  pro Sitz nur die eigene Hand angehängt. Keine Broadcast-Stürme, keine
  Polling-Schleifen; Uhren laufen clientseitig ab einem Server-Zeitstempel.
- **Backpressure.** Ist der Sende-Puffer eines Sockets voll
  (`bufferedAmount` über Grenze), wird für ihn nur der jeweils neueste Stand
  vorgemerkt; ein hängendes Handy bremst nie die anderen.
- **WebSocket-Bibliothek `ws`,** ohne `permessage-deflate` (Frames sind klein,
  Kompression kostet mehr CPU als sie spart). Wechsel auf `uWebSockets.js` nur,
  wenn der Lasttest es verlangt; die Transportschicht ist dafür austauschbar
  gekapselt.
- **Schlanke Web-UI.** Vite + TypeScript ohne großes Framework, ein Bundle pro
  Ansicht, gehashte Dateinamen, vorkomprimiert (brotli/gzip), von Caddy mit
  `Cache-Control: immutable` ausgeliefert. Die Monospace-Schrift ist eine
  System-Schrift oder ein Subset unter 30 KB.
- **Karten einmal laden.** Packs liegen nach Pack-Import als kompakte,
  vorab berechnete Struktur im Speicher (gemeinsam für alle Sessions mit
  demselben Pack-Set, nur lesend). Eine Session hält nur Karten-IDs.
- **Messbar.** `/metrics` (Prometheus-Format): Aktionen/s, Latenz-Histogramm,
  offene Sockets, Redis-Schreiblatenz, Event-Loop-Verzögerung. Ein
  Lasttest-Skript (k6 oder eigener Node-Bot-Schwarm) spielt echte Partien und
  läuft in CI gegen die Budgets.

## 6. Protokoll (WebSocket)

Übernommen vom X4, mit JSON + zod statt handgeschriebenem Parser:

- Client → Server: `join`, `resume`, `submit`, `judge`, `swap`, `pause`,
  `unpause`, `again`, `fame`, und nur für den Host: `start`, `next`, `close`,
  `abort`, `kick`, `continueWithout`, `addTime`, `end`, `rematch`, `settings`,
  `deck`, `categories`.
- Server → Client: `welcome` (Ticket, Spieler-ID, Wort), `state`
  (öffentlicher Stand + private Hand nur für den eigenen Sitz), `error` mit
  Code, `kicked`, `expired`, `fame`.
- Big Screen bekommt eigene `screen`-Frames: nur öffentliche Daten, in der
  Juror-Phase die eingesetzten Sätze.
- **Revision** in jedem `state`; `submit`/`judge` nennen die Revision, die der
  Client gesehen hat. Veraltete Befehle werden abgelehnt, der Client bekommt
  sofort den aktuellen Stand.
- Ein Ticket (128 Bit, zufällig) pro Sitz, rotiert bei jedem `resume`. Ein
  neuer Socket mit gültigem Ticket übernimmt den Sitz, der alte wird
  geschlossen.

## 7. Datenmodell (Postgres, Drizzle ORM)

| Tabelle | Zweck | Wichtige Felder |
|---|---|---|
| `sessions` | Abschluss-Datensatz einer Session (der laufende Stand liegt in Redis) | `code`, `settings` (jsonb), `pack_set`, `created_at`, `ended_at` |
| `players` | stabile Identität pro Browser | `id` (uuid), `display_name`, `word`, `first_seen`, `last_seen` |
| `session_players` | Sitz in einer Session | `session_id`, `player_id`, `seat`, `ticket_hash`, `points`, `kicked` |
| `packs` | ein Pack = ein unveränderliches Dokument pro Version | `id`, `slug`, `version`, `title`, `license`, `rights_holder`, `source_url`, `sha256`, `content` (jsonb: Kategorien, Fragen, Antworten), `imported_at` |
| `rounds` | Verlauf für Fame | `session_id`, `round`, `prompt_id`, `winner_player_id`, `sentence` |

Redis-Schlüssel: `session:{code}` (Stand als JSON inkl. Revision und Uhren),
`session:{code}:tickets` (Hash Sitz → Ticket-Hash), `ratelimit:{ip}:{route}`.

Tickets und Host-Token werden nur gehasht gespeichert. Laufende Sessions
verfallen in Redis nach 24 h ohne Aktivität; Postgres behält davon nur den
Abschluss-Datensatz und die Statistik, nach 30 Tagen ohne Sitze und Tickets.

## 8. Kartenpacks und Rechte

- **Keine fremden Kartentexte im Repo.** Gleiche Regel wie in der Firmware.
- Packs kommen per Import ins System: vorhandenes `XCAWPK2`-Format von der
  SD-Karte bzw. aus dem Release `cardparty-packs`, oder JSONL aus
  `tools_local/cardparty_l10n`. Ein Admin-Kommando im Container importiert sie.
- `Deutsch.dat` und `Alles.dat` stehen unter **CC BY-NC-SA 2.0**: nicht
  kommerziell, Namensnennung, Weitergabe unter gleichen Bedingungen. Die
  Web-App muss Titel, Lizenz und Rechteinhaber jedes aktiven Packs anzeigen
  (Fußzeile und Big Screen am Ende). Eine kommerzielle Nutzung schließt das aus.
- Eigene Packs (Nach Mitternacht, Moralisch Bankrott) laufen unter euren
  eigenen Lizenzangaben (CC0).

## 9. Sicherheit und Datenschutz

- TLS über Caddy, HSTS. WebSocket nur über `wss://`, `Origin`-Prüfung.
- Session-Code: 6 Zeichen aus 31 eindeutigen Zeichen (ohne 0/O/1/I/L),
  rund 887 Mio. Kombinationen. Plus Rate-Limit pro IP für `join` und für
  Code-Abfragen, damit niemand Codes durchprobiert.
- Host kann die Lobby **sperren** (keine neuen Beitritte) und Spieler entfernen.
- Keine Konten, keine E-Mail, keine IP-Speicherung in der DB. Gespeichert
  werden Anzeigename, Zufallswort, Statistik. Cookie/LocalStorage nur für
  Spieler-ID und Ticket; das gehört in eine kurze Datenschutzseite.
- Rate-Limits und Größenlimits für jede Nachricht (wie auf dem X4: kurze
  Namen, begrenzte Nachrichtenlänge).
- Ein Handy mit offenem TCP-Fenster blockiert nie den Tisch: Frames an hängende
  Sockets werden verworfen bzw. zusammengefasst (Backpressure).

## 10. Betrieb (Docker)

`docker compose up -d` startet `caddy`, `app`, `redis`, `postgres`.

- Volumes: `pgdata`, `redisdata`, `caddy_data`. Redis mit
  `appendonly yes`, `maxmemory` gesetzt, Policy `noeviction` (eine laufende
  Session darf nie verdrängt werden; voll heißt: keine neue Session).
- Backups: `pg_dump` täglich, 14 Tage vorhalten. Redis braucht kein Backup
  über AOF hinaus: verliert man es, enden laufende Partien, sonst nichts.
- Healthchecks: `app` `/healthz` (Redis und Postgres erreichbar,
  Event-Loop-Verzögerung), `redis-cli ping`, `pg_isready`.
- Konfiguration nur per Umgebungsvariablen (`.env`, nicht im Repo):
  `PUBLIC_URL`, `DATABASE_URL`, `REDIS_URL`, `SESSION_TTL_HOURS`, `LOG_LEVEL`.
- Logs als JSON (pino), keine Spielinhalte und keine Tickets im Log.
- Lokal: `docker compose -f compose.dev.yml up` mit Hot Reload, Caddy optional.

## 11. Offene Entscheidungen

1. Domain und Server (eigener VPS, Homelab, Hetzner?).
2. Postgres (Vorschlag: bestes `jsonb`, Drizzle-Unterstützung) oder MySQL
   (falls ihr es ohnehin betreibt). Das Schema oben passt in beide; die
   Entscheidung muss vor Meilenstein M2 fallen. Eine eigene Dokument-DB für
   Karten ist bewusst ausgeschlossen; Packs sind `jsonb`-Dokumente
   (Abschnitt 5).
3. Nur Deutsch und Englisch, oder i18n offen für mehr?
4. Big Screen: reine Anzeige (Vorschlag) oder auch Juror-Wahl per Tastatur am
   Laptop, wenn der Juror kein Handy hat?
5. Wall of Fame pro Server (wie X4 pro Gerät) oder pro Session-Gruppe
   („unsere Runde“)?
6. Soll der X4 später derselben Web-Session als Big Screen oder Host beitreten
   können? Technisch machbar, aber nicht v1.
