# Konzept-Erweiterung: Backend in C++

Stand: 2026-10-31 · Ersetzt in [KONZEPT.md](KONZEPT.md) die Abschnitte 5
(Architektur), 5a (nur die Umsetzungsdetails, die Budgets bleiben), 6
(Protokoll-Technik) und 10 (Betrieb) dort, wo diese Node.js voraussetzen.
Alles Übrige im Konzept gilt weiter.

## 1. Leitlinie

**Lesbarkeit und Wartbarkeit vor allem anderen.** Der Code soll in einem Jahr
von jemandem verstanden werden, der ihn nie gesehen hat. Dafür nehmen wir
bewusst in Kauf: etwas mehr Zeilen, etwas weniger Abstraktion, und keine
Absicherung gegen jeden denkbaren Angriff.

Konkret heißt „keine fancy Sachen“:

- Kein Template-Metaprogramming, keine Konzepte/CRTP/Typ-Tricks, keine eigenen
  Allokatoren, keine Coroutinen, kein Lock-free.
- Keine Vererbungshierarchien. Klassen sind konkrete Dinge (`Session`,
  `Game`, `PackLoader`); Polymorphie nur, wo ein Test einen Fake braucht.
- Keine Makros außer im Test-Framework.
- Ein Thread für das Spiel, ein Thread für die Persistenz. Keine Thread-Pools,
  keine geteilten Datenstrukturen außer einer Warteschlange.
- Lieber eine einfache Schleife als ein cleverer Algorithmus, solange die
  Budgets halten.

## 2. Warum C++ hier passt

- Die Spielregeln existieren schon als freistehendes C++17 in
  `0b-ivan/inx-4x-pro/src/apps_local/cardparty/` und laufen dort in
  Host-Tests auf dem PC. Sie werden **übernommen, nicht neu geschrieben**.
- Ein einzelner Prozess hält alle Partien im Speicher; C++ macht das schnell
  und mit wenig RAM, ohne Garbage Collector.

Der Preis, offen benannt: HTTP, JSON und Datenbank-Anbindung sind in C++
umständlicher als in Node; Bauzeiten sind länger. Deshalb macht das Backend
**nur das Nötigste**: WebSocket-Spielprotokoll, eine Handvoll JSON-Endpunkte,
Persistenz. Statische Dateien liefert Caddy aus, nicht der C++-Server.

## 3. Bausteine

| Aufgabe | Wahl | Grund |
|---|---|---|
| Sprache | C++20, nur „normale“ Sprachmittel | `std::string`, `std::vector`, `std::unordered_map`, `std::optional`, Lambdas |
| Build | CMake, eine `CMakeLists.txt` pro Ordner | Standard, jeder kennt es |
| HTTP + WebSocket | uWebSockets (+ uSockets) | ein Event-Loop-Thread, einfache Lambda-API, sehr schnell |
| JSON | nlohmann/json | am besten lesbar, Fehler sind verständlich |
| Postgres | libpqxx | Standard-C++-Client, einfache Transaktionen |
| Redis | redis-plus-plus (auf hiredis) | schlichte synchrone API |
| Tests | doctest | eine Header-Datei, `CHECK(a == b)` |
| Logging | eigener kleiner Logger (`log::info(...)` auf stdout) | Docker sammelt stdout; keine Bibliothek nötig |
| Abhängigkeiten | Debian-Pakete wo vorhanden, sonst CMake `FetchContent` mit fester Version | reproduzierbar ohne Paketmanager-Zoo |

## 4. Aufbau des Backends

```
server/
  CMakeLists.txt
  src/
    main.cpp                 Konfiguration lesen, alles verdrahten, Loop starten
    config.h / .cpp          Umgebungsvariablen → struct Config
    log.h / .cpp             log::info / log::warn / log::error
    game/                    Spielregeln aus der Firmware (siehe Abschnitt 5)
      Game.h  Table.h  Sentence.h  DeckSet.h  Categories.h  ...
    session/
      Session.h / .cpp       eine Partie: Game + Table + Sitze + Big-Screens
      SessionRegistry.h/.cpp Code → Session, Anlegen, Aufräumen
      Codes.h / .cpp         Session-Code und Tickets erzeugen
    net/
      Server.h / .cpp        uWebSockets: Routen, Verbindungen, Senden
      Protocol.h / .cpp      Live-Spiel: JSON-Befehle lesen, Zustands-Frames schreiben
      Messages.h / .cpp      Snapshots für Redis und REST-Antworten
                             (nur diese beiden Dateien fassen JSON an)
    store/
      PersistQueue.h / .cpp  Warteschlange + Persistenz-Thread
      RedisStore.h / .cpp    laufende Sessions speichern/laden
      PostgresStore.h / .cpp Spieler, Fame, Runden, Packs
    packs/
      PackLoader.h / .cpp    XCAWPK2 (aus der Firmware) und JSONL einlesen
  tests/
    game/ session/ net/ store/   je Ordner doctest-Dateien
```

Regeln für Dateien und Funktionen:

- Eine Klasse pro Datei, Dateiname = Klassenname.
- Funktionen bleiben unter ~40 Zeilen; wenn nicht, gibt es einen guten Grund im
  Kommentar.
- Öffentliche Methoden haben einen Satz Kommentar, *was* sie tun, wenn der
  Name es nicht schon sagt. Kommentare im Code erklären *warum*, nicht *was*.
- Fehler im Spiel: Rückgabewert `ActionResult` (Enum: `Ok`, `NotYourTurn`,
  `StaleRevision`, `Paused`, …). Keine Exceptions im Spielpfad. Exceptions nur
  beim Start (Konfiguration, DB nicht erreichbar) und dort, wo libpqxx sie
  wirft; sie werden direkt an der Aufrufstelle gefangen und geloggt.

## 5. Spielregeln aus der Firmware übernehmen

1. Die Header aus `src/apps_local/cardparty/` (Core, Table, Sessions,
   Sentence, DeckSet, Categories, SdPack, History) werden nach
   `server/src/game/` kopiert, mit Quell-Commit im Commit-Text.
2. Die zugehörigen Host-Tests aus `host-tests/cardparty/` werden mit
   übernommen und auf doctest umgestellt. **Erst wenn sie grün sind**, wird
   etwas geändert.
3. Danach nur gezielte Anpassungen für den Server, jede mit Test:
   - Feste Arrays für Namen (`char[24]`) dürfen bleiben; sie sind klar und
     schnell. Umbau auf `std::string` nur, wo es den Code lesbarer macht.
   - Zeit kommt als Parameter (`now` in Millisekunden), wie bisher.
   - `kHost` (X4) wird zum Host-Sitz bzw. Host-Token der Web-Session.
4. Was die Firmware nur wegen des X4 hat (E-Ink, SD-Karte, Hotspot,
   `HalStorage`), bleibt draußen.

### M1: interner JSON-Spielzustand

`net/Messages.cpp` speichert Game, Table und die übernommenen Sitzbindungen
in einem versionierten Snapshot (Schema 1). Enthalten sind private Hände,
anonyme Ablage mit internen Autoren, RNG, Ziehstapel-Bits, Kategorien,
Regeln, Uhren, Pause/Warten, Votes, Tickets und Revisionen. Dieser Snapshot
ist ausschließlich für interne Persistenz bestimmt und darf niemals als
öffentlicher Client-Frame verschickt oder geloggt werden.

Der Aufrufer stellt dasselbe unveränderliche Deck bereit; ein stabiler
Inhaltsfingerabdruck prüft Kartenreihenfolge, Kategorien und Pick/Draw-Werte.
Deck-Zeiger und optionale Blocker-Callbacks bleiben lokale Referenzen und
werden nicht serialisiert. Uhren speichern Restzeiten, Offline-Sitze die
verstrichene Wartezeit. Beim Laden werden sie auf den neuen monotonen
Zeitursprung bezogen; während eines Server-Neustarts steht die Spielzeit.
Ungültige oder fremde Snapshots ändern den vorhandenen Zustand nicht.

Die übernommenen numerischen Verbindungsbindungen sind in M1 lediglich
Teil des vollständigen Regelzustands. M2 muss beim echten Server-Neustart
alte Socket-Bindungen lösen und Tickets beim Resume drehen. M1 eröffnet
keine Netzwerk-Session und prüft keine Host-Token. `kHost` bezeichnet die
authentifizierte Host-Rolle unabhängig von einem Spieler-Sitz; M2 prüft den
Host-Token, bevor es die übernommenen Host-Methoden aufruft.

## 6. Ablauf im Prozess

```
            uWebSockets-Loop (Thread 1, besitzt alle Sessions)
  WS-Nachricht ─► Protocol::parse ─► Session::handle(Befehl, now)
                                         │ Game/Table ändern
                                         ├─► Frames bauen, an Sitze + Big Screens senden
                                         └─► PersistQueue::push(Snapshot)  (kopiert, kein Warten)
  Timer alle 250 ms ─► jede Session: Table::tick(now) ─► ggf. Frames + Snapshot

            Persistenz-Thread (Thread 2)
  PersistQueue ─► neuester Snapshot je Session ─► RedisStore::save
              ─► Rundenende/Spielende ─► PostgresStore (Fame, Runden)
```

- **Nur Thread 1 fasst Sessions an.** Damit braucht die Spiellogik keine
  Locks. Thread 2 bekommt Kopien (JSON-Strings) über eine Warteschlange mit
  einem `std::mutex` und einer `std::condition_variable`. Liegen für eine
  Session mehrere Snapshots an, wird nur der neueste geschrieben.
- **Start:** Alle Sessions aus Redis laden, dann Loop starten. Handys
  verbinden sich mit ihrem Ticket neu.
- **Herunterfahren (SIGTERM):** keine neuen Verbindungen, Warteschlange
  leeren, Sockets schließen.
- **Hängendes Handy:** uWebSockets meldet einen vollen Sende-Puffer
  (`getBufferedAmount`); dann wird für diesen Socket nur der neueste Stand
  vorgemerkt und beim nächsten `drain` geschickt.

## 7. Schnittstellen

- `POST /api/sessions` → `{ code, hostToken }`
- `GET  /api/sessions/{code}` → existiert? Sprache, Phase (für die Startseite)
- `GET  /api/fame` → Ruhmeshalle (Namen mit Wort, Zähler, Siegersätze), aus dem Speicher
- `GET  /api/packs` → alle geladenen Packs mit Titel, Sprache, Lizenz, Rechteinhaber, Quelle, Kartenzahl
- `GET  /healthz` → 200, wenn Redis und Postgres erreichbar sind
- `GET  /metrics` → Prometheus-Textformat, von Hand geschrieben: Sessions,
  Sockets, Aktionen mit Latenz-Histogramm, längster Tick, Frame-Größen,
  Speicher, Rate-Limit-Treffer
- Rate-Limits pro Client-Adresse (X-Forwarded-For von Caddy): Code-Abfrage und
  Socket-Aufbau `RATE_LIMIT_PER_MINUTE` (Standard 60), Anlegen ein Zehntel;
  zu viel → 429 `rate_limited`. Im Server, weil Caddy ohne Plugin keine
  Rate-Limits kann.
- `WS   /ws/{code}` (Spieler und Host), `WS /ws/{code}/screen` (Big Screen)

Nachrichten wie in KONZEPT.md Abschnitt 6, als JSON. `Protocol.cpp` ist die
einzige Stelle, die WebSocket-JSON in Befehle umwandelt und Zustände in JSON;
sie prüft Typen und Längen und lehnt alles andere ab, auch unbekannte Felder.
`Messages.cpp` macht dasselbe für Snapshots und die REST-Antworten. Getrennt,
weil eine Datei für beides zu lang und unübersichtlich würde.

Befehle (Client → Server), alle mit `type`:

| type | Felder | Wer |
|---|---|---|
| `join` | `name`, optional `player` (Geräte-ID, 16 Hex) | jeder (Lobby); die ID verlässt den Server nie |
| `resume` | `token` (Sitz-Ticket) | Spieler nach Reload |
| `host` | `token` (Host-Token) | macht die Verbindung zum Host |
| `submit` | `token`, `revision`, `slots` | Spieler |
| `judge` | `token`, `revision`, `index` | Juror |
| `swap` | `token`, `revision`, `index` | Spieler, 1× pro Runde |
| `pause`, `unpause`, `again` | `token` | Spieler |
| `start`, `next`, `close`, `abort`, `end`, `rematch`, `hostPause`, `hostUnpause`, `addTime`, `continueWithout` | – | Host |
| `kick` | `index` (Sitz) | Host |
| `settings` | `settings`: `rounds`, `answerSecs`, `extensionSecs`, `jurorSecs`, `autoSecs`, `rando` | Host; Runden nur in der Lobby, Werte wie auf dem X4 |
| `packs` | `packs`: 1–8 Pack-IDs | Host, nur in der Lobby; `state` nennt in Lobby und Ende die aktiven Packs mit Lizenz |
| `lock`, `unlock` | – | Host; gesperrt lehnt `join` mit `locked` ab, `state.locked` zeigt es |

Antworten (Server → Client): `welcome` (`token`, `seat`; das Ticket rotiert bei
jedem `resume`), `state` (öffentlicher Stand plus `me` mit eigener Hand),
`error` (`code`), sowie `host`, `expired`, `replaced`, `kicked`. Auf `host`
folgt sofort ein `state`, damit die Host-Seite den Tisch ohne Warten zeigt.
Nach `replaced` und `kicked` schließt der Server den Socket.

Der Big Screen verbindet sich mit `/ws/{code}/screen`. Er bekommt sofort und
nach jeder Änderung einen `screen`-Frame: derselbe öffentliche Stand wie
`state`, aber ohne `me`, also nie eine Hand oder ein Ticket. Er sendet nichts;
jede Nachricht wird mit `error` `screen_read_only` beantwortet.

## 8. Sicherheit: bewusst pragmatisch

Behalten, weil billig und wichtig:

- Server entscheidet alles; Clients schicken nur Absichten.
- Zufällige Tickets (128 Bit) pro Sitz, Host-Token; niemand sieht fremde Hände.
- Größenlimit pro Nachricht, Längenlimit für Namen.
- TLS und einfache Rate-Limits in Caddy, nicht im C++-Code.

Bewusst weggelassen bzw. später:

- Kein Hashen von Tickets in Redis (Redis ist nur intern erreichbar).
- Keine `Origin`-Prüfung, kein eigenes Rate-Limiting im Backend.
- Keine Abwehr gegen gezielte Angriffe über das hinaus, was Caddy liefert.

## 9. Frontend: genauso schlicht

- Statische Dateien: HTML, CSS, JavaScript (ES-Module), **kein Framework,
  kein Bundler, kein Build-Schritt.** Grundlage ist die vorhandene
  `CardPartyPage.html` der Firmware.
- Eine gemeinsame `style.css` (Tokens aus UI-KONZEPT.md), je Ansicht eine
  HTML-Datei und ein JS-Modul: `index`, `play`, `screen`.
- QR-Codes per kleiner, eingebundener JS-Bibliothek (eine Datei, Version
  fest im Repo).
- Caddy liefert `web/` direkt aus, mit gzip/brotli.

## 10. Docker

```
deploy/
  compose.yml          caddy, server, redis, postgres
  compose.dev.yml      server mit Debug-Build, web/ als Volume (Seite neu laden reicht)
  Caddyfile            web/ statisch, /api und /ws an server:8080
  server.Dockerfile    Stage 1: debian + cmake + Pakete → Release-Build
                       Stage 2: debian-slim + Laufzeit-Bibliotheken + Binary, non-root
```

Umgebungsvariablen wie bisher: `PUBLIC_URL`, `DATABASE_URL`, `REDIS_URL`,
`SESSION_TTL_HOURS`, `LOG_LEVEL`, zusätzlich `PORT` (Standard 8080).

## 11. Code-Stil (verbindlich)

- `clang-format` mit einer schlichten Konfiguration (Google-Basis, 100
  Zeichen); `clang-tidy` nur mit wenigen, nützlichen Checks (Bugprone,
  Modernize-Grundlagen), Warnungen als Fehler im CI.
- Namen: Klassen `PascalCase`, Funktionen und Variablen `camelCase`,
  Konstanten `kPascalCase`, Member mit `_` am Ende (`players_`).
- `auto` nur, wo der Typ rechts offensichtlich ist.
- Keine Rohzeiger mit Besitz; `std::unique_ptr` für Besitz, Referenzen für
  Zugriff.
- Includes: eigene Header zuerst, dann Bibliotheken, dann Standard.
- Jede Datei beginnt mit einem Satz, wofür sie da ist.

## 12. Auswirkungen auf Roadmap und ToDo

- M0: CMake-Gerüst, Docker, Caddy statt pnpm/Vite.
- M1: Firmware-Header + Tests übernehmen statt TypeScript-Port. Deutlich
  weniger Arbeit, geringeres Fehlerrisiko.
- M2: uWebSockets-Server, Messages, Sessions, Persistenz-Thread.
- M3/M4: statisches Frontend auf Basis der Firmware-Seite.
- Lasttest: kleines C++- oder Python-Bot-Programm, das echte Partien spielt.

## 13. Hetzner Cloud mit Cloudflare Tunnel

CAW v1 läuft auf **einem** Hetzner-Cloud-Server (Ubuntu 24.04 LTS).
Ein dedizierter, im Cloudflare-Dashboard verwalteter Tunnel veröffentlicht
`caw.obivan.org`. `cloudflared` läuft zusammen mit dem vorhandenen Compose-
Stack und verbindet sich **ausgehend** zu Cloudflare; Caddy hört nur im
Docker-Netz auf HTTP :80. Es gibt **keine öffentlichen HTTP-/HTTPS-Ports**
auf Hetzner, weder für Caddy noch für Redis, Postgres oder Prometheus.
Das `deploy/compose.hetzner.yml`-Override entfernt die Port-Mappings von
Caddy vollständig; das bisherige `deploy/compose.yml` bleibt für CI/Lokal.
Es wird kein zweites Backend und kein Kubernetes eingeführt.

- Die Hetzner Cloud Firewall lässt eingehend ausschließlich SSH TCP 22
  vom **eigenen Admin-IP-Netz** zu. Ausgehend muss Cloudflare Tunnel über
  TCP/UDP 7844 erreichbar sein; 443/53 für Updates, DNS und Paketbezug.
  Auf dem Host wird SSH-Key-Login erzwungen, Passwortlogin deaktiviert.
- Die Cloudflare-Route wird **im Dashboard** als Published Application
  `caw.obivan.org` → `http://caddy:80` angelegt. DNS wird über den
  Tunnel verwaltet; **keine A-/AAAA-Records zur Hetzner-Origin-IP**.
  Bei einem bereits verwendeten Tunnel muss dessen Connector das
  Docker-Netz erreichen. Ein separater Tunnel für CAW ist übersichtlicher
  und verhindert, dass fremde Connector-Replikate an falsche Origins gehen.
- TLS wird von **Cloudflare am öffentlichen Edge** terminiert, nicht von
  Caddy; die Strecke Cloudflare ↔ cloudflared ist verschlüsselt, die
  lokale Strecke cloudflared ↔ Caddy läuft über das private Docker-Netz.
  `PUBLIC_URL=https://caw.obivan.org`. HTTPS und WSS müssen von außen
  getestet werden; ohne Tunnel ist die Web-App nicht öffentlich erreichbar.
- Nur `cloudflared` liest das Token aus einer **nicht versionierten**
  Datei `deploy/secrets/cloudflared-token` per `--token-file`. Das
  Token wird weder in Env, Shell-Befehl, Git noch Logs gespeichert.
- Die Tunnel-Konfiguration blockiert `/metrics` **immer** an Caddy:
  Die übliche `remote_ip private_ranges`-Abfrage ist hinter `cloudflared`
  gefährlich, weil das Gateway selbst eine private Docker-IP besitzt.
  Prometheus erreicht weiterhin `server:8080/metrics` direkt intern.
  Caddy setzt die Rate-Limit-Quell-IP aus `CF-Connecting-IP` als
  `X-Forwarded-For` für den C++-Server. Der Caddy-Container wird
  nicht direkt aus dem Internet erreichbar gemacht.
- Cloudflare Access darf die öffentliche Spieloberfläche oder die
  WebSocket-Pfade nicht durch eine Login-Seite blockieren; sonst können
  anonyme Mitspieler nicht beitreten. Keine aggressiven Caching-Regeln
  für `/api/*` oder `/ws/*`.

Persistente Docker-Volumes bleiben über Container-Updates erhalten.
Postgres-Dumps im Compose-Volume (14 Tage) sind nur **lokale Sicherungen**:
Zusätzlich müssen verschlüsselte Offsite-Backups und eine Restore-Probe
eingerichtet werden; Redis-AOF muss für vollständige laufende Sessions
ebenfalls außerhalb des Servers gesichert werden. Hetzner-Snapshots allein
sind kein konsistentes Postgres-Backup. Der Produktions-Rollout benutzt
ausschließlich `bin/hetzner-preflight.py` und die unter `deploy/hetzner/`
dokumentierten Schritte. `bin/smoke` ist **nicht** für Produktion geeignet,
weil es Redis und Postgres absichtlich neu startet.

Die rechtliche Freigabe der Karten, das echte Impressum und ein geprüfter
Public-HTTPS-/WSS-Test bleiben Voraussetzung für `v1.0.0`; es wird kein
Tag automatisch erstellt.
