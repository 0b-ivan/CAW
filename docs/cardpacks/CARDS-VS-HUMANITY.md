# Cards vs Humanity – versionierte Original-Kartensets

Quelle: https://www.cards-vs-humanity.de/set/list/

Die Webseite veröffentlicht sechs Sets als CSV. Die Originaldateien liegen
unverändert unter `assets/cardpacks/cards-vs-humanity/` **einschließlich**
`ATTRIBUTION.md` und `SHA256SUMS`. Die Metaangaben und Download-URLs stehen
zusätzlich in `docs/cardpacks/cvh_sources.json`.

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

Die Website erklärt das Teilen und die Bearbeitung der Originalkarten unter
dieser Lizenz ausdrücklich für zulässig. Unser Repo dokumentiert daher
Namensnennung, URLs, Lizenzhinweise, Unverändertheit und SHA-256-Prüfsummen.
Bei öffentlicher Nutzung bleiben die Bedingungen Nichtkommerzialität,
Weitergabe unter gleichen Bedingungen und mögliche Rechte weiterer
Fanbeiträge zwingend zu beachten; es handelt sich **nicht** um CC0/MIT.

## Optional: Quellen erneut lokal herunterladen

Im Projektverzeichnis:

```sh
python3 bin/fetch-cvh-sets.py --list
python3 bin/fetch-cvh-sets.py
```

Der Downloader holt die sechs **unveränderten CSV-Dateien** in das durch
`.gitignore` ausgeschlossene Verzeichnis
`local-cardpacks/cards-vs-humanity/`. Mit `--set base-1` lässt sich ein
einzelnes Set abrufen; `--force` überschreibt vorhandene Dateien.

**Aktuelle Grenze:** Die versionierten CSV-Dateien sind Quelldaten, noch keine
spielbaren CAW-Packs. Erst M5 definiert und implementiert die validierte
CSV-Konvertierung zu unserem Pack-Format, Import nach PostgreSQL und
Lizenzanzeige im Spiel. Dieses Downloadwerkzeug macht keinerlei Änderungen
an Datenbank, Server oder Spielregeln. Die hier dokumentierten Zahlen
stammen von der Quellseite und wurden am 31.10.2026 gegen die sechs
Original-CSV-Dateien abgeglichen: Header `typ,text`, `b` für schwarze
und `w` für weiße Karten; alle **1.764** Datenzeilen stimmen mit den
angegebenen Anzahlen überein. Die Platzhalterkonventionen und die
Rechte einzelner Fanbeiträge müssen vor dem Import noch geprüft werden.
