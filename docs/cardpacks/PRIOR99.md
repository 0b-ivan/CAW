# Prior99 – CAH Fullformat German

Quelle: https://github.com/Prior99/cah-fullformat-german  
Importierter Quell-Commit: \`92501b245f7002ac3d3875099d859a5579aa2161\`  
Stand: 2026-11-01

## Import in CAW

Die 717 Karten liegen als **ein auswählbares Deck** unter
\`assets/cardpacks/prior99-fullformat-german/\`:

- \`cards.csv\`: 107 schwarze Karten, 610 weiße Karten
- \`packs.json\`: Titel, Sprache, Lizenzhinweise, Urheber, Quell-Commit,
  \`defaultSelected: false\`
- \`ATTRIBUTION.md\`: Ursprung, Bearbeitungsschritte, Lizenzkonflikt

Der CAW-Server lädt den Pack beim Start automatisch aus \`PACKS_DIR\`.
Im Spiel kann der Host ihn ausdrücklich im Pack-Menü auswählen und mit
anderen Packs mischen; er wird bei neuen Sessions **nicht standardmäßig**
aktiviert. Damit werden die enthaltenen Erwachsenen-/Schwarzhumor-Karten
nicht unbemerkt Teil des Basisspiels.

Der Release-Docker-Build enthält diesen Pack, sobald der PR gemergt
und das Server-Image neu gebaut/veröffentlicht wurde. Ein Merge oder
Deployment ist durch diesen PR allein nicht erfolgt.

## Bestandsaufnahme und Validierung

| Quellkategorie | Schwarz | Weiß |
|---|---:|---:|
| Basis | 59 | 299 |
| Böse | 4 | 44 |
| Drogen | 2 | 6 |
| Eklig | 1 | 22 |
| FSK 16 | 4 | 55 |
| FSK 18 | 2 | 27 |
| Geschichte | 3 | 26 |
| Kultur | 16 | 52 |
| Politik | 5 | 27 |
| Religion | 4 | 23 |
| Werbung | 7 | 29 |
| **Gesamt** | **107** | **610** |

Die Kategorien wurden im CSV zu einem Paket zusammengeführt;
die einzelnen Kategorie-Tags sind **nicht** separat im CAW-Format
erhalten. 92 Fragen erfordern Pick 1 (davon 37 ohne explizite
Leerstelle), 13 Pick 2 und zwei Pick 3. Der Konverter erhält diesen
Spielablauf. Keine Karte überschreitet 300 UTF-8-Bytes.
Innerhalb des Prior99-Sets sind keine identischen Texte enthalten.

Mit den sechs bereits vorhandenen Cards-vs-Humanity-Packs sind bei
normalisiertem Textvergleich elf schwarze und zwei weiße Texte
identisch. Der CAW-Deck-Builder entfernt **wortgleiche** Dopplungen
automatisch, nicht jedoch semantisch ähnliche Karten.

**Inhalt:** Das Set enthält teils explizite, diskriminierende,
historisch überholte oder anderweitig beleidigende Aussagen.
Eine redaktionelle Prüfung und ggf. Inhaltsfilterung sind offen.
Die Kategorien „FSK 16“/„FSK 18“ sind Quelllabels und keine amtlichen
Altersfreigaben. Deshalb gilt der gesamte Pack als Opt-in für Erwachsene.

## Urheberrecht – noch nicht abschließend geprüft

Prior99 bezeichnet seine LaTeX-Dateien als GPL-3.0-or-later und behauptet
im README fälschlich, das ursprüngliche Cards Against Humanity sei GPL.
Das offiziell frei erhältliche Original steht unter
[CC BY-NC-SA 2.0](https://creativecommons.org/licenses/by-nc-sa/2.0/);
die Bedingungen umfassen Namensnennung, Nichtkommerzialität und
Weitergabe von Bearbeitungen unter gleichen Bedingungen.

Die Rechte der einzelnen deutschen Übersetzer und Fanbeiträge sowie
die Lizenzkompatibilität sind **nicht abschließend geklärt**.
Die Angabe im Manifest nennt beide Quellenhinweise und ist
**keine rechtliche Freigabe**. Der Nutzer hat den Import trotz dieser
offenen Fragen ausdrücklich verlangt. Vor öffentlichem Betrieb und
kommerzieller Nutzung muss die Rechtefrage geprüft werden.
Die MIT-Lizenz von CAW erstreckt sich nicht auf die Kartentexte.

Referenzen:
- https://github.com/Prior99/cah-fullformat-german/blob/master/README.md
- https://s3.amazonaws.com/cah/CAH_MainGame.pdf
- https://creativecommons.org/licenses/by-nc-sa/2.0/

## Wiederholbarer Import

Das vorhandene Skript \`bin/convert-prior99-cards.py\` unterstützt
eine lokale, gegen den Quell-Commit geprüfte Konvertierung.
Ohne \`--rights-confirmed\` führt es lediglich die Bestandsprüfung aus.
Es lädt nicht selbst Kartendaten nach; die ursprünglichen LaTeX-Dateien
werden nicht zusätzlich in CAW gespeichert.

Prüfungen:
\`python3 -m unittest discover -s tests -p 'test_vendored_prior99_pack.py'\`
\`python3 -m unittest discover -s tests -p 'test_prior99_importer.py'\`
und die vollständige C++-CI mit API-Pack-Check.
