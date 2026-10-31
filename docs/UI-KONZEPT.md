# UI-Konzept

Grundlage: die Handy-Seite der X4-Firmware
(`src/apps_local/cardparty/web/CardPartyPage.html` in `0b-ivan/inx-4x-pro`)
und deren Gestaltungsregeln (`docs/node-terminal-design-system.md`, NODE-Stil).
Die Handy-Ansicht soll sich **wie heute** anfühlen. Neu sind Startseite,
Host-Bereich im Browser (statt X4-Tasten) und der Big Screen.

## 1. Gestaltungsprinzipien

- **Monochrom.** Schwarz `#000` als Grund, Weiß `#fff` als Schrift und Linie.
  Keine Farben, keine Grautöne, keine Verläufe, Schatten, Rundungen, Glows
  oder Animations-Effekte.
- **Monospace für alles.** `ui-monospace, Menlo, SFMono-Regular, Consolas,
  Liberation Mono` – keine Webfont-Pflicht (Performance-Budget).
- **Typografie trägt die Hierarchie:** kurze Labels in GROSSBUCHSTABEN mit
  Sperrung, Fließtext normal geschrieben, Fragen groß.
- **Linien statt Kästen:** 1 px weiße Trennlinien; Auswahl = invertiert
  (weißer Grund, schwarze Schrift); deaktiviert = gestrichelter Rahmen.
- **Befehle in eckigen Klammern:** `[ ABGEBEN ]`, `[ PAUSE ]`, `[ START ]`.
- **Touch zuerst:** Tippziele mindestens 48 px hoch, wichtige Aktionen
  unten in der Fußleiste, Daumenreichweite.
- **Ruhe:** Neu gezeichnet wird nur, wenn sich wirklich etwas ändert. Keine
  Lade-Animationen; ein Status in Worten („VERBINDE …“, „WARTE AUF JUROR“).

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

Werkzeuge in der Kopfzeile: `[⏸]` Pause, `[≡]` Menü (Ruhmeshalle, Sprache,
Regeln; für den Host zusätzlich der Host-Bereich).

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

### 4.2 Session anlegen (Host)

Regeln mit Standardwerten, alles später im Spiel änderbar:
Runden, Antwort-Uhr, Verlängerung, Juror-Uhr, Auto-Weiter, Rando, Host spielt
mit (ja/nein), Packs und Kategorien. Dann `[ SESSION ERÖFFNEN ]`.

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
`GEWINNER: ADA·TOAST`, Punktestand. Host: `[ NÄCHSTE RUNDE ]`.

### 4.8 Pause, Warten, Entfernt

Vollflächige Hinweise mit Linie oben und unten: „PAUSE · GESETZT VON IVAN“,
`[ WEITER ]` nur für den, der pausiert hat, und den Host. „WARTET AUF ADA“
mit Sekunden. „DU WURDEST ENTFERNT“ mit `[ NÄCHSTE LOBBY ]`.

### 4.9 Ende

Krone über dem/den Sieger(n), Endstand als Balken aus Zeichen
(`████████  7`), Siegersätze der Partie, `[ NOCHMAL ]` (Stimme), Zähler
„3 VON 5 WOLLEN NOCHMAL“. Host: `[ REMATCH ]` `[ ENDE ]`.

### 4.10 Host-Bereich (Menü `≡` → HOST)

Liste wie das X4-Menü, jede Zeile eine Aktion mit Zustand rechts:
PAUSE · +ZEIT · REGELN · KATEGORIEN · PACKS · SPIELER (entfernen,
zweimal tippen) · LOBBY SPERREN · SPIEL BEENDEN (zweimal tippen) ·
HOST ÜBERGEBEN. Gefährliche Aktionen brauchen eine zweite Bestätigung.

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

Die Frage mit **jeder Antwort eingesetzt**, als nummerierte ganze Sätze
untereinander; bei mehr Sätzen zweispaltig. Eingesetzte Teile invertiert.
Kopfzeile: „JUROR ADA ENTSCHEIDET“.

### Auflösung

Nur der Siegersatz, sehr groß, Antworten invertiert, darunter der Gewinner
und eine schmale Punktetabelle.

### Ende

Zeremonie wie am X4: Krone, Sieger, Balken-Endstand, danach im Wechsel die
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

## 8. Nicht vorgesehen

Farbthemes, Sounds, Konfetti, Avatare, Emojis in der UI, Chat. Wenn eines
davon gewünscht wird, kommt es zuerst ins Konzept.
