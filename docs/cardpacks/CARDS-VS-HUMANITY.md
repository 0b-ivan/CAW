# Cards vs Humanity – externe Kartensets

Quelle: https://www.cards-vs-humanity.de/set/list/

Die Webseite veröffentlicht sechs Sets als CSV. Die Metaangaben und unveränderten
Download-URLs liegen in `docs/cardpacks/cvh_sources.json`. Dort sind **keine
Kartentexte** gespeichert.

| Set | Schwarze Karten | Weiße Karten |
|---|---:|---:|
| Basisspiel 1 – Minions mit Genitalherpes | 45 | 216 |
| Basisspiel 2 – Heidi Klums Hungersnot | 45 | 216 |
| Erweiterung 1 – Wir werden bösartig | 18 | 153 |
| Erweiterung 2 – Black Cards Matter | 144 | 0 |
| Fan-Erweiterung 1 – That's what they said | 45 | 180 |
| Fan-Erweiterung 2 – Deine Mudda! | 288 | 414 |
| **Insgesamt** | **585** | **1179** |

## Urheberrecht / Lizenz

Quelle und Herausgeber der Fan-Übersetzung: Cards vs Humanity,
https://www.cards-vs-humanity.de/ (enthält Hinweise zu Original und
abgeleiteten Karten). Die Seite nennt **Creative Commons BY-NC-SA 2.0**:
https://creativecommons.org/licenses/by-nc-sa/2.0/deed.de

Vor einer öffentlichen Bereitstellung oder Veränderung der Kartentexte
Urheberschaft, Namensnennung, nichtkommerzielle Nutzung, ShareAlike und
mögliche Rechte weiterer Beiträger konkret prüfen. Insbesondere macht ein
kostenloser Download die Dateien nicht gemeinfrei.

## Download – nur lokale Originaldateien

Im Projektverzeichnis:

```sh
python3 bin/fetch-cvh-sets.py --list
python3 bin/fetch-cvh-sets.py
```

Der Downloader holt die sechs **unveränderten CSV-Dateien** in das durch
`.gitignore` ausgeschlossene Verzeichnis
`local-cardpacks/cards-vs-humanity/`. Mit `--set base-1` lässt sich ein
einzelnes Set abrufen; `--force` überschreibt vorhandene Dateien.

**Aktuelle Grenze:** Die CSV-Dateien sind externe Quelldaten, noch keine
spielbaren CAW-Packs. Erst M5 definiert und implementiert die validierte
CSV-Konvertierung zu unserem Pack-Format, Import nach PostgreSQL und
Lizenzanzeige im Spiel. Dieses Downloadwerkzeug macht keinerlei Änderungen
an Datenbank, Server oder Spielregeln. Die hier dokumentierten Zahlen
stammen von der Quellseite und wurden noch nicht gegen die CSV-Inhalte
abgeglichen; das Datenformat konnte zum Zeitpunkt der Einbindung nicht
direkt eingesehen werden.
