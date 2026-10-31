# Roadmap

Jeder Meilenstein endet mit grüner CI und etwas, das man ausprobieren kann.
Details und Häkchen: [TODO.md](TODO.md). Hintergrund: [KONZEPT.md](KONZEPT.md),
[KONZEPT-CPP-BACKEND.md](KONZEPT-CPP-BACKEND.md), [UI-KONZEPT.md](UI-KONZEPT.md).

| # | Meilenstein | Ergebnis, das man anfassen kann |
|---|---|---|
| M0 | Gerüst und Infrastruktur | `docker compose up` liefert die Startseite über Caddy, C++-Server antwortet auf `/healthz`, CI läuft |
| M1 | Spielregeln übernommen | Firmware-Regeln + Tests laufen im Server-Repo grün, ganze Partien als Test |
| M2 | Echtzeit-Server und Sessions | Session anlegen, beitreten, spielen per Bot-Skript; Neustart der App verliert nichts |
| M3 | Spieler- und Host-UI | Eine echte Partie mit Handys, Host steuert im Browser |
| M4 | Big Screen und QR | Laptop/TV zeigt Lobby-QR, Frage, Sätze in der Juror-Phase, Sieger, Endstand |
| M5 | Kartenpacks | Import der vorhandenen Packs, Kategorien, bis zu 8 Packs gemischt |
| M6 | Persistenz und Wall of Fame | Statistik und Siegersätze bleiben, Name·Wort pro Gerät |
| M7 | Performance und Härtung | Lasttest erfüllt die Budgets aus KONZEPT 5a, Rate-Limits, Backpressure |
| M8 | Betrieb und Release v1.0 | Läuft auf eurem Server mit TLS, Backups, Monitoring, Datenschutzseite |

Reihenfolge mit Absicht: Die Spielregeln (M1) kommen vor jeder UI. Sie sind
auf dem X4 schon getestet und werden samt Tests übernommen statt neu
geschrieben (siehe KONZEPT-CPP-BACKEND.md). Der Big Screen (M4) kommt erst,
wenn das Spiel auf Handys läuft, weil er nur anzeigt, was der Server schon hat.

Später (nicht v1): mehrere App-Instanzen, X4 als Big Screen oder Host einer
Web-Session, weitere Sprachen, Pack-Upload durch Hosts.
