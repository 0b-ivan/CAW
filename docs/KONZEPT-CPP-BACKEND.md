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

## 14. Eigene Karten im Browser (nach M8)

Der Host kann in der Lobby über **HOST → EIGENE KARTEN** schwarze Fragekarten
und weiße Antwortkarten erstellen, ändern und löschen. Die Texte erscheinen
als zusätzliches Pack im aktuellen Spiel; keine fremden Karten werden in das
Repository geschrieben. Prompt-Lücken sind `___` (eine bis drei Lücken;
keine Lücke entspricht Pick 1); Pick 3 zieht zwei Zusatzkarten. Pro Session
sind maximal 100 Fragen und 100 Antworten mit je höchstens 300 UTF-8-Bytes
zulässig. Die bestehende CSV-Kartenvalidierung ist auch für den Editor
verbindlich.

- `GET/PUT /api/sessions/{code}/cards`: ausschließlich mit privatem
  `Authorization: Bearer <host-token>`; PUT nur in der Lobby. PUT nimmt ein
  vollständiges Dokument aus `prompts: string[]` und `answers: string[]`
  entgegen, lehnt ungültige Daten vollständig ab und aktiviert das Pack.
  Zwei leere Arrays entfernen das bisherige eigene Pack, sofern mindestens
  ein anderes spielbares Pack aktiv bleibt.
- Das Pack ist **sessionspezifisch**, nie in der globalen Packliste und nie
  für andere Sessions auswählbar. Es liegt mitsamt Kartentexten ausschließlich
  im privaten Redis-Room-Snapshot (Schema 5) und wird vor der Engine beim
  Restore wiederhergestellt. Snapshot-Schema 4 bleibt lesbar.
- Der Editor kann die Karten zusätzlich als JSON exportieren bzw. importieren.
  Export/Import finden lokal im Browser statt; für dauerhafte Nutzung über
  die 24-Stunden-Session-Grenze hinaus ist die eigene JSON-Datei nötig.
- Der Server akzeptiert niemals Host-Tokens im Query-String und protokolliert
  keine Kartentexte. Während laufender Runden sind Kartenänderungen gesperrt.
- Maximal acht gemischte Packs bleiben bestehen. Beim Aktivieren eines eigenen
  Packs muss ein Platz frei sein; anderenfalls informiert die UI den Host.
  Das Format und die UI müssen ohne Framework, Build-Schritt oder Datenbank
  auf dem synchronen Spiel-Thread auskommen.

## 13. Bestehender Hetzner-Server mit vorhandenem Cloudflare Tunnel

CAW läuft neben den bereits vorhandenen Anwendungen auf dem bestehenden
Hetzner-Docker-Host. Es wird **keine neue VM und kein zweiter Tunnel**
angelegt. Der vorhandene `cloudflared`-Dienst auf dem Host wird um genau
eine Route für `caw.obivan.org` ergänzt. Bereits benutzte Hostnamen,
Container, Ports und Tunnel-Routen bleiben unangetastet.

Das bestehende `deploy/compose.yml` wird für CI und lokale Entwicklung
beibehalten. Das Produktions-Override `deploy/compose.hetzner.yml` bindet
Caddy ausschließlich an `127.0.0.1:${CAW_ORIGIN_PORT:-18081}` auf dem
Host. Cloudflare Tunnel leitet `caw.obivan.org` intern an
`http://127.0.0.1:18081`; dieser Hostport ist **nicht aus dem Internet**
erreichbar. Der Port wird vor dem Start auf Kollisionen geprüft und kann
bei Bedarf über `CAW_ORIGIN_PORT` angepasst werden. Keine Host-Port-
Veröffentlichung für CAW-Server, Redis oder PostgreSQL. Die bestehende
`cloudflared`-Installation/Unit/Token wird nicht verändert; nur ihre
Published Application beziehungsweise Ingress-Regel wird ergänzt.

- Cloudflare terminiert Browser-HTTPS und WSS. Das Tunnelprotokoll verbindet
  den bestehenden Hostdienst ausgehend; die lokale Origin-Strecke ist HTTP
  über Loopback. `PUBLIC_URL=https://caw.obivan.org`; Caddy hört auf
  `:80` im eigenen Docker-Netz. Es gibt **keinen** zusätzlichen
  `cloudflared`-Compose-Dienst und keine neuen Token-Secrets im CAW-Repo.
- Die Hetzner-Firewall und die vorhandenen SSH-Zugangsregeln bleiben
  bestehen; unnötige Änderungen würden andere Anwendungen gefährden.
  Weder Port 80 noch 443 noch der CAW-Origin-Port dürfen neu als öffentlich
  erreichbare Ingress-Regeln geöffnet werden. Cloudflare Tunnel benötigt
  weiter die bereits funktionierende ausgehende Verbindung.
- `deploy/Caddyfile.hetzner` blockiert `/metrics` **ausnahmslos** als 404.
  Der Tunnel-Connector besitzt eine vertrauenswürdige lokale Quelladresse;
  eine einfache `remote_ip private_ranges`-Freigabe wäre falsch.
  Prometheus scrapt intern `server:8080/metrics`. Der öffentliche
  `CF-Connecting-IP`-Header wird für CAW-Rate-Limits in
  `X-Forwarded-For` übertragen. Externe Clients dürfen den
  Loopback-Origin nicht direkt erreichen.
- Die Cloudflare-Route muss die bestehenden Hostnamen und Ingress-Regeln
  erhalten; bei einer lokal verwalteten YAML-Konfiguration steht der
  CAW-Eintrag **vor** der abschließenden Catch-all-Regel.
  Cloudflare Access darf `/ws/*` nicht mit einer Login-Schranke blockieren.
  Für `/api/*` und `/ws/*` werden keine aggressiven Cache-Regeln gesetzt.

Vor Rollout werden mit `bin/hetzner-preflight.py` Compose-Portbindung,
geheime Umgebungsvariablen und Betreiberangaben geprüft. Danach kontrollieren
HTTPS, WSS und die öffentliche 404-Antwort auf `/metrics` das Routing.
`bin/smoke` darf **nicht auf dem Produktivserver** ausgeführt werden,
weil es absichtlich Redis/PostgreSQL neu startet.

Daten bleiben in getrennten Docker-Volumes; die bestehenden fremden
Anwendungen werden nicht in den CAW-Compose-Stack eingebunden.
Lokale Postgres-Dumps mit 14 Tagen Aufbewahrung sind kein Schutz bei
vollständigem Hostausfall. Verschlüsselte Offsite-Backups für Postgres und
Redis-AOF sowie eine Restore-Probe bleiben vor der Produktionsfreigabe
erforderlich. Ebenso rechtliche Prüfung der Karten und echte Betreiber-
angaben. Der Release-Tag `v1.0.0` wird nicht vor Live-Abnahme erzeugt.
