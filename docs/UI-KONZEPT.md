# UI-Konzept

Grundlage: die Handy-Seite der X4-Firmware
(`src/apps_local/cardparty/web/CardPartyPage.html` in `0b-ivan/inx-4x-pro`)
und deren Gestaltungsregeln (`docs/node-terminal-design-system.md`, NODE-Stil).
Die Handy-Ansicht soll sich **wie heute** anfühlen. Neu sind Startseite,
Host-Bereich im Browser (statt X4-Tasten) und der Big Screen.

## 1. Gestaltungsprinzipien

- **Monochrom.** Schwarz `#000` als Grund, Weiß `#fff` als Schrift und Linie.
  Bedienelemente bleiben flach und kontrastreich. **Ausnahme: physische
  Spielkarten** dürfen gebrochenes Weiß (`#f4f4f0`), eine feine neutrale
  Papierkante, bis zu 6 px Eckradius und einen dezenten schwarzen/weißen
  Versatzschatten bekommen. Keine Akzentfarben, Glows oder Verläufe.
  Animationen nur für die bereits vorgesehenen CAW-Intros, Gewinner- und
  Timer-Signale.
  **Weitere gezielte Material-Ausnahme:** Eingesetzte Antworttexte dürfen als
  schwarze Kunststoff-Prägebänder (klassisches DYMO-Reliefband) erscheinen:
  helle, sichtbar erhabene Reliefbuchstaben mit hellen und dunklen
  Lichtkanten, leicht glänzender schwarzer Kunststoff und sanfte
  Band-Schlagschatten. Ausschließlich für die Kunststoffbänder sind
  dezente monochrome CSS-Verläufe und mehrlagige Schatten erlaubt; sie
  simulieren Material und Prägung, nicht eine farbige oder animierte UI.
  Die Schrift auf dem Band ist eine gezielte Ausnahme vom Monospace-Prinzip:
  eine verfügbare runde, mechanisch wirkende Systemschrift (ohne Webfont).
  Feste, datenunabhängige Materialvarianten bilden kleine Knicke,
  Reflexionswinkel und nicht perfekt gerade Kanten nach; kein Zufall bei jedem
  Rendern. Kein Bild und keine Animation. Die normalen Handkarten und
  Juror-Antwortoptionen bleiben frei von Prägebändern; beim Einsetzen in
  den Fragesatz entsteht das Band. Lange Antworten werden anhand der
  tatsächlichen Textzeilen in eigenständige, geringfügig unterschiedliche
  Bandstücke aufgeteilt. Auch bei Viewport-Änderungen und auf dem Big Screen
  bleiben Text und Zeilenumbrüche vollständig lesbar, ohne Overflow.
- **Monospace für alles.** `ui-monospace, Menlo, SFMono-Regular, Consolas,
  Liberation Mono` – keine Webfont-Pflicht (Performance-Budget).
- **Typografie trägt die Hierarchie:** kurze Labels in GROSSBUCHSTABEN mit
  Sperrung, Fließtext normal geschrieben, Fragen groß.
- **Linien statt Kästen:** 1 px weiße Trennlinien für Navigation und
  Struktur; echte Frage-/Antwortkarten sind die bewusste Ausnahme. Auswahl
  erhält eine klar unterscheidbare invertierte Kartenfläche plus
  Positionskennzeichnung; deaktivierte Aktionen bleiben gestrichelt.
- **Befehle in eckigen Klammern:** `[ ABGEBEN ]`, `[ PAUSE ]`, `[ START ]`.
- **Touch zuerst:** Tippziele mindestens 48 px hoch, wichtige Aktionen
  unten in der Fußleiste, Daumenreichweite.
- **Kein Seitenscrollen.** Jede Ansicht passt in 320×568, 390×844 und
  1280×800 px: Kopfzeile, Inhalt und Fußleiste auf einem Bildschirm. Reicht
  der Platz nicht, scrollt nur eine Liste in sich (Hand, Antworten, Spieler,
  Menü), nie die Seite, und die Hauptaktion bleibt sichtbar. Lieber einen
  Schritt weglassen oder zusammenlegen als scrollen lassen. Playwright prüft
  das für jede Ansicht. Ausnahme: die Textseiten Lizenzen, Datenschutz und
  Impressum, die aus der Fußzeile der Startseite verlinkt sind.
- **Ruhe:** Neu gezeichnet wird nur, wenn sich wirklich etwas ändert.
  Keine Endlosschleifen oder Lade-Animationen. Ausnahme: einmaliger
  CAW-Bootscreen beim ersten Seitenaufruf der Browser-Sitzung; kurze
  Gewinner- und Zehn-Sekunden-Signale. Status immer auch als Text.

## 2. Design-Tokens

| Token | Wert |
|---|---|
| `--ink` / `--paper` | `#fff` / `#000` |
| `--line` | `1px solid var(--ink)` |
| Schrift Basis | 16 px, Zeilenhöhe 1.4 |
| Label (`.eyebrow`) | 12 px, `letter-spacing: .05em`, GROSS |
| Frage Handy (`h1`) | `clamp(22px, 6vw, 30px)` |
| Frage Big Screen | `clamp(40px, 5vw, 96px)` |
| Tippziel | min. 48 px (Liste 56 px) |
| Abstände | 8 / 14 / 16 / 20 px |
| Layout-Breite Spieler/Host | Handy bis 720 px; Desktop ab 900 px zweispaltiges Spielfeld bis 1040 px; Big Screen volle Fläche |
| Fokus | 2 px weiße Outline, 3 px Abstand |

Eine CSS-Datei für alle Ansichten, Ansicht-spezifisch nur Layout und Größen.

## 3. Aufbau jeder Handy-Seite

```
┌─────────────────────────────────────┐
│ C/A/W            ● VERBUNDEN  [⏸][≡] │  Kopfzeile: Marke, Verbindung, Werkzeuge
│ RUNDE 3/9 · JUROR: ADA·TOAST   0:42 │  Meta: Runde, Juror, Uhr
├─────────────────────────────────────┤
│                                     │
│   Inhalt der Phase (scrollt)        │
│                                     │
├─────────────────────────────────────┤
│ 2/3 GEWÄHLT              [ ABGEBEN ] │  Fußleiste: Status links, Hauptaktion rechts
└─────────────────────────────────────┘
```

Werkzeuge in der Kopfzeile: `[≡ MENÜ]` in die Meta-Zeile integriert.
Das Menü gliedert sich durch Trennlinien in SPIEL, HOST und SESSION.
Die Restzeit bleibt auf Handy und Big Screen stets oben rechts sichtbar;
ab 00:10 wird sie größer und invertiert blinkend dargestellt, bei 00:00
bleibt sie statisch. `prefers-reduced-motion` deaktiviert das Blinken.

## 4. Ansichten Handy

### 4.1 Startseite `/`

```
C/A/W  CARDS AGAINST WHATEVER
────────────────────────────────
[ NEUE SESSION ]
────────────────────────────────
CODE EINGEBEN
[ K 7 M 2 Q X ]      [ BEITRETEN ]
────────────────────────────────
DE / EN · LIZENZEN · DATENSCHUTZ
```

Die Startseite soll ohne vertikales oder horizontales Scrollen in 320×568,
390×844 und 1280×800 px passen. Der Header bleibt kompakt; Session-Aktion,
Code-Eingabe, Sprachwahl und Status müssen ohne Scrollen sichtbar bleiben.
Neben `[ NEUE SESSION ]` steht die Schaltfläche `[ ANLEITUNG ]`.
Sie öffnet einen per Tastatur und Touch bedienbaren, intern scrollbaren
Hilfedialog (de/en), gegliedert in zwei Bereiche:
- **SO GEHT'S LOS:** Host erstellt eine Session auf der öffentlichen CAW-Seite;
  Mitspieler öffnen den Einladungslink, scannen den QR-Code oder geben
  den sechsstelligen Code ein; optional wird BIG SCREEN auf einem anderen
  Bildschirm geöffnet. Der Host kann ab drei Spielern starten.
- **SPIELREGELN:** 3–8 Spieler, sieben Handkarten als Standard (3–10 vor
  Spielstart einstellbar), rotierender Juror, Pick 1–3 und gelegentlich
  Draw 2, anonyme Abgabe, Juror bestimmt Rundensieger mit einem Punkt,
  Handauffüllung, einmaliger Kartentausch pro Runde, Spielende und
  Endstand. Optionale Rando-/Zeitregeln werden kurz erläutert.

Die Anleitung setzt **öffentliches Hosting** voraus. Hinweise zu
`localhost`, LAN-IP, WLAN, Tunnel-Konfiguration oder lokalen Servern
gehören **nicht** in den Spielhilfe-Dialog. Schließen per Button oder Escape.
Erst wenn Textzoom/Extremgrößen den Platz überschreiten, ist vertikales
Scrollen als barrierefreie Ausnahme erlaubt. Keine festen Höhen, die Inhalte
abschneiden.

Die Kopfzeile verwendet die **CUT-DECK-Marke**: geneigte, rechteckige C- und
W-Karten mit geraden 90°-Ecken und spiegelbildlichen Eckwinkeln unten links
bzw. rechts; die mittlere A-Karte ist weiß invertiert. Rechts folgt ein
schräger Slash und das Wortzeichen `CAW / CARDS AGAINST WHATEVER`.
Keine runden Karten, Zylinder oder 3D-Effekte.

Beim **ersten Aufruf je Browser-Sitzung** erscheint ein einzelner
Bootscreen mit **ca. 1,7 Sekunden** Laufzeit. Die einzelnen Bewegungen
behalten ihr ursprüngliches ruhiges Tempo: Deck verschiebt sich
(ca. 520 ms) → Slash erscheint (ca. 320 ms) → Wortzeichen erscheint
(ca. 340 ms) → kurzer Übergang zur Startseite (ca. 260 ms).
**Kartenmischen, bildfüllender A-Karten-Zoom und unnötige Wartepausen
entfallen**, statt die gesamte Animation schneller abzuspielen.
Die Animation läuft nicht in Schleife; der Skip-Button reagiert sofort.
Sie kann über das Logo auf der Startseite erneut ausgelöst und jederzeit
übersprungen werden. `sessionStorage` merkt den einmaligen Besuch; ohne JS
bleibt die Startseite verwendbar. Bei `prefers-reduced-motion` wird der
Bootscreen ohne Bewegung übersprungen. `[ NEUE SESSION ]` legt die Session
an, merkt sich den Host-Token im Browser und öffnet `/s/{code}`.
`[ BEITRETEN ]` fragt den Code beim Server nach und öffnet dieselbe Seite;
falsche oder abgelaufene Codes werden auf der Startseite in Worten gemeldet.

### 4.2 Session anlegen (Host)

Kein eigener Regel-Bildschirm vor dem Anlegen (ein Schritt weniger, nichts zu
scrollen): `[ NEUE SESSION ]` eröffnet die Session sofort mit den
Standardregeln des X4 (10 Runden, Rando an, Uhren aus) und zeigt die
Host-Lobby. Die Regeln stellt der Host im Menü unter SPIELREGELN ein, jederzeit;
sie gelten sofort für die **laufende** Session, die Rundenzahl nur in der
Lobby. Jede Regel hat `[‹]` / `[›]` und springt durch dieselben Werte wie der
X4: Runden 3/5/8/10/12/15/20/30, Antwort-Uhr AUS/30/45/60/90/120 S,
Verlängerung AUS/10/15/20/30 S, Juror-Uhr AUS/30/60/90 S, Auto-Weiter
AUS/5/8/12 S, Rando AN/AUS. Mitspielen: in der Lobby Namen eingeben und
`[ MITSPIELEN ]`. Packs und Kategorien folgen mit M5.

### 4.3 Beitreten

```
SESSION K7M2QX
────────────────────────────────
DEIN NAME
[ Ivan                       ]
ZU LANG / BEREITS VERGEBEN (Fehler in Worten)
                     [ BEITRETEN ]
```

### 4.4 Lobby

Spieler: Spielerliste zweispaltig (`IVAN·TOAST`), „3 VON MIND. 3“, Hinweis
„WARTE AUF HOST“. Host zusätzlich: QR links, rechts Code, URL,
`[ LINK KOPIEREN ]` und `[ TEILEN ]` (nur wenn das Gerät einen
Teilen-Dialog hat), darunter `[ MITSPIELEN ]`, in der Fußleiste `[ START ]`
(gestrichelt bis 3 Spieler), dazu `[ BIG SCREEN ]`. LOBBY SPERREN steht im
Menü; gesperrt zeigt die Fußleiste „LOBBY GESPERRT“, Neue sehen „LOBBY
GESPERRT · FRAG DEN HOST“.

### 4.5 Antworten

Die weißen Handkarten zeigen ihre Antworttexte ohne Prägeband oder
Reliefeffekt als normalen dunklen Text auf hellem Papier, damit auch längere
Antworten sofort lesbar bleiben. Die invertierte Darstellung ausgewählter
Handkarten bleibt bestehen. Erst wenn Antworten in die Frage eingesetzt oder
in der Juror- und Auflösungsansicht gezeigt werden, erscheint das schwarze
Prägeband.

Die Hand enthält standardmäßig **7 Karten** (Host-Einstellung **3–10** im Menü SPIELREGELN, ausschließlich in der Lobby). Spieler sehen immer alle gerade zugeteilten Karten; bei Draw 2 kann die Hand kurzfristig um bis zu zwei Karten wachsen. Geändert wird nur vor dem Start einer neuen Partie; bereits gespeicherte und laufende Partien behalten ihren Wert.

Frage oben in eigenem Scroll-Bereich (max. 30 % Höhe), darunter die Hand
als durchgängiges Raster echter Spielkarten (zwei Spalten am Handy, auf dem
Desktop rechts neben der Frage). **Alle aktuell zugeteilten Handkarten (standardmäßig sieben) sind zugleich im DOM**
und ohne Seitenwechsel erreichbar. Nur die Kartenhand scrollt vertikal;
Frage, Kopfzeile und `[ ABGEBEN ]` bleiben stehen. Lange Kartentexte dürfen
nicht abgeschnitten oder durch Verkleinern unlesbar werden. Antippen wählt
in Reihenfolge; die Position steht vor der Karte (`<1/3>`). Die Frage ist
zugleich die Vorschau: gewählte Karten stehen als geprägte Antwortbänder
in ihren Lücken, keine eigene Vorschauzeile. Fußleiste: `[ ABWERFEN ]` (1× pro Runde, genau
eine Karte gewählt), `2/3 GEWÄHLT`, `[ ABGEBEN ]`.
Nach Abgabe: „ABGEGEBEN · 4/6“ mit Fortschritt aus Zeichen (`████░░`).

### 4.6 Juror wählt

Die Frage steht **einmal oben**, darunter nur die Antworten, nummeriert und
anonym (`[2] ANTWORT A / ANTWORT B`). Lange Fragen würden sonst in jedem
Eintrag wiederholt und die Liste etwa verdoppeln. Juror: antippen = auswählen;
die gewählte Antwort erscheint als geprägtes Band in den Lücken der Frage oben,
so liest er den ganzen Satz vor dem `[ BESTÄTIGEN ]`. Die anonymen
Antwortoptionen darunter bleiben als normaler, gut lesbarer Text auf weißen
Karten (ausgewählt: invertiert). Ausschließlich die in die obere Frage
eingesetzten Antworten erhalten das Prägeband. Alle anderen:
„JUROR ENTSCHEIDET“ und dieselbe Liste zum Mitlesen (ohne Auswahl). Der Big
Screen zeigt weiterhin ganze eingesetzte Sätze (Abschnitt 5).

### 4.7 Auflösung

Siegersatz groß, eingesetzte Antworten als **geprägte schwarze Prägebänder**,
darunter
`GEWINNER: ADA·TOAST`, Punktestand. Das Gewinner-Handy zeigt kurz eine
textliche `DU HAST DIE RUNDE GEWONNEN`-Meldung mit zweimaliger Invertierung;
bei reduzierter Bewegung nur statisch. Host: `[ NÄCHSTE RUNDE ]`.
Bei aktiviertem Auto-Weiter zählt die Auflösung sichtbar herunter und wechselt
anschließend selbst in die nächste Runde.

### 4.8 Pause, Warten, Entfernt

Vollflächige Hinweise mit Linie oben und unten: „PAUSE · GESETZT VON IVAN“,
`[ WEITER ]` nur für den, der pausiert hat, und den Host. „WARTET AUF ADA“
mit Sekunden. „DU WURDEST ENTFERNT“ mit `[ NÄCHSTE LOBBY ]`.

### 4.9 Ende

**Keine Krone.** Große Sieger-Typografie und Endstand als Balken aus Zeichen
(`████████  7`), `[ NOCHMAL ]` (Stimme), Zähler „3 VON 5 WOLLEN NOCHMAL“.
Host: `[ REMATCH ]`; zurück zur Startseite über das Menü. Darunter die
Siegersätze der Partie („ADA: …“), die Liste scrollt in sich.

### 4.11 Ruhmeshalle (Menü → RUHMESHALLE)

Für alle. `NAME·WORT` mit „3 SIEGE · 12 RUNDEN · 5 SPIELE“, acht pro Seite
mit `[‹]` `[›]`, darunter die fünf neuesten Siegersätze. Ohne Datenbank:
„NOCH KEINE EINTRÄGE“.

### 4.10 Host-Bereich (Menü `≡` → HOST)

Im Untermenü **SPIELREGELN** zeigt eine eigene Stepper-Zeile `HANDKARTEN` die aktuelle Handgröße (3–10), mit Standard `7`. Wie die Rundenzahl lässt sich diese Einstellung **nur in der Lobby** ändern; während der Partie ist sie sichtbar, aber gesperrt. Die Einstellung wird in den Serverstatus und in Redis-Snapshots übernommen.

Schlichte Zeilen, keine einzeln umrahmten Schaltflächen; gruppiert unter
SPIEL / HOST / SESSION, mit Zustand rechts. Gezeigt wird nur, was im
aktuellen Zustand geht:

- SPIEL: PAUSE/WEITER (Spieler), SPRACHE DE/EN (pro Gerät)
- HOST: START · PAUSE/WEITER · JETZT AUFDECKEN · +ZEIT · RUNDE ABBRECHEN
  (zweimal tippen) · SPIELREGELN · SPIELER (entfernen, zweimal tippen) ·
  SPIEL BEENDEN (zweimal tippen) · REMATCH
- SESSION: LINK KOPIEREN · TEILEN · STARTSEITE

Gefährliche Aktionen brauchen eine zweite Berührung innerhalb von 3 s
(„NOCHMAL TIPPEN“). Dazu für den Host: KARTEN-PACKS (nur Lobby) und LOBBY
SPERREN/ÖFFNEN, für alle: BIG SCREEN (QR des Links) und RUHMESHALLE. Offen:
HOST ÜBERGEBEN.

## 5. Big Screen `/s/{code}/screen`

Für 16:9 und 4:3, lesbar aus 4 m. Kein Scrollen, kein Bedienelement außer
`[ VOLLBILD ]` (blendet sich nach 3 s ohne Mausbewegung aus). Text wird auf
die Fläche eingepasst: Er startet bei `clamp(40px, 5vw, 96px)` und wird in
Schritten von 8 % kleiner, bis alles passt. Kopfzeile: Marke, Runde und
Juror, Uhr rechts; Fußzeile: Status in Worten. Den Link findet jeder im Menü
unter BIG SCREEN als QR-Code, der Host zusätzlich in der Lobby.

### Lobby

```
┌──────────────────────────────────────────────────────────────┐
│ C/A/W                                        SESSION K7M2QX   │
├──────────────────────────────┬───────────────────────────────┤
│                              │  SPIELER (4 VON MIND. 3)       │
│        ██████████            │  IVAN·TOAST                    │
│        ██ QR-CODE ██         │  ADA·KRAKE                     │
│        ██████████            │  BERT·SOCKE                    │
│                              │  CYRA·LAMA                     │
│  caw.example.org/s/K7M2QX    │                               │
├──────────────────────────────┴───────────────────────────────┤
│ HANDY SCANNEN ODER CODE EINGEBEN · WARTE AUF HOST              │
└──────────────────────────────────────────────────────────────┘
```

### Antworten

Frage riesig in der Mitte, oben Runde und Juror, unten Fortschritt
`ABGEGEBEN ████░░ 4/6` und Uhr. Keine Namen, wer fehlt (keine Bloßstellung);
nur bei „WARTET AUF …“ der Name, wie am Handy.

### Juror entscheidet

Der Countdown bleibt oben rechts sichtbar, auch wenn der Juror wählt.
Die Frage mit **jeder Antwort eingesetzt**, als nummerierte ganze Sätze
untereinander; bei mehr Sätzen zweispaltig. Eingesetzte Teile als geprägte
schwarze Prägebänder.
Kopfzeile: „JUROR ADA ENTSCHEIDET“.

### Auflösung

Nur der Siegersatz, sehr groß, Antworten als geprägte Prägebänder, darunter der Gewinner
und eine schmale Punktetabelle.

### Ende

Zeremonie ohne Krone: Sieger-Typografie, Balken-Endstand, darunter alle 6 s
der nächste Siegersatz der Partie, „x VON y WOLLEN NOCHMAL“ und in der
Fußzeile Rechteinhaber und Lizenz der Packs.

## 5a. Eigene Karten – Host-Editor (nach M8)

Im Host-Menü der Lobby führt **EIGENE KARTEN** zu einer intern scrollbareren
Menüseite mit Fragen und Antworten. Für jede Karte gibt es ein Textfeld und
`[ ENTFERNEN ]`, hinzu kommen `[ FRAGE HINZUFÜGEN ]` beziehungsweise
`[ ANTWORT HINZUFÜGEN ]`. Eine Erklärung für `___` steht vor den Fragen.
`[ SPEICHERN & AKTIVIEREN ]` überträgt das gesamte Set nur mit Host-Token
an den Server und stellt es in die laufende Deckauswahl. Der Editor erlaubt
Import und Export als JSON-Datei, damit eigene Karten nicht an die Lebensdauer
der Session gebunden sind. Alle Fehler erscheinen als Text, nie nur als Farbe.

Schwarz/Weiß, Monospace, 48-px-Tippziele, Fokusmarkierung und keine scrollende
Seite bleiben verbindlich. Sämtliche editorbezogenen UI-Texte gibt es auf
Deutsch und Englisch. Eine laufende Partie sperrt das Speichern.

## 6. Responsiv

| Breite | Verhalten |
|---|---|
| 320–400 px | Kopfzeile ohne Umbruch, kleinere Labels, Frage max. 30 % Höhe |
| 401–899 px | kompakte Handy-/Tablet-Ansicht, Antwort-Hand zwei Spalten |
| ab 900 px (Laptop) | eigene Desktop-Komposition: Frage links, Karten und Aktionen rechts; Host-Lobby mit Einladung links und Spielerliste rechts |
| Big Screen | eigene Ansicht, volle Fläche, Text eingepasst |

Getestet mit Playwright bei 320×568, 390×844, 1280×800 und Big Screen
1920×1080 / 1024×768: kein horizontales Scrollen, Hauptaktion immer sichtbar,
keine abgeschnittenen Karten.

## 7. Barrierefreiheit und Sprache

- Kontrast ist durch Schwarz/Weiß maximal; Fokus immer sichtbar.
- Alles per Tastatur bedienbar (Laptop-Host); `aria-pressed` für Auswahl,
  `role="status"` für Statuszeilen.
- Keine Information nur über Position oder Farbe; Auswahl auch als Text
  (`<1/3>`).
- Sprache Deutsch und Englisch, umschaltbar, alle Texte aus
  Übersetzungstabellen. Session-Sprache vom Host, Anzeige-Sprache pro Gerät.
- Umlaute in Karten und Namen werden korrekt dargestellt und nie zerschnitten.
- Der Bootscreen bietet einen sofort erreichbaren `[ ÜBERSPRINGEN ]`-Knopf;
  solange er angezeigt wird, ist die Startseite nicht fokussierbar.
- Playwright prüft scharfkantige Kartenecken, einmalige Intro-Wiedergabe,
  Überspringen, erneuten Aufruf, `prefers-reduced-motion` und
  320/390/1280-px-Ansichten ohne horizontalen Überlauf.

## 8. Nicht vorgesehen

Farbthemes, Sounds, Konfetti, Avatare, Emojis in der UI, Chat sowie
laufende dekorative Animationen. Die oben definierten Ausnahmen gelten.
Wenn eines
davon gewünscht wird, kommt es zuerst ins Konzept.

## 11. UI-Abnahme: kleine Lobby, erreichbare Einladungen und Desktop (2026-11-01)

- **Host-Lobby bei 320×568:** QR, Code und alle Einladungsaktionen
  einschließlich `BIG SCREEN` sind direkt bedienbar, ohne einen zweiten
  verschachtelten Scrollbereich für die Einladungsaktionen. Kopieren,
  Teilen (falls verfügbar) und Big Screen stehen auf schmalen Geräten in
  **einer gemeinsamen Reihe**, damit unter dem QR noch echte Spielerzeilen
  sichtbar bleiben. Nur die Teilnehmerliste darf bei vielen Spielern
  intern scrollen; die Start-Aktion bleibt fixiert. Alle Buttons bleiben
  mindestens 48 px hoch.
- **Einladungslinks im Heimnetz:** Wird die Host-Seite über
  `localhost`, `127.0.0.1` oder `[::1]` geöffnet, gibt es keine
  automatisch ermittelbare Adresse, die andere Geräte erreichen können.
  Deshalb bietet die Host-Lobby ein explizites Eingabefeld für die
  erreichbare Basisadresse (etwa `http://192.168.1.10:8080`).
  Bis zur gültigen Eingabe wird kein irreführender Loopback-QR-Code
  angeboten und keine solche Einladung kopiert/geteilt. Nach Eingabe
  verwenden QR, Kopieren und Teilen dieselbe bestätigte URL, ohne
  Host-Token. Die Eingabe wird nur im lokalen Browser gespeichert.
  Nach dem Speichern klappt das Feld zu, damit die Spielerliste auch bei
  320×568 Platz erhält; ein Tipp auf die angezeigte URL öffnet die
  Adressbearbeitung erneut. Bei einem bereits erreichbaren Hostnamen
  oder einer LAN-IP ist kein zusätzlicher Schritt erforderlich.
- **MacBook ab 900 px:** Frage links, Hand rechts und Hauptaktion unten
  rechts. Frage- und Antwortkarten dürfen den verfügbaren Raum besser
  nutzen (lesbarere Schrift, größere Karten), ohne aus dem Viewport zu
  ragen. Die Juror-Auswahl wird im rechten Bereich bei wenigen Antworten
  vertikal zentriert und erhält größere Tippflächen; bei vielen Antworten
  bleibt sie intern scrollbar. Diese Anpassungen gelten nicht für den
  eigenständigen Big Screen.
- **Regression:** Geometrie-Tests bei 320×568 und 390×844 für QR/
  Einladungsaktionen, Copy/Share-URL und Host-Link-Eingabe sowie
  Desktop-Checks bei 1280×800 für Kartentextgröße und Juror-Position.

## 10. Host-Krone (Mockup-Abgleich 2026-11-01)

Als **einzige neue Akzentfarbe** ist für die Host-Krone ein gedämpftes Gold
(`#D7AE4A`) freigegeben. Sie markiert eine Rolle, nicht einen Spielgewinn:
Kein Gold für andere Elemente, keine Glows, Animationen, Emojis oder
zusätzlichen Ressourcen. Das Symbol ist eine kleine, flache SVG-Krone und
steht immer gemeinsam mit der sichtbaren Beschriftung `HOST`; allein die
Farbe darf die Rolle nicht vermitteln.

- **Lobby auf Handy, Desktop und Big Screen:** Nur Teilnehmer, die über die
  authentifizierte Host-Verbindung **selbst mitspielen**, tragen neben ihrem
  Namen die goldene Krone mit `HOST`. Alle anderen bleiben unverändert.
- **Host ohne Spielerplatz:** Er erhält die Kennzeichnung an seiner eigenen
  Host-Kopfzeile, aber keinen künstlichen Spielerplatz. Ein Host kann nur
  durch ein gültiges Host-Token erkannt werden, nie durch seinen Namen oder
  `DU`.
- **Datenfluss:** Der Server veröffentlicht lediglich ein boolesches
  `isHost` für vorhandene, authentifizierte Spielerplätze. Host-Token,
  Verbindungsschlüssel und private Karten gelangen nicht in diese Liste.
  Bei Beitritt, Rollenbestätigung, Wechsel oder Trennung wird die
  öffentliche Kennzeichnung aktualisiert. Nach einem Neustart erscheint
  sie wieder, wenn sich der Host neu authentifiziert.
- **Abnahme:** Tests mit mitspielendem Host, eigenständigem Host ohne Sitz
  sowie normalem Spieler; dieselbe Kennzeichnung bei den anderen Handys
  und im Big Screen. Siegeransichten bekommen dadurch **keine Krone**.

## 9. Freigegebene Kartenoptik (Mockup-Abgleich 2026-11-01)

Die Mockups mit realistischer Spielkartenoptik sind für ein eigenes UI-Polish-PR
freigegeben. Diese Entscheidung präzisiert die strikten Flat-Design-Regeln in
Abschnitt 1; **nur Spielkarten** erhalten Materialität. Sie ändert keine
Spielregel und kein Client-/Server-Protokoll.

- **Marke und Startseite:** Das CUT-DECK-Logo bleibt aus CSS aufgebaut,
  zeigt aber 3–4 sichtbar versetzte, nahezu rechteckige Kartenrücken und
  eine schwarze Vorderkarte mit `C/A/W`. Keine Bilddateien oder Fonts nötig.
  Die primäre Aktion `NEUE SESSION` darf eine helle, invertierte Fläche
  erhalten; Nebenaktionen bleiben eckig und zurückhaltend.
- **Frage:** Die schwarze Fragekarte erscheint **zentriert und kompakt**
  wie im freigegebenen Handy-Mockup: maximal 380 px breit, mit leichter
  statischer Drehung und zwei versetzten Kartenrücken dahinter. Die
  Fragen-Schrift wird auf langen Karten reduziert; der Inhalt bleibt im
  Fragebereich scrollbar. Auf Desktop wird die Frage **nicht** als
  bildschirmbreites Panel dargestellt.
- **Antworten:** Antwortkarten sind papierweiß mit schwarzer Schrift,
  sichtbar von der schwarzen Fragekarte unterscheidbar. Die Handy-Hand zeigt
  **immer zwei Spalten**, ohne Paginierung: Alle zugeteilten Karten werden in einer
  zusammenhängenden Liste gerendert. Die Kartenhand besitzt einen eigenen
  vertikalen Touch-/Maus-/Tastatur-Scrollbereich mit sichtbarem Scrollbalken.
  Die Auswahlreihenfolge und der Scrollstand bleiben beim Antippen erhalten;
  die komplette Seite sowie die untere Hauptaktion scrollen nicht. Gewählte Karten
  invertieren wieder zu Schwarz mit weißer Schrift, erhalten eine starke
  weiße Kontur und die sichtbare Pick-Position. Die Auswahl bleibt zusätzlich
  über `aria-pressed` lesbar, nicht nur über Farbe oder Schatten. Auf Laptop
  bleibt das Spielfeld zentriert statt auf vier Spalten zu wachsen.
  `ABWERFEN` liegt als Nebenaktion über der Hand, der aktuelle Pick-Zähler
  steht beim Handtitel; `ABGEBEN` bekommt die volle Breite der unteren
  Aktionsleiste. Die Hauptaktion bleibt bei 320×568 sichtbar.
- **Juror:** Antwortoptionen erscheinen als einzelne, gut lesbare
  Kartenstreifen mit einem **sichtbaren Radio-Indikator**. Die ausgewählte
  Option wird invers und erhält einen deutlich gefüllten Indikator; das
  `aria-pressed`-Attribut bleibt maßgeblich. Die Frage steht weiterhin
  nur einmal oben und zeigt die gewählte Kombination. Die Hauptaktion
  `BESTÄTIGEN` füllt die untere Aktionsleiste aus. Keine fremden Hände
  oder Zuordnungen.
- **Auflösung:** Der Siegersatz erhält die Materialität einer großen
  hellen Karte; Name und Punktestand bleiben typografisch darunter.
  Keine Krone oder Konfetti als zwingender Bestandteil.
- **Big Screen:** In der Jurorphase steht eine kurze schwarze
  Fragekarte **oberhalb** der Antwortliste, damit dieselbe Komposition wie
  auf dem Handy sichtbar bleibt. Die **ganzen** eingesetzten Sätze folgen
  in papierhellen Kartenreihen, ab fünf Optionen in zwei Spalten. Der Inhalt
  wird weiterhin an 16:9 und 4:3 angepasst und nie abgeschnitten. Bei
  hohen Textmengen geht Lesbarkeit vor dekorativer Rotation.
- **Lobby:** Host: QR und Code als kompakter oberer Block, Teilnehmer
  darunter als einspaltige nummerierte/markierte Zeilen, feste
  `[ START ]`-Aktion. Spieler: derselbe klare Teilnehmerblock ohne die
  privaten Host-Werkzeuge, mit `WARTE AUF HOST`. Beide Zustände sind
  Teil der visuellen Abnahme; die Gast-Lobby darf kein Host-Token zeigen.
- **Desktopdarstellung:** Am MacBook wird die Spielansicht als eigenes,
  räumlich gegliedertes Desktop-Layout gestaltet, nicht wie eine vergrößerte
  Handyansicht und nicht als schmale 420-px-Handyspalte. Auf breiten Displays
  steht die kompakte gestapelte Frage links; Hand, Seitennavigation und
  Hauptaktion stehen rechts. Die Host-Lobby stellt QR-/Einladungsbereich
  und Teilnehmerliste nebeneinander dar, ohne den QR-Code zu überdimensionieren.
  Handy und Tablet behalten ihre vertikale Komposition. Der echte Big Screen
  (TV) bleibt vollflächig. Getestet werden Desktop 1280×800 und mobile 320×568
  sowie 390×844; kein horizontaler oder vertikaler Dokument-Scroll.
- **Performance/Accessibility:** Karten sind CSS, keine Bild-Assets oder
  zusätzlichen Abhängigkeiten. Keine Daueranimationen; `prefers-reduced-motion`
  und die Mindest-Tippflächen bleiben erhalten. Testmatrix 320×568,
  390×844, 1280×800, Big Screen 1920×1080 und 1024×768. Die Browser-Tests
  prüfen bei einer vollen Hand auch scrollbaren Zugriff auf die letzte Karte,
  Auswahl aus dem unteren Teil der Hand und feste Hauptaktion.
