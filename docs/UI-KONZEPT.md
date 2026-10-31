# UI-Konzept

Grundlage: die Handy-Seite der X4-Firmware
(`src/apps_local/cardparty/web/CardPartyPage.html` in `0b-ivan/inx-4x-pro`)
und deren Gestaltungsregeln (`docs/node-terminal-design-system.md`, NODE-Stil).
Die Handy-Ansicht soll sich **wie heute** anfühlen. Neu sind Startseite,
Host-Bereich im Browser (statt X4-Tasten) und der Big Screen.

## 1. Gestaltungsprinzipien

- **Monochrom.** Schwarz `#000` als Grund, Weiß `#fff` als Schrift und Linie.
  Keine Farben, keine Grautöne, keine Verläufe, Schatten, Rundungen, Glows
  oder Animations-Effekte, außer den ausdrücklich beschriebenen CAW-Intros,
  Gewinner- und Timer-Signalen.
- **Monospace für alles.** `ui-monospace, Menlo, SFMono-Regular, Consolas,
  Liberation Mono` – keine Webfont-Pflicht (Performance-Budget).
- **Typografie trägt die Hierarchie:** kurze Labels in GROSSBUCHSTABEN mit
  Sperrung, Fließtext normal geschrieben, Fragen groß.
- **Linien statt Kästen:** 1 px weiße Trennlinien; Auswahl = invertiert
  (weißer Grund, schwarze Schrift); deaktiviert = gestrichelter Rahmen.
- **Befehle in eckigen Klammern:** `[ ABGEBEN ]`, `[ PAUSE ]`, `[ START ]`.
- **Touch zuerst:** Tippziele mindestens 48 px hoch, wichtige Aktionen
  unten in der Fußleiste, Daumenreichweite.
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
| Layout-Breite Handy/Laptop | `min(100%, 720px)`, zentriert |
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
Erst wenn Textzoom/Extremgrößen den Platz überschreiten, ist vertikales
Scrollen als barrierefreie Ausnahme erlaubt. Keine festen Höhen, die Inhalte
abschneiden.

Die Kopfzeile verwendet die **CUT-DECK-Marke**: geneigte, rechteckige C- und
W-Karten mit geraden 90°-Ecken und spiegelbildlichen Eckwinkeln unten links
bzw. rechts; die mittlere A-Karte ist weiß invertiert. Rechts folgt ein
schräger Slash und das Wortzeichen `CAW / CARDS AGAINST WHATEVER`.
Keine runden Karten, Zylinder oder 3D-Effekte.

Beim **ersten Aufruf je Browser-Sitzung** erscheint ein einzelner Bootscreen:
Karten flach mischen (ca. 0,9 s) → Deck nach links → Slash erscheint →
Wortzeichen erscheint → A-Karte wächst als scharfkantiges, weißes Rechteck
bildfüllend → Startseite. Die Animation läuft nicht automatisch in Schleife.
Sie kann über das Logo auf der Startseite erneut ausgelöst und jederzeit
übersprungen werden. `sessionStorage` merkt den einmaligen Besuch; ohne JS
bleibt die Startseite verwendbar. Bei `prefers-reduced-motion` wird der
Bootscreen ohne Bewegung übersprungen. Die Startseite bietet weiterhin nur
M0-Aktionen; Session erstellen und beitreten funktionieren erst ab M2.

### 4.2 Session anlegen (Host)

Regeln mit Standardwerten, alles später im Spiel änderbar:
Runden, Antwort-Uhr, Verlängerung, Juror-Uhr, Auto-Weiter, Rando, Host spielt
mit (ja/nein), Packs und Kategorien. Dann `[ SESSION ERÖFFNEN ]`.
Im Host-Menü gelten spätere Regeländerungen unmittelbar für die **laufende**
Session, ohne eine neue Session anzulegen. Antwort- und Juror-Uhr haben
`−10 S` / `+10 S` mit Untergrenze; die laufende Restzeit wird angepasst.

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

Spieler: Spielerliste (`IVAN·TOAST`), „3 VON MIND. 3“, Hinweis „WARTE AUF
HOST“. Host zusätzlich: großer QR + URL + `[ LINK KOPIEREN ]` `[ TEILEN ]`,
`[ BIG SCREEN ]` (zeigt den Big-Screen-Link als QR), `[ LOBBY SPERREN ]`,
`[ START ]` (gestrichelt bis 3 Spieler).

### 4.5 Antworten

Frage oben in eigenem Scroll-Bereich (max. 30 % Höhe), darunter die Hand
als Liste. Antippen wählt in Reihenfolge; die Position steht vor der Karte
(`<1/3>`). Fußleiste: `[ ABWERFEN ]` (1× pro Runde), `2/3 GEWÄHLT`,
`[ ABGEBEN ]`. Vorschau des eingesetzten Satzes über der Fußleiste.
Nach Abgabe: „ABGEGEBEN · 4/6 WARTEN“.

### 4.6 Juror wählt

Juror: alle Antworten als **ganze eingesetzte Sätze**, nummeriert, anonym;
antippen = auswählen, `[ BESTÄTIGEN ]`. Alle anderen: „JUROR ENTSCHEIDET“ und
dieselben Sätze zum Mitlesen (ohne Auswahl).

### 4.7 Auflösung

Siegersatz groß, eingesetzte Antworten **invertiert hervorgehoben**, darunter
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
(`████████  7`), Siegersätze der Partie, `[ NOCHMAL ]` (Stimme), Zähler
„3 VON 5 WOLLEN NOCHMAL“. Host: `[ REMATCH ]` `[ ENDE ]`.

### 4.10 Host-Bereich (Menü `≡` → HOST)

Schlichte Zeilen, keine einzeln umrahmten Schaltflächen; gruppiert unter
SPIEL / HOST / SESSION, mit Zustand rechts: PAUSE · ±ZEIT · SPIELREGELN ·
KATEGORIEN · PACKS · SPIELER (entfernen, zweimal tippen) · LOBBY SPERREN ·
SPIEL BEENDEN (zweimal tippen) · HOST ÜBERGEBEN · BIG SCREEN LINK.
`BIG SCREEN LINK` zeigt einen kopierbaren `/s/{code}/screen`-Link statt
zusätzlichem QR-Dialog. Gefährliche Aktionen brauchen zweite Bestätigung.

## 5. Big Screen `/s/{code}/screen`

Für 16:9 und 4:3, lesbar aus 4 m. Kein Scrollen, kein Bedienelement außer
`[ VOLLBILD ]` (blendet sich nach 3 s aus). Text wird auf die Fläche
eingepasst (Schriftgröße schrittweise kleiner, bis alles passt).

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
untereinander; bei mehr Sätzen zweispaltig. Eingesetzte Teile invertiert.
Kopfzeile: „JUROR ADA ENTSCHEIDET“.

### Auflösung

Nur der Siegersatz, sehr groß, Antworten invertiert, darunter der Gewinner
und eine schmale Punktetabelle.

### Ende

Zeremonie ohne Krone: Sieger-Typografie, Balken-Endstand, danach im Wechsel die
Siegersätze der Partie. Fußzeile mit Lizenz und Rechteinhaber der Packs.

## 6. Responsiv

| Breite | Verhalten |
|---|---|
| 320–400 px | Kopfzeile ohne Umbruch, kleinere Labels, Frage max. 30 % Höhe |
| 401–719 px | Standard-Handy |
| ab 720 px (Laptop) | zentrierte 720-px-Spalte; Host kann am Laptop spielen |
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
