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
