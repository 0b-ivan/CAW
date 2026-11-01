# Prior99 – CAH Fullformat German: Importprüfung

Quelle: https://github.com/Prior99/cah-fullformat-german
Geprüfter Quell-Commit: 92501b245f7002ac3d3875099d859a5579aa2161
Stand: 2026-11-01. **Nicht für öffentliche Weiterverbreitung freigegeben.**

## Technische Bestandsaufnahme

| Kategorie | Schwarz | Weiß |
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

Pick-Werte: 92 × Pick 1, 13 × Pick 2, 2 × Pick 3.
37 Pick-1-Fragen enthalten keine Leerstelle; CAW ergänzt die Antwort
wie vorgesehen am Satzende. Keine Karte überschreitet 300 UTF-8-Bytes.
Innerhalb der Quelle wurden keine identischen Karten festgestellt.
Gegenüber den sechs bestehenden Cards-vs-Humanity-Packs wurden
11 identische schwarze und 2 identische weiße Karten gefunden.
CAW entfernt identische Karten beim Mischen der Decks.

Der Konverter erstellt ein einziges optionales Deck mit allen elf
Kategorien. Es enthält auch ausdrücklich für Erwachsene vorgesehene,
teils veraltete und potentiell diskriminierende Inhalte. Die
Quell-Kategoriebezeichnungen sind keine verbindliche Altersfreigabe.

## Rechteprüfung: vorläufig gesperrt

README und TeX-Dateien von Prior99 bezeichnen das Material als GPL
(v3 oder später) und behaupten, bereits das Originalspiel sei GPL.
Die offiziell frei erhältlichen Cards-Against-Humanity-Originalkarten
stehen jedoch unter CC BY-NC-SA 2.0, einschließlich Namensnennung,
Nichtkommerzialität und ShareAlike. Ein GPL-Vermerk des Forks kann
die Rechte an der Vorlage nicht eigenmächtig neu lizenzieren.
Die Rechte der Übersetzer und weiterer Fanbeiträge sind ebenfalls
nicht vollständig belegt.

- Original-Lizenz: https://s3.amazonaws.com/cah/CAH_MainGame.pdf
- Lizenzbedingungen: https://creativecommons.org/licenses/by-nc-sa/2.0/
- Prior99-README: https://github.com/Prior99/cah-fullformat-german/blob/master/README.md

Vor öffentlichem Import, Release-Image oder Betrieb müssen Herkunft,
erforderliche Zustimmungen, Rechteinhaber und tatsächlich anwendbare
Lizenz geklärt und dokumentiert werden. Bis dahin **keine Prior99-
Kartentexte in das CAW-Repository oder das öffentliche Docker-Image**.
Die MIT-Lizenz des Anwendungscodes gilt nicht für Kartentexte.

## Lokaler Konverter nach Rechteklärung

Das Werkzeug lädt selbst nichts herunter. Es akzeptiert nur einen sauberen
Git-Checkout des oben genannten Commits. Das lokale Zielverzeichnis
local-cardpacks/ steht in .gitignore.

    git clone https://github.com/Prior99/cah-fullformat-german.git local-cardpacks/upstream-prior99
    git -C local-cardpacks/upstream-prior99 checkout 92501b245f7002ac3d3875099d859a5579aa2161
    python3 bin/convert-prior99-cards.py --source local-cardpacks/upstream-prior99

Ohne zusätzliche Parameter wird **nur geprüft** und nichts geschrieben.
Nach tatsächlicher Rechteklärung können Lizenz und Rechteinhaber
explizit angegeben werden:

    python3 bin/convert-prior99-cards.py \
      --source local-cardpacks/upstream-prior99 \
      --rights-confirmed \
      --license 'VERIFIZIERTE_LIZENZ' \
      --rights-holder 'VERIFIZIERTE_RECHTEINHABER'

Die Ausgabe steht unter local-cardpacks/prior99-fullformat-german/
(cards.csv und packs.json). Nach Freigabe lässt sich genau dieser
Ordner als /app/packs/prior99-fullformat-german in den Server-Container
mounten. Der vorhandene CAW-Loader lädt das Deck beim Start; das Host-Menü
kann es auswählen. **Achtung:** Geladene deutschsprachige Packs können
auch in CAWs Standardauswahl landen; dieses Erwachsenen-Deck darf
nicht ungeprüft auf einer öffentlichen Instanz bereitgestellt werden.
Die Konvertierung ersetzt keine rechtliche Freigabe.
