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
