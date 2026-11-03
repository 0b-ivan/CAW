# CAW auf dem vorhandenen Hetzner-Server

Stand: 2026-11-01. Ziel: `https://caw.obivan.org`.
**Kein neuer Server, kein zweiter Cloudflare-Tunnel und kein Release-Tag.**

CAW wird als eigenständiger Docker-Compose-Stack neben den vorhandenen
Diensten betrieben. Der bestehende `cloudflared`-Systemdienst leitet einen
weiteren Hostnamen an einen **nur lokal erreichbaren** Origin-Port.
Bestehende Tunnel-Routen und Deployments (z. B. Blog und weitere Dienste)
bleiben unverändert.

```text
Handy / TV  -- HTTPS + WSS --> Cloudflare Edge
                                      |
                              bestehender Tunnel
                                      |
                        cloudflared (Hetzner systemd)
                                      |
                          http://127.0.0.1:18081
                                      |
                   Caddy (CAW Compose, intern :80)
                            |                |
                       statische UI     C++-Server
                                           |    |
                                         Redis Postgres
```

Cloudflare terminiert öffentliches TLS. Die Verbindung zum lokalen Caddy
ist HTTP über Loopback. Es werden **keine** öffentlichen Ports 80/443/18081
für CAW geöffnet. Postgres, Redis und der C++-Server veröffentlichen keine
Hostports. Prometheus ist optional nur über Loopback erreichbar.

## 1. Bestehenden Host prüfen – nichts neu installieren, solange es läuft

Per SSH am vorhandenen Hetzner-Host:

```sh
cat /etc/os-release | head -n 3
docker version
docker compose version
systemctl is-active cloudflared
sudo ss -ltnp | grep ':18081 ' || true
docker ps --format 'table {{.Names}}\t{{.Ports}}'
```

Wenn `18081` belegt ist, in `deploy/.env` einen anderen freien Port
als `CAW_ORIGIN_PORT` setzen und denselben Port in der Cloudflare-Route
verwenden. Vorhandene Docker-Stacks, Cloudflare-Tunnel, SSH- oder
Firewall-Regeln **nicht** zurücksetzen. `docker compose down` nur mit
den CAW-Dateien und nie mit `--volumes` ausführen.

Die bisherige Hetzner-Firewall und die vorhandenen SSH-Key-Zugänge
beibehalten. Keine zusätzlichen Inbound-Ports für CAW freigeben.

## 2. CAW isoliert auschecken

```sh
sudo mkdir -p /opt/caw
sudo chown "$USER:$(id -gn)" /opt/caw
git clone https://github.com/0b-ivan/CAW.git /opt/caw
cd /opt/caw
# Bis PR #11 gemergt wurde:
git switch feat/hetzner-deployment-prep-20261031
# Danach: git switch main && git pull --ff-only

cp deploy/hetzner/env.example deploy/.env
openssl rand -hex 32
```

Das erzeugte Passwort in `deploy/.env` bei `POSTGRES_PASSWORD` eintragen,
danach `chmod 600 deploy/.env`. Keine Secrets in Git oder Chat kopieren.
`CAW_ORIGIN_PORT=18081` bei Bedarf ändern, `PUBLIC_URL` bleibt
`https://caw.obivan.org`.

**Das vorhandene `cloudflared`-Token wird weder kopiert noch geändert.**
Es wird kein zusätzlicher Tunnel-Container gestartet.

Für die endgültige öffentliche Freigabe:

```sh
cp web/operator.example.json web/operator.json
# Hier echte Betreiberangaben eintragen, erst nach deiner Freigabe.
nano web/operator.json
```

Die Datei wird von Git ignoriert. Die gelieferten Drittanbieter-Karten
dürfen nur im Rahmen ihrer CC BY-NC-SA 2.0-Lizenz genutzt werden.
Die rechtliche Freigabe bleibt separat offen.

## 3. CAW lokal auf Hetzner starten und prüfen

Vor vollständigem Go-live ist die Prüfung ohne Impressum mit
`--prepare` möglich. Das ist **keine** rechtliche Freigabe.

```sh
cd /opt/caw
python3 bin/hetzner-preflight.py --prepare

docker compose -f deploy/compose.yml -f deploy/compose.hetzner.yml \
  config --format json >/dev/null
docker compose -f deploy/compose.yml -f deploy/compose.hetzner.yml \
  -p caw up --build --detach --wait
docker compose -f deploy/compose.yml -f deploy/compose.hetzner.yml \
  -p caw ps

curl -fsS http://127.0.0.1:18081/healthz
curl -s -o /dev/null -w '%{http_code}\n' \
  http://127.0.0.1:18081/metrics # erwartet: 404
```

Wenn ein anderer `CAW_ORIGIN_PORT` gesetzt wurde, auch die
`curl`-Befehle anpassen. Alle CAW-Container und Volumes gehören zum
Compose-Projekt `caw`; fremde Deployments bleiben unberührt.

Das Docker-Override verwendet `ports: !override` und benötigt
**Docker Compose v2.24.4 oder neuer**, um die Entwicklungs-Ports
der Basisdatei vollständig zu ersetzen. Vor dem Start die Ausgabe
von `docker compose ... config --format json` durch den Preflight
validieren. Eine Direktveröffentlichung an `0.0.0.0` wäre ein Fehler.

## 4. Vorhandenen Cloudflare-Tunnel um eine Route erweitern

`cloudflared` auf dem Hetzner-Host ist bereits vorhanden. Nur die
neue Route hinzufügen:

| Eigenschaft | Wert |
|---|---|
| Öffentlicher Hostname | `caw.obivan.org` |
| Service type | HTTP |
| Origin-URL | `http://127.0.0.1:18081` |
| Öffentliche Verbindungen | HTTPS und WSS |

**Remote-managed Tunnel:** Im Cloudflare-Zero-Trust-Dashboard unter
Networking → Tunnels den **bestehenden Hetzner-Tunnel** wählen und
eine Published Application/Hostname hinzufügen. Alle vorhandenen
Hostname-Routen erhalten. Cloudflare richtet die erforderliche
Tunnel-DNS-Verknüpfung ein; **keinen öffentlichen A-/AAAA-Eintrag** zur
Hetzner-Origin-IP setzen.

**Lokal verwalteter Tunnel:** Die vorhandene
`/etc/cloudflared/config.yml` (oder den dort tatsächlich verwendeten
Konfigurationspfad) sichern. In `ingress:` **vor** der letzten
Catch-all-Regel nur Folgendes ergänzen:

```yaml
  - hostname: caw.obivan.org
    service: http://127.0.0.1:18081
```

Dann Konfiguration validieren und den bestehenden Dienst erst während
eines geeigneten Wartungsfensters neu laden/neu starten.
Vorhandene Einträge **nicht überschreiben**. Beim remote-managed Tunnel
keine lokale YAML-Änderung vornehmen. Cloudflare Access darf anonyme
Mitspieler und `/ws/*` nicht durch eine Login-Seite blockieren;
`/api/*` und `/ws/*` nicht zwischenspeichern.

## 5. Öffentliche Abnahme

Sobald Route und HTTPS bereit sind:

```sh
python3 bin/hetzner-preflight.py --public
node bin/hetzner-wss-check.mjs
curl -fsS https://caw.obivan.org/healthz
curl -s -o /dev/null -w '%{http_code}\n' \
  https://caw.obivan.org/metrics   # muss 404 sein
```

Der Node-Test legt nur einen temporären Raum an und prüft den echten
WSS-Host-Handshake; er gibt den Host-Token nicht aus. Abschließend
eine Partie mit mindestens drei echten Handys und optionalem Big Screen
einschließlich Reconnect durchspielen. Cloudflare stellt das
Browser-Zertifikat bereit; Caddy benötigt für diesen Loopback-Origin
kein eigenes Zertifikat.

`bin/smoke` **nie** auf dem produktiven Hetzner-Host ausführen:
das Skript stoppt absichtlich Postgres und Redis.

## 6. Backups, Betrieb und Go-live-Blocker

- CAW bringt eigene Docker-Volumes für Postgres, Redis-AOF und Backups mit.
  Der `backup`-Dienst legt lokale Postgres-Dumps für 14 Tage an.
  Bereits laufende andere Container, Volumes und Backups werden nicht
  verändert.
- Vor dem öffentlichen Betrieb müssen **verschlüsselte Offsite-Backups**
  von Postgres-Dumps und Redis-AOF mit regelmäßiger Restore-Probe
  eingerichtet werden. Hetzner-Snapshots sind nur eine Ergänzung.
  Der Ziel-Speicher und Zugangsdaten fehlen noch.
- **Lokaler Restore-Test ohne Storage Box:** `bin/verify-local-restore`
  liest den neuesten Dump aus dem CAW-Backup-Volume, startet ein
  netzwerkisoliertes, kurzlebiges `postgres:17-alpine`, stellt die Tabellen
  `players` und `winning_lines` wieder her und entfernt die Testumgebung.
  Als `caw-deploy` auf Hetzner ausführen, **ohne** das aktuelle
  Production-Checkout oder die von Caddy live eingebundenen Web-Dateien
  zu verändern:

  ```sh
  sudo -u caw-deploy -H bash -c '
    set -Eeuo pipefail
    cd /opt/caw
    git fetch --quiet origin main
    git show origin/main:bin/verify-local-restore |
      CAW_REPO_ROOT=/opt/caw bash
  '
  ```

  Alternativ nach einem regulären Deployment `bin/verify-local-restore`
  direkt ausführen. Das vorhandene Skript `bin/restore` **nicht ohne explizite Zieldatenbank**
  ausführen: Sein Standardziel ist die **produktive** Datenbank `caw`.
  Der isolierte Test benötigt keine Netzverbindung und verändert keine
  produktiven Datenbanken oder Container.
  **Ein lokaler Restore ist kein Offsite-Backup:** Bei vollständigem
  Server- oder Volume-Verlust können Daten trotzdem verloren gehen.
- Ein Update muss im CAW-Compose-Projekt stattfinden, nicht mit einem
  Host-weiten Docker-Cleanup. Vorher Datenbank/Redis sichern.
- Monitoring über das bestehende Monitoring des Hosts oder optional
  das Compose-Profil; `/metrics` ist über Cloudflare bewusst 404.
  Der interne Prometheus-Scrape nutzt `server:8080/metrics`.
- **Noch offen:** echte Betreiberangaben, Offsite-Backup-Ziel,
  rechtliche Freigabe der Kartendaten, externe TLS-/WSS-Abnahme.
  Erst danach den Release-Tag `v1.0.0` erstellen.

Es wird kein zusätzlicher Hetzner-Server, kein Tunnel und kein
Cloudflare-Token automatisiert angelegt.


## 7. Automatische Updates aus GitHub Actions (ohne Freigabe zunächst inaktiv)

Der Workflow `.github/workflows/deploy-hetzner.yml` startet bei
**Pushes auf `main`** und kann manuell gestartet werden. Ein Job darf
allerdings **erst nach erfolgreicher `main`-Push-CI für genau denselben
Commit** das Image bauen und deployen. Dafür fragt er die GitHub-Actions-
Run-API ab (bis zu 15 Minuten); eine PR-CI genügt nicht. Schlägt die
Push-CI fehl, läuft die Abfrage ab oder ist der Commit nicht mehr auf
`main`, bleibt Hetzner unverändert. Diese Form umgeht die beobachteten
`BuildFailed`/`startup_failure`-Starts des `workflow_run`-Triggers.
**Ab 2026-11-01 technisch aktiviert auf ausdrücklichen Betreiberwunsch:**
Jeder `main`-Push nach grüner vollständiger CI löst automatisch einen
Deployment-Versuch aus. Die früheren Repository-Variablen
`CAW_PRODUCTION_APPROVED` und `CAW_AUTO_DEPLOY` werden für den Workflow
nicht mehr benötigt. Eine aktiv konfigurierte Environment-Freigabe
(Required Reviewers) bleibt wirksam und wird nicht umgangen.

Nach bestätigter Push-CI baut er
`ghcr.io/0b-ivan/caw-server:sha-<commit>` und aktualisiert
ausschließlich das vorhandene Compose-Projekt `caw` über SSH.
Ein manueller Start ist über **Actions → Deploy CAW to Hetzner → Run
workflow** möglich, muss aber dieselbe erfolgreiche `main`-Push-CI
nachweisen.

**Einmalig auf dem Hetzner-Host:**

```sh
cd /opt/caw
git switch main
git pull --ff-only
# Es muss die bestehende Produktionskonfiguration geben:
test -f deploy/.env
test -f web/operator.json
python3 bin/hetzner-preflight.py
docker compose -p caw -f deploy/compose.yml -f deploy/compose.hetzner.yml ps
```

Der Checkout muss auf `main` ohne Änderungen an versionierten Dateien
liegen. Die ignorierten Dateien `deploy/.env` und `web/operator.json`
werden nicht verändert oder hochgeladen. Einen **eigenen Deploy-SSH-User**
mit Zugriff auf `/opt/caw` und die Docker-Engine einrichten. Docker-Zugriff
ist praktisch Root-Zugriff auf den Host: den Schlüssel nur für diese
Automatisierung verwenden und alle GitHub-Environment-Rechte beschränken.

**GitHub-Einstellungen:** Im Repository unter **Settings → Environments**
die Environment `production` anlegen. Darin als **Secrets** hinterlegen:

| Secret | Zweck |
|---|---|
| `CAW_SSH_HOST` | öffentliche DNS-Adresse oder IP des Hetzner-SSH-Servers |
| `CAW_SSH_USER` | dedizierter Deploy-Benutzer mit Docker-Rechten |
| `CAW_SSH_PRIVATE_KEY` | privater, passwortloser SSH-Deploy-Schlüssel |
| `CAW_SSH_KNOWN_HOSTS` | geprüfte SSH-`known_hosts`-Zeile für exakt diesen Host |

Die Host-Key-Zeile z. B. per `ssh-keyscan -p 22 -t ed25519 HOST`
erfassen, aber **Fingerabdruck unabhängig am Hetzner-Server** mit
`ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub` vergleichen.
Niemals `StrictHostKeyChecking=no` verwenden. Schlüssel und Passwörter
weder committen noch in Chat-/CI-Logs kopieren. Optional als
GitHub-**Environment-Variable** `CAW_SSH_PORT` setzen (sonst 22).
Ggf. im GHCR-Package `caw-server` den Zugriff der Repository-Actions
zulassen; es wird kein dauerhafter GHCR-Token auf Hetzner gespeichert.
Ein SSH-Deploy aus GitHub Actions setzt voraus, dass der konfigurierte
SSH-Port für GitHub-Runner erreichbar ist; **am Cloudflare-Tunnel
selbst wird nichts geändert**.

**Automatisch aktiv auf Betreiberanweisung vom 2026-11-01:**
Für den technischen Rollout sind keine zusätzlichen Repository-Variablen
mehr zu setzen. Der Workflow startet bei jedem Push auf `main`, wartet
auf dessen vollständige grüne Push-CI für die exakte SHA und verwendet
die bereits hinterlegten vier SSH-Secrets der Environment `production`.
Ein manuelles `workflow_dispatch` benötigt dieselbe CI-Abnahme.
Optionale **Required Reviewers** in der Environment bleiben wirksam.

**Risiko/noch nicht abgenommen:** Die bereits lokal wiederhergestellten
PostgreSQL-Dumps und Redis-RDB-Snapshots schützen nicht vor vollständigem
Hetzner-Hostverlust. Die auf Wunsch vorerst ausgesetzte verschlüsselte
externe Sicherung von PostgreSQL und Redis sowie die rechtliche Prüfung
der Betreiberangaben und Kartentexte sind weiterhin offen. Eine
technische Deployment-Aktivierung ist keine `v1.0.0`-Freigabe.

**Pipeline im Betrieb:**

1. `main` besteht sämtliche C++-, Web-, Last- und Compose-Tests.
2. GitHub erstellt und pusht das SHA-getaggte Container-Image nach GHCR.
3. Der SSH-Deploy prüft Commit/Checkout/Compose, erzeugt einen
   zusätzlichen lokalen Postgres-Dump und bewahrt die bisherige
   `CAW_VERSION` auf.
4. Nur `server` und `caddy` werden per
   `docker compose ... up --no-deps --no-build --wait` aktualisiert.
   Redis-/Postgres-Volumes, Backup-Dienst und der bestehende
   `cloudflared` werden **nicht** neu erstellt.
5. Lokales `/healthz`, öffentliches HTTPS und die
   `/metrics`-404-Sperre werden geprüft. Danach folgt ein echter
   WSS-Handshake von GitHub Actions aus.
6. Bei Fehler vor Abschluss der HTTP-Prüfungen rollt das Skript selbst
   zurück. Scheitert erst WSS, führt die CI
   `bin/rollback-hetzner` per SSH aus. Ein fehlgeschlagener Rollback
   bleibt in Actions sichtbar und erfordert einen Betreiber-Eingriff.

Die bisherige `CAW_VERSION` bleibt im lokalen `deploy/.env` erhalten
bzw. wird auf einen vollständigen SHA-Tag geändert. Dadurch verwenden
spätere manuelle `docker compose up`-Aufrufe nicht versehentlich
`:latest`. Das vorherige Image bleibt auf Hetzner für einen Rollback
lokal vorhanden. Docker-Images dürfen nicht durch automatische
Host-weite `docker image prune -a`-Jobs während eines Deployments
entfernt werden.

**Wichtig für die weitere Betriebsfreigabe:** Die Betreiberangaben,
Kartenrechte insbesondere Prior99 und verschlüsselte Offsite-Backups für
PostgreSQL **und Redis-AOF** mit Restore-Test bleiben offen. Die externe
HTTPS-/WSS-Abnahme und der GitHub-SSH-Zugriff wurden am 2026-11-01
erfolgreich getestet. Ein lokaler Dump vor Deploy ersetzt keine
Offsite-Sicherung; der jetzt aktive Auto-Deploy-Workflow bestätigt
keine rechtliche oder vollständige Disaster-Recovery-Freigabe.

## 8. Release-Kandidat bauen und GHCR-Zugriff prüfen – ohne Deployment

Der zusätzliche Workflow \`.github/workflows/release-candidate.yml\` beweist,
dass ein SHA-getaggtes CAW-Server-Image wirklich in GHCR veröffentlicht
werden kann. Er läuft beim erstmaligen Einchecken der Workflow-Datei auf
\`main\` und danach nur noch über \`workflow_dispatch\`. Vor dem Publish
prüft \`bin/verify-main-ci\` die erfolgreiche komplette Push-CI des
**exakten aktuellen \`main\`-Commits**. Das fertige Image erhält den Tag
\`ghcr.io/0b-ivan/caw-server:sha-<40-stelliger Git-SHA>\`. Besteht der
Tag bereits, veröffentlicht der Job ihn nicht erneut.

Er setzt weder \`CAW_PRODUCTION_APPROVED\` noch \`CAW_AUTO_DEPLOY\`, verwendet
**keine** SSH-Secrets und berührt weder Hetzner, PostgreSQL, Redis noch den
Cloudflare-Tunnel. Ein grüner Image-Publish ist ein vorbereitender Techniktest
und ausdrücklich **kein** Rollout oder Freigabeersatz.

Vor dem ersten echten Release weiterhin Betreiberangaben/Kartenrechte sowie
externe Backup-Wiederherstellung klären. Die lokal erfolgreich getesteten
PostgreSQL-Dumps und Redis-RDB-Snapshots schützen nicht vor Hostverlust.

## 9. Ohne Spielunterbrechung aktualisieren – Leerlauf-Gate

Der bestehende CAW-C++-Server läuft nur einmal. Ein Compose-Recreate trennt
alle offenen WebSockets; Redis-Resume stellt den Spielzustand wieder her,
macht den Neustart aber nicht transparent. Deshalb darf der automatische
Deploy **nur bei Leerlauf** die Instanz ersetzen.

- Der interne Metrik-Endpunkt wird im vorhandenen Server-Container gelesen.
  \`caw_sockets\` muss \`0\` sein; \`caw_active_games\` muss \`0\` sein.
  Playing, Judging und Reveal zählen auch dann als aktiv, wenn alle Spieler
  gerade offline sind. Lobbys oder beendete Partien ohne Clients blockieren
  hingegen nicht.
- Der alte Hetzner-Server hat \`caw_active_games\` noch nicht. Für dessen
  **erste** Aktualisierung werden stattdessen die Phasen aller
  Redis-Room-Snapshots intern geprüft. Lobbys (Phase 0) und beendete
  Partien (Phase 4) blockieren bei null WebSockets nicht. Laufende Partien
  (Phasen 1–3) blockieren auch bei unterbrochenen Verbindungen. Bei fehlenden
  Snapshots, abweichender Raumanzahl oder unlesbaren Daten wird nicht
  ausgerollt. Zugangstokens und private Karten werden niemals protokolliert.
- Vor jeder Änderung am laufenden Checkout wird zweimal kontrolliert,
  unmittelbar nach dem GHCR-Image-Pull noch einmal mit kurzer Stabilitätszeit.
  Bei belegter Instanz meldet GitHub Actions einen zurückgestellten Rollout
  und lässt alle Container und die getrackten Dateien unverändert.
- Ein wiederkehrender GitHub-Actions-Lauf alle 30 Minuten probiert den
  aktuellen, CI-geprüften \`main\`-Commit erneut. Ein vorhandener SHA-Tag
  wird nicht neu gebaut; bereits installierte Versionen werden nicht
  erneut gestartet. Erhaltene Environment-Freigaben bleiben wirksam.
- Bei einem unlesbaren internen Metrik-Endpunkt wird **nicht** deployt.
  Ein Ausfall von \`/healthz\` bzw. der öffentliche WSS-Test wird wie
  bisher als Deployment-Fehler behandelt.

Der erste Rollout am 2026-11-01 schlug vor einem Containerwechsel fehl,
weil der ältere Host-Checkout noch keine \`bin/deploy-hetzner\` besaß.
GitHub Actions liest das Skript jetzt direkt aus dem erfolgreich getesteten
SHA-Commit in eine temporäre Datei unter \`.git\` ein. Der bereits gesicherte
GitHub-Zugang dient dabei nur zum Lesen; der GHCR-Token bleibt auf stdin.

**Grenze:** Zwischen letzter Leerlaufkontrolle und Compose-Recreate kann
eine neue Session entstehen. Völlig unterbrechungsfreie Rolling Updates
unter beliebiger Last benötigen später kontrollierte Neuanmeldungs-Sperre
(Admission Drain) oder zwei Serverinstanzen mit Room-Affinität.

**Einmalige Betreiberfreigabe für 2026-11-01:** Die verifizierte
Legacy-Raumprüfung hat bei null WebSocket-Verbindungen genau eine als
unfertig markierte, aber laut Betreiber verlassene Partie gefunden.
Der bekannte Alt-Checkout `cc02372*` darf daher ausnahmsweise mit
dieser einen Session aktualisiert werden. Vor jeder Umschaltung bleibt
die Null-Socket-Prüfung verpflichtend, und die Redis-Snapshots bleiben
unverändert. Mehr als eine unfertige Session blockiert weiterhin.
Die Ausnahme greift nach dem Upgrade nicht noch einmal.

**Zusätzliche Einmalfreigabe am 02.11.2026:** Der produktive Checkout
`fc59685e124a00c4666c53947fd2e0f174848b30` hat bereits die Metrik
`caw_active_games`. Ihr Wert ist 2 bei 0 WebSockets. Der Betreiber bestätigt,
dass keine Partie gespielt wird. Nur auf exakt diesem Checkout und bei genau
zwei als aktiv gespeicherten Partien greift die zusätzliche Ausnahme im
**Metrik-Pfad**. Die frühere Ein-Partie-Ausnahme für `cc02372*` bleibt
unverändert. Vor jeder Prüfung muss der unabhängig laufende Backup-Worker
gesund sein; der jüngste Redis-Snapshot muss isoliert wiederherstellbar sein.
Alle gespeicherten Räume werden mit der aktuellen Metrik abgeglichen.
Kein Spielstand wird beendet oder gelöscht. Bei jeder Abweichung bleibt
der Wechsel gesperrt. Nach dem Fast-Forward greift die Ausnahme nicht erneut.

**Nachprüfung vom 02.11.2026:** Der erste Deploy mit der neuen Sicherung
lieferte einen erfolgreichen isolierten Redis-Restore mit 18 Schlüsseln,
blockierte aber wegen abweichender Zählung zwischen `caw_sessions` und
`session:*`. Die Ausnahme akzeptiert deshalb einen kleineren Redis-
Raumbestand ausschließlich dann, wenn die **beiden** vom Server als aktiv
gemeldeten Partien mit validem JSON und Phase 1–3 nachweislich in Redis
vorhanden sind. Fehlende aktive Spiele, zusätzliche Redis-Räume, ungültige
Snapshots oder abweichende Spielzahlen blockieren weiterhin. Es werden nur
anonyme Zähler protokolliert. Der Redis-Backup-Worker läuft unabhängig weiter.

## 10. Tägliche lokale Redis-Sicherung und Restore-Audit

`redis-backup` läuft als unabhängiger Container aus dem vorhandenen
`redis:7-alpine`-Image. Er verwendet ausschließlich das bereits bestehende
Compose-Volume `backups`, **nicht** das produktive `redisdata`-Volume.
Beim ersten Start und anschließend nach jedem erfolgreichen 24-Stunden-Zyklus
wird über `redis-cli --rdb` ein konsistenter Snapshot erzeugt. Nach
Fehlern folgt ein erneuter Versuch nach einer Stunde. Die veröffentlichten
`/backups/redis/caw-*.rdb` sind privat (Dateimodus 0600) und werden nach
14 Tagen entfernt. Das Redis-AOF bleibt unverändert aktiv.

Der automatische GitHub-Deploy installiert den Worker mit
`docker compose ... up --no-deps ... redis-backup`, **ohne** den
Redis-Server, PostgreSQL oder andere CAW-Dienste neu zu starten.
Ein fehlgeschlagener erster Backup-Start verhindert die Umschaltung
des App-Servers. Bei späteren Updates bleibt der Worker unverändert.

**Manuelle Prüfung auf Hetzner**, angemeldet als `caw-deploy`:

```sh
cd /opt/caw
env -u CAW_VERSION docker compose --env-file deploy/.env \
  -p caw -f deploy/compose.yml -f deploy/compose.hetzner.yml \
  ps redis-backup

bash bin/verify-local-redis-restore
```

Der Restore-Test kopiert den jüngsten RDB-Snapshot in ein temporäres
Verzeichnis und startet Redis mit `--network none`, einer
nur-lesbaren RDB-Datei und ausschließlich einem Unix-Socket.
Er meldet nur die Anzahl wiederhergestellter Keys, **keine**
privaten Sessioninhalte. In der CI wird zusätzlich ein synthetischer
Schlüssel nachweislich wiederhergestellt.

Der Workflow `.github/workflows/redis-backup-audit.yml` führt um
07:17 UTC täglich auf Hetzner einen Frische- und Restore-Test aus.
Scheitert einer dieser Schritte, ist der GitHub-Actions-Lauf rot.
Für tatsächliche Benachrichtigungen müssen die GitHub-Actions-
Fehlerbenachrichtigungen im Benutzerkonto aktiviert sein; ein
externer Alarmkanal ist noch nicht eingerichtet.

**Wichtig:** Die Sicherungen befinden sich auf **demselben Host**.
Sie schützen weder vor kompletter Hetzner-Hostzerstörung noch vor
Verlust des Backup-Volumes. Verschlüsselte Offsite-Backups mit
getrennter Restore-Probe bleiben ausstehend (Storage Box vorerst
vom Betreiber zurückgestellt).

### Backup-Worker auch bei noch gespeicherten aktiven Partien aktivieren

Seit 2026-11-02 gibt es einen separaten Bootstrap-Schritt **vor** dem
Game-Idle-Gate im GitHub-Workflow `deploy-hetzner.yml`. Er richtet
ausschließlich den Redis-Backup-Worker ein; **Server, Caddy, Redis,
PostgreSQL, Spielräume und Tunnel werden nicht neu gestartet**.

Liegt der produktive Checkout wegen alter Sessions noch auf einer Version
ohne `redis-backup`-Definition, liest der Bootstrap das geprüfte
`deploy/redis-backup.sh` aus dem aktuellen `main`-Commit und speichert
es privat unter `.git/caw-redis-backup.sh`. Nur für den Backup-Worker
wird zusätzlich `.git/caw-redis-backup-bootstrap.yml` verwendet.
Das bestehende Compose-Projekt `caw`, sein Redis-Netz und sein
`backups`-Volume werden wiederverwendet; ein vollständiges
`docker compose up` ist dafür **nicht nötig**. Das Setup ist
idempotent und durch die normale Deploy-`flock`-Sperre serialisiert.

Nach dem Start muss der Worker *healthy* werden. Ein
`verify-local-redis-restore`-Lauf aus dem CI-geprüften Git-Commit
lädt den neuesten Snapshot isoliert; bei Fehlern wird der GitHub-Job
rot. Der tägliche Audit findet den Worker über die
Compose-Container-Labels und prüft den Restore auch dann, wenn die
Spielserver-Version noch zurückgestellt ist.

Der restliche Auto-Deploy führt unverändert die Leerlaufprüfung aus
und darf weiterhin **keine laufende Partie unterbrechen**. Sobald
alle Spielräume tatsächlich beendet sind, wechselt das reguläre
Deployment bei Bedarf auf die versionierte Backup-Compose-Definition.
Externe Backups fehlen weiterhin.

## 11. Deploy-Status korrekt lesen (03.11.2026)

Nach dem erfolgreichen Erst-Rollout am 02.11.2026 blieb Release
`f01d90c7` zurückgestellt: mehrere planmäßige Prüfungen meldeten
`1 active matches, 0 sockets` (zuletzt Run
[41560312537](https://github.com/0b-ivan/CAW/actions/runs/41560312537)).
Das Workflow-Ergebnis `success` bedeutete in diesen Fällen lediglich,
dass der **Leerlaufschutz korrekt eingegriffen hatte**, nicht dass der
Commit bereits auf dem Server lief.

Der Workflow schreibt daher eine eindeutig bezeichnete Zusammenfassung:

- **CAW: DEPLOYED** – exakt dieser SHA-Release wurde installiert. Nur
  dann ist bei fehlerhafter anschließender WSS-Abnahme ein Rollback zulässig.
- **CAW: UNCHANGED** – der gewünschte oder ein neuerer Release war schon
  installiert. Die öffentliche WSS-Prüfung bleibt aktiv; kein automatischer
  Rollback auf einen älteren Stand nach einem bloßen No-op.
- **CAW: NOT DEPLOYED (deferred)** – eine Partie bzw. ein Socket blockiert.
  Der Release steht weiterhin aus. Der Workflow prüft ausschließlich
  die öffentliche HTTPS-Readiness und die `/metrics`-Sperre, **ohne
  testweise einen Spielraum anzulegen**. Die nächste geplante Prüfung
  erfolgt ungefähr 30 Minuten später.

Im letzten Fall ist der erfolgreiche Actions-Lauf ein erfolgreicher
**Schutz-/Erreichbarkeitstest**, keine Produktionsversionsbestätigung.
Bei einer nicht beendeten, aber getrennten Partie verlängert allein
das Verstreichen der Zeit die Sitzung nicht: standardmäßig läuft die
Lebensdauer nach 24 Stunden ohne Aktivität aus
(`SESSION_TTL_HOURS`, gegebenenfalls auf dem Host abweichend konfiguriert).
Es werden keine Partien zur Freigabe des Deployments gelöscht oder
zwangsweise beendet.

Eine einmalige historische Sonderfreigabe für die alten Checkouts
`cc02372*` und `fc59685*` wurde beim erfolgreichen Release
`dcb97c4c` abgeschlossen und anschließend in PR #35 aus dem
Deploy-Code entfernt. Die damaligen Beschreibungen in Abschnitt 9 sind
ausschließlich als Verlauf zu verstehen, nicht als aktuelle
Handlungsanweisung. Nachweis:
[docs/validation/PRODUCTION-2026-11-02.md](../validation/PRODUCTION-2026-11-02.md).

## 12. Kein False Positive mehr bei gespeicherten Offline-Partien (03.11.2026)

Die ältere Metrik `caw_active_games` zählt unfertige Spielphasen auch
dann, wenn **niemand mehr verbunden ist**. Das führte in den Runs
[41560312537](https://github.com/0b-ivan/CAW/actions/runs/41560312537)
und [41563947097](https://github.com/0b-ivan/CAW/actions/runs/41563947097)
zurückgestellten Releases bei genau einer unfertigen Partie und
`caw_sockets=0`. Der Betreiber bestätigte ausdrücklich, dass kein
Spiel mehr stattfindet. Die Metrik war korrekt für den gespeicherten
Spielstatus, aber kein hinreichender Indikator für aktive Spieler.

**Neue Deploy-Regel:** Offene WebSockets blockieren ausnahmslos. Bei
null Sockets darf ein unfertiges Spiel einen sicheren Neustart
überstehen, **wenn alle unfertigen Spielräume nachweislich in Redis
gespeichert sind**. Der Deploy prüft hierzu nur anonyme Room-Anzahlen
und Phasen aller `session:*`-Snapshots, den gesunden Backup-Worker,
einen erfolgreichen netzwerkisolierten RDB-Restore und einen stabilen
`caw_actions_total` über drei Kontrollen mit mindestens 60 Sekunden
Ruhezeit. Ungültige, fehlende oder zusätzliche aktive Redis-Räume,
offene WebSockets oder neue Spieleraktionen führen weiterhin zur
Zurückstellung bzw. zu einem sicheren Abbruch.

Die laufenden Container und Redis-Schlüssel werden **nicht** vor
diesen Prüfungen verändert. Danach beendet nur Compose den einzelnen
C++-Spielserver; dessen normaler Shutdown leert die Snapshot-Queue.
Die Daten bleiben in Redis und können über vorhandene Tickets wieder
aufgenommen werden. Neue Zimmer müssen nicht angelegt und alte Spiele
nicht gelöscht werden. Vollständig unterbrechungsfreies Deployment
während aktiver Verbindungen benötigt weiterhin Admission-Drain oder
mehrere Instanzen.

Die Ausnahme für die alten `cc02372*`-/`fc59685*`-Checkouts bleibt
entfernt. Das hier ist eine wiederverwendbare, datenbewahrende Regel
für gespeicherte und momentan nicht verbundene Partien, kein
betreiberabhängiger Force-Rollout.

## 13. Zustandsfreie öffentliche WSS-Prüfung bei Wiederholungs-Deploys (03.11.2026)

Die automatischen Wiederholungen von Release `05986a4d` waren stabil,
führten aber alle 30 Minuten die vollständige
`bin/hetzner-wss-check.mjs` aus. Dieses Skript legt per
`POST /api/sessions` jeweils eine gültige Test-Lobby an.
Bei 48 Prüfungen pro Tag entstehen so zahlreiche zusätzliche
Redis-Keys und bis zum TTL-Ablauf belegte Spielräume, obwohl
kein Release gewechselt wurde.

Ab jetzt gilt:

- **Neu installiert (`DEPLOYED`):** Weiterhin vollständiger
  Cloudflare-HTTPS-/WSS-Host- und State-Handshake samt einmaliger
  Test-Session, um Spielprotokoll, Authentifizierung und möglichen
  Rollback nach einer echten Installation abzudecken.
- **Bereits aktuell (`UNCHANGED`):** Neue WebSocket-Probe über
  `wss://caw.obivan.org/ws/probe` mit fester JSON-Antwort
  `{"type":"probe","ok":true}`. Die Prüfung enthält außerdem
  `/healthz`; sie erzeugt **keine** Room-IDs, Host-Tokens,
  Redis-Snapshots oder Postgres-Einträge.
- **Aufgeschoben (`NOT DEPLOYED`):** Ausschließlich die bereits
  etablierte öffentliche HTTPS-Readiness. Diese funktioniert auch,
  wenn der alte Server `/ws/probe` noch nicht implementiert hat.

Die Probe ist rate-limitiert und verarbeitet keine Spieleingaben.
Bei Fehlern im unveränderten System wird der Workflow rot, aber ein
Rollback wird **nicht** ausgelöst. Vor dem Merge prüft die CI einen
echten WebSocket-Upgrade durch den lokalen Caddy-Reverse-Proxy und
vergleicht `caw_sessions` vor und nach der Probe.

Alle vor der Umstellung angelegten Test-Lobbys werden über den
normalen Session-TTL-Ablauf bereinigt. Es sind **keine**
Löschaktionen gegen produktives Redis notwendig.
