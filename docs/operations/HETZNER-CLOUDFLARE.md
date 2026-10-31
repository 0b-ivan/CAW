# CAW auf Hetzner Cloud über Cloudflare Tunnel

Stand: 2026-10-31. **Vorbereitung**, noch kein produktives Deployment.
Produktionsadresse: `https://caw.obivan.org`.

## Aufbau

```
Handy/TV --HTTPS/WSS--> Cloudflare Edge
                              |
                        Tunnel (verschlüsselt, ausgehend)
                              |
                     cloudflared (Docker)
                              |
                        Caddy :80 (Docker-Netz)
                         |           |
                    web/ statisch   C++-Server :8080
                                      |       |
                                    Redis  Postgres
                                             |
                                       pg_dump (14 Tage lokal)
```

Kein A-/AAAA-Eintrag zur Hetzner-Origin-IP. Keine öffentlichen Ports
80/443 auf dem Hetzner-Server. Einzig SSH TCP 22 ist administrativ
erreichbar und **auf die eigene Admin-IP begrenzt**. DNS und TLS verwaltet
Cloudflare, nicht der Caddy-Container. Cloudflare Tunnel benötigt ausgehend
TCP/UDP **7844**, außerdem DNS und Updates über ausgehend 53/443.
[Cloudflare: Firewall](https://developers.cloudflare.com/cloudflare-one/networks/connectors/cloudflare-tunnel/configure-tunnels/tunnel-with-firewall/).

## 1. Hetzner-Server anlegen

1. Hetzner Cloud: neue Ubuntu-**24.04 LTS**-VM in Deutschland anlegen.
   Für ein einzelnes kleines Spiel reicht ein Einstieg mit 2 vCPU / 4 GB RAM,
   mit Reserve empfehlenswert 4 vCPU / 8 GB RAM. Speicher/Traffic später
   anhand der Lastmessungen wählen; das ist keine Kapazitätsgarantie.
2. Eigenen SSH-*Public*-Key bei Erstellung hinterlegen, keine Passwörter,
   keinen privaten Schlüssel in das Repo.
3. Cloud Firewall **Inbound** nur TCP 22 von deiner Admin-CIDR (v4/v6),
   keine 80/443/8080/5432/6379/9090. Egress nach Cloudflare TCP/UDP 7844
   plus DNS, HTTPS und Paketupdates erlauben. Ohne feste Admin-IP sicheren
   VPN-/Tailscale-/Headscale-Zugang für SSH vorsehen, nicht 22 pauschal öffnen.
4. Erst nach erfolgreicher SSH-Key-Anmeldung Passwort-Login und
   Root-Passwort-Login deaktivieren. Danach Updates und Docker Engine +
   Compose-Plugin gemäß [offizieller Ubuntu-Anleitung](https://docs.docker.com/engine/install/ubuntu/)
   installieren. Prüfen mit `docker version` und `docker compose version`.
5. Ein normaler Wartungs-User führt die Befehle aus. Docker-Gruppenmitglieder
   besitzen praktisch Root-Rechte; nur vertrauenswürdige Admins hinzufügen.

Bei einer Firewall mit **restriktivem Egress** müssen sowohl 7844/UDP
(QUIC) als auch 7844/TCP (HTTP/2-Fallback) erreichbar sein. Cloudflare
beschreibt die benötigten Zieladressen in der verlinkten Anleitung.

## 2. Projekt und lokale Geheimnisse

```sh
sudo mkdir -p /opt/caw
sudo chown "$USER":"$(id -gn)" /opt/caw
git clone https://github.com/0b-ivan/CAW.git /opt/caw
cd /opt/caw
# Solange die Hetzner-PR offen ist, deren Feature-Branch testen:
git switch feat/hetzner-deployment-prep-20261031
# Nach dem Merge: git switch main && git pull --ff-only
cp deploy/hetzner/env.example deploy/.env
openssl rand -hex 32
```

Den generierten Wert in `deploy/.env` als `POSTGRES_PASSWORD` eintragen,
nicht in einem Chat oder Commit veröffentlichen. Die Vorlage setzt bereits
`PUBLIC_URL=https://caw.obivan.org` und `CADDY_ADDRESS=:80`.
`chmod 600 deploy/.env` ausführen.

Die CSV-Sets sind im Image. **Nur nichtkommerziell** betreiben und
Rechte/Attribution vor einer öffentlichen Veröffentlichung freigeben.

```sh
mkdir -p deploy/secrets
chmod 700 deploy/secrets
# In Cloudflare den Token für den CAW-Tunnel kopieren.
# Paste NUR den Token in die Datei, ohne "cloudflared tunnel run"-Befehle:
nano deploy/secrets/cloudflared-token
chmod 600 deploy/secrets/cloudflared-token
```

`deploy/secrets/`, `deploy/.env` und `web/operator.json` sind
gitignored. Das Tunnel-Token wird als **Compose Secret / token-file** statt
als Umgebungsvariable oder Kommandozeilenargument übergeben.

Für den öffentlichen Betrieb muss auch das Impressum vorhanden sein.
**Keine erfundenen Angaben:**

```sh
cp web/operator.example.json web/operator.json
# Vor Livebetrieb Betreibername, ladungsfähige Anschrift und E-Mail eintragen
nano web/operator.json
```

## 3. Cloudflare-Tunnel einrichten

Cloudflare-Dashboard → **Networking → Tunnels**:
- Empfehlung: eigener, remote-managed Tunnel `caw-hetzner`. Damit geraten
  alte Connector-Replikate auf anderen Servern nicht versehentlich in den
  Traffic-Pfad.
- **Published application**: Hostname `caw.obivan.org`, Service **HTTP**,
  URL **`http://caddy:80`**. Die Adresse `caddy` wird **innerhalb**
  des Docker-Netzes von `cloudflared` aufgelöst.
- Die Cloudflare-DNS-Verknüpfung dem Tunnel zuordnen; keinen A-/AAAA-Record
  mit der Hetzner-IP als alternativen Origin anlegen.
- Cloudflare **WebSockets aktiv**, keine Access-Login-Sperre für die
  öffentliche Spielseite und `/ws/*`. Keine Caching-Regel für `/api/*`
  oder `/ws/*`. Schutzregeln dürfen echte anonyme Browser nicht blockieren.
- Wenn schon ein Tunnel **auf derselben Hetzner-VM** betrieben wird:
  statt eines zweiten Containers entweder denselben Connector im Compose-
  Netz integrieren oder den Host-Connector auf einen nur lokal gebundenen
  Port konfigurieren. **Nicht** einfach den existierenden Tunnel-Token mit
  alten Replikaten auf anderen Servern duplizieren.

Cloudflare unterstützt WebSockets und leitet HTTPS zum internen HTTP-Origin
weiter. [Tunnel-Routing](https://developers.cloudflare.com/tunnel/get-started/)
und [WebSockets](https://developers.cloudflare.com/network/websockets/).

## 4. Verbindliche Compose-Dateien und Start

Die Basis-Compose-Datei allein veröffentlicht Entwicklungsports. **Auf
Hetzner immer beide Dateien zusammen angeben.** Das Override setzt
`ports: !reset []` für Caddy. Docker Compose v2 mit Unterstützung für
`!reset` ist erforderlich.

```sh
cd /opt/caw
python3 bin/hetzner-preflight.py
docker compose -f deploy/compose.yml -f deploy/compose.hetzner.yml \
  config --format json >/dev/null
docker compose -f deploy/compose.yml -f deploy/compose.hetzner.yml \
  up --build -d --wait
docker compose -f deploy/compose.yml -f deploy/compose.hetzner.yml ps
docker compose -f deploy/compose.yml -f deploy/compose.hetzner.yml \
  logs --tail=80 cloudflared
```

`cloudflared` zeigt im Dashboard **Healthy/Connected**. Docker `ps`
darf **keine veröffentlichten Caddy-Ports** ausweisen. Prometheus ist
optional über das Compose-Profil nur auf `127.0.0.1:9090` erreichbar;
`/metrics` ist über den Tunnel **immer 404**. Interne Prometheus-Scrapes
gehen unmittelbar an `server:8080`.

## 5. Abnahme nach Verbindung

```sh
python3 bin/hetzner-preflight.py --public
node bin/hetzner-wss-check.mjs
curl -fsS https://caw.obivan.org/healthz
curl -s -o /dev/null -w '%{http_code}\n' \
  https://caw.obivan.org/metrics   # muss 404 sein
```

Der Node-Check legt einen **temporären Spielraum** an und testet einen
echten Host-WebSocket über `wss://`; Tokens werden nicht ausgegeben.
Anschließend mit mindestens drei echten Geräten eine komplette Partie
prüfen, inklusive Big Screen, Reconnect, Neustart und Attribution.
`./bin/smoke` **niemals auf dem produktiven Server ausführen**: Das
Skript stoppt absichtlich Redis und PostgreSQL.

Cloudflare ist am Edge für das Browser-Zertifikat zuständig.
`Caddyfile.hetzner` hört ausdrücklich nur auf dem internen HTTP-Port;
ein fehlendes Caddy-Origin-Zertifikat ist in dieser Architektur kein
Fehler. Browser muss allerdings ein gültiges HTTPS-/WSS-Zertifikat sehen.

## 6. Backup, Monitoring, Updates, Wiederherstellung

- Postgres: vorhandener `backup`-Dienst schreibt täglich `pg_dump`
  (14 Tage) in ein lokales Docker-Volume. Redis: AOF in `redisdata`.
  **Ein Hetzner-Serverausfall kann beide Volumes vernichten.**
- Vor Produktivfreigabe **verschlüsseltes Offsite-Backup** für Dumps
  und Redis-AOF einrichten, z.B. in einem externen Object-Storage mit
  Zugriffsschlüssel, Aufbewahrung und dokumentiertem Restore.
  Ein Hetzner-Server-Backup/Snapshot kann ergänzen, ersetzt die
  Datenbank-/Redis-Sicherung nicht. Destination und Schlüssel sind noch
  nicht festgelegt: dies ist ein **Go-live-Blocker**.
- Für einen manuellen lokalen Test:
  `docker compose -f deploy/compose.yml exec -T backup /bin/sh /backup.sh once`,
  den Dump im `backups`-Volume kontrollieren, dann
  `bin/restore <dump-name> caw_check` auf eine getrennte Prüfdatenbank.
  Keine produktive Datenbank ungeprüft überschreiben.
- Optional Monitoring:
  `docker compose -f deploy/compose.yml -f deploy/compose.hetzner.yml
  --profile monitoring up -d`, lokal nur Port 9090 auf Loopback.
- Bei einem Update zuerst Datenbank/Redis sichern und **keine**
  `docker compose down --volumes` ausführen. Rollback mit vorherigem
  Git-Commit bzw. später einem geprüften GHCR-Tag, dann Restore nur wenn
  nötig und kompatibel. `v1.0.0` erst nach Live-Abnahme.

## Noch manuell zu erledigen

- Hetzner-VM anlegen, SSH-Key, Admin-CIDR und Firewall konfigurieren.
- Cloudflare-Tunnel (Token) sowie DNS/Hostname verbinden.
- Betreiberangaben außerhalb von Git ergänzen.
- Extern verschlüsselte Datensicherung inkl. Redis und Restore-Probe.
- Cloudflare-Zertifikat und WSS auf der Live-Domain prüfen.
- Lizenz-/Betreiberfreigabe für öffentlichen, nichtkommerziellen Betrieb.

**Nichts davon wird von dieser PR vorgetäuscht oder automatisch ausgelöst.**
