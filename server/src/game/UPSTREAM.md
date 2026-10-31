# Herkunft der Regeln

Quelle: `0b-ivan/inx-4x-pro`, Commit
`b7b6dacd9c996b61e48a68f07b8216d08b8f7f21` (main, 2026-10-29).
Die `CardParty*.h` stammen aus `src/apps_local/cardparty/`.
Die elf Test-Suiten stammen aus `host-tests/cardparty/`.

Zuerst werden die Header unverändert geprüft. Im Testgerüst ersetzen
doctest-Assertions die bisherigen CHECK-Makros bzw. assert; Ausdrücke,
Szenarien, Seeds und Reihenfolge bleiben erhalten. Zusätzliche Klammern
schützen zusammengesetzte Ausdrücke vor doctests Expression-Decomposition.
Jede Suite bleibt ein eigenes Programm, damit gleichnamige Fixtures nicht
kollidieren. Assertions bleiben auch im Release-Build aktiv.

Nicht übernommen werden Activity, E-Ink, HalStorage, Hotspot, BrowserServer,
Wire-Protokoll, SD-Dateisnapshots und deren integrationsabhängige Tests.
Der SdPack-Reader arbeitet bereits ausschließlich über ByteSource; die
SD-Hardware steckt nicht in diesen Headern. Catalog bleibt als getestete,
reine Katalogvalidierung erhalten. Ein Pack-Importer gehört zu M5.

Das Demo-Deck ist ausschließlich eine Test-Fixture. Nach dem ersten grünen
Baseline-Lauf werden seine Texte durch nummerierte synthetische Testtexte
ersetzt, bei gleichen Kartenanzahlen, Pick- und Draw-Werten. Es gibt kein
mitgeliefertes spielbares Kartenpack.

Ergebnis vor jeder Regelanpassung: 11/11 Suiten grün (Release mit den
Original-Fixtures, Debug mit synthetischem Demo-Deck). GCC 14 warnt im
Debug-Build bei `test_shuffle` vor einem theoretisch zu kleinen Namenspuffer
für einen beliebigen `int`; nur dieser Testpuffer wächst von 8 auf 16 Bytes.
Die Regeln bleiben unverändert. SHA-256-Werte der unveränderten Header und
die tatsächlichen Ausgaben liegen unter `docs/validation/`.

## Server-Anpassungen nach der Übernahme

- `Sessions::disconnectAll()`: markiert nach einem Server-Neustart alle Sitze
  als getrennt. Die alten Verbindungs-IDs im Redis-Snapshot gehören zu
  Sockets, die es nicht mehr gibt; die Handys holen ihren Sitz per Ticket
  zurück (`resume`). Getestet in `tests/session/test_session.cpp`.
