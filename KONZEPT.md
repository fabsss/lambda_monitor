# Konzept: Lambda-Monitor für ESP32-S3 (Seeed Studio XIAO ESP32-S3)

Dieses Dokument beschreibt das technische Konzept für eine ESP32-S3-Anwendung,
die das analoge Signal einer Lambda-Sprungsonde erfasst, interpretiert, über
die Zeit statistisch auswertet und auf einer selbst gehosteten HTTP-Seite
(via eigenem WLAN-Hotspot) live darstellt.

## 1. Zielsetzung

- Erfassung eines analogen 0–3 V Signals (extern per OPV aus dem
  Sprungsonden-Rohsignal verstärkt) an einem parametrierbaren ADC-Pin
- Umrechnung in eine verständliche "mager ↔ fett"-Skala
- Live-Anzeige (Zeiger/Skala) + Kurzzeit-Mittelwert (5 s) während der Fahrt
- Langzeit-Statistik (Min/Max, Zeitanteile je Bereich), überlebt Spannungsabfall
- Zeitlicher Signalverlauf ("Oszilloskop"-Ansicht)
- Betrieb an Klemme 15 (Zündung) ohne definiertes Herunterfahren → das Gerät
  kann jederzeit ohne Vorwarnung stromlos werden. Das Design muss dafür robust sein.

## 2. Hardware & Signalerfassung

### 2.1 Analogeingang

**Hardware-Basis:** Seeed Studio **XIAO ESP32-S3** auf dem
[LED Driver Board](https://wiki.seeedstudio.com/led_driver_board/), das die
Spannungsversorgung sowie einen Eingangs-Spannungswandler/-teiler bereits
mitbringt, sodass das extern per OPV auf 0–3 V verstärkte Sonden-Signal
direkt an eine Klemme des Boards angeschlossen werden kann.

- **Pin-Empfehlung:** einen der Pins **A0–A3** (`D0`–`D3`, entspricht
  `GPIO1`–`GPIO4`) verwenden, empfohlen **A0 (`D0` / `GPIO1`)** als
  Default, da dieser Pin auf dem XIAO-Modul am wenigsten wahrscheinlich für
  andere Bordfunktionen reserviert ist (`D4`/`D5` = `GPIO5`/`GPIO6` sind die
  Standard-I²C-Pins (SDA/SCL) und `D8`–`D10` sind die Standard-SPI-Pins –
  beide potenziell durch das LED Driver Board für Onboard-Peripherie
  belegt). Der ADC-Pin bleibt trotzdem **konfigurierbar** (Auswahl aus
  `A0`–`A3`, ggf. auch `A4`/`A5`, falls I²C auf dem Board ungenutzt ist)
- **Wichtige Randbedingung beim ESP32-S3:** `GPIO1`–`GPIO10` (also `A0`–`A5`
  sowie `D8`–`D10`) hängen an **ADC1**, `GPIO11`–`GPIO20` an **ADC2**. Da
  **ADC2 sich mit dem WLAN-Treiber die Hardware teilt** und im laufenden
  SoftAP-Betrieb (siehe Abschnitt 7) unzuverlässig bzw. blockiert ist, **muss**
  der Eingangspin zwingend ein **ADC1-Pin** sein – `A0`–`A3` erfüllen das
  und sind damit die sichere Wahl
- ⚠️ Hinweis: Ich konnte die genaue Beschaltung des LED Driver Boards (welcher
  physische Signal-/Klemmeneingang auf welches XIAO-GPIO geroutet ist) nicht
  automatisiert aus dem Seeed-Wiki verifizieren (Netzwerkzugriff auf die
  Domain war in dieser Umgebung blockiert). Bitte anhand des Schaltplans/der
  Beschriftung auf dem Board gegenprüfen, dass der genutzte Eingang tatsächlich
  auf einen der `A0`–`A3`-Pins geroutet ist – falls nicht, entsprechend einen
  anderen ADC1-fähigen Pin (`GPIO1`–`GPIO10`) in der Konfiguration wählen
- Nutzung der **ADC-Kalibrierung** (`esp_adc/adc_cali`), um Bauteiltoleranzen
  auszugleichen; ADC-Werte werden intern immer in mV umgerechnet, nie als
  Rohcounts weiterverarbeitet

### 2.2 Parametrierbarer Spannungsbereich

Da der externe OPV-Verstärkungsfaktor je nach Beschaltung/Sonde variiert,
wird der gültige Spannungsbereich nicht hart codiert, sondern als
Kalibrierwerte hinterlegt:

| Parameter | Bedeutung | Default |
|---|---|---|
| `u_min` | Spannung bei "sehr mager" (Sensor-Minimum) | 0,0 V |
| `u_max` | Spannung bei "sehr fett" (Sensor-Maximum) | 3,0 V |
| `u_lambda1` | Spannung am Umschaltpunkt λ = 1 | 1,5 V |
| `deadband` | Toleranzband um `u_lambda1`, das noch als "λ = 1" gilt | ±0,15 V |

Diese vier Werte sind über die Weboberfläche (Konfigurationsseite,
Abschnitt 8.4) änderbar und werden mit den übrigen Konfigurationsdaten im
Flash (NVS) persistiert.

### 2.3 Mittelwertbildung / Rauschunterdrückung

Zweistufiges Filterkonzept, da zwei unterschiedliche Zeitkonstanten gebraucht
werden (schnelle Live-Anzeige vs. träger Fahr-Mittelwert):

1. **Abtastung:** ADC wird mit fester Rate abgetastet (z. B. 100 Hz)
2. **Entrauschung (schnell):** gleitender Mittelwert / kleiner Medianfilter
   über ~5–10 Samples direkt nach der Abtastung, um ADC-/Zündrauschen zu
   unterdrücken, ohne die für die Sprungsonde relevante Schaltdynamik
   (Sprungfrequenz, siehe 3.3) zu verschlucken
3. **Anzeige-Mittelwert (langsam):** separater gleitender Mittelwert über die
   letzten 5 s (ringpuffer- oder EMA-basiert) für den in Screen 1 geforderten
   "Mittelwert über längeren Bereich"

Beide Mittelwerte laufen parallel und unabhängig, damit die schnelle Anzeige
(Zeiger) nicht durch die 5-s-Glättung träge wirkt.

## 3. Signalinterpretation

### 3.1 Eigenschaft der Sprungsonde (wichtig für die Skalierung)

Eine Lambda-**Sprungsonde** (Zirkonoxid-Sonde, keine Breitbandsonde) liefert
kein linear zu λ proportionales Signal. Sie kippt sprunghaft zwischen einer
niedrigen Spannung (mager) und einer hohen Spannung (fett) um den
stöchiometrischen Punkt λ = 1 – dazwischen ist die Kennlinie extrem steil,
außerhalb dieses schmalen Bereichs liefert sie **keine** verlässliche
quantitative Aussage über den genauen λ-Wert.

**Vorschlag:** Statt einen pseudo-exakten Lambda-Zahlenwert vorzutäuschen
(was fachlich falsch wäre), wird ein **"Gemisch-Index"** auf einer
normierten Skala von **-100 (sehr mager)** über **0 (λ = 1)** bis
**+100 (sehr fett)** berechnet – linear interpoliert zwischen den
Kalibrierpunkten `u_min → -100`, `u_lambda1 → 0`, `u_max → +100`. Zusätzlich
wird eine textuelle Kategorie abgeleitet:

| Gemisch-Index | Kategorie |
|---|---|
| -100 … -60 | sehr mager |
| -60 … -20 | mager |
| -20 … +20 | λ ≈ 1 (Zielbereich, `deadband`) |
| +20 … +60 | fett |
| +60 … +100 | sehr fett |

Die Schwellen der Kategorien sind ebenfalls parametrierbar (gleiche
Konfigurationsseite). Optional kann später eine reale λ-Näherung ergänzt
werden, falls z. B. auf eine Breitbandsonde umgestellt wird – die
Architektur (siehe 9) sieht dafür einen austauschbaren
"Signalinterpreter"-Baustein vor.

### 3.2 Sensor-Bereitschaft (Aufwärmphase)

Sprungsonden liefern erst ab ca. 300 °C ein gültiges Signal. Wichtig laut
Rückmeldung: Ein "Kleben" des Signals auf einem Wert ist **nicht** nur ein
Zeichen für "Sonde noch kalt" – es kann im Fehlerfall genauso bedeuten, dass
der Motor über längere Zeit tatsächlich dauerhaft zu mager oder zu fett
läuft. Diese beiden Fälle dürfen nicht verwechselt werden, sonst würde ein
echter Dauerfehler fälschlich als "Sonde nicht bereit" ausgeblendet.

**Überarbeitetes Konzept – Aufwärmphase nur als Zustand direkt nach dem
Einschalten, zeitlich begrenzt, danach nie wieder aktiv:**

Eine kleine Zustandsmaschine mit genau zwei Zuständen pro Betriebszyklus
(KL15 ein → aus):

```
        Power-On
           │
           ▼
     ┌───────────┐   Schaltaktivität erkannt   ┌───────────┐
     │ AUFWÄRMEN │ ───────────────────────────►│  BETRIEB  │
     └───────────┘   ODER Timeout t_warmup      └───────────┘
           │                                          │
           │ (max. t_warmup, konfigurierbar)          │ bleibt bis
           └──────────────────────────────────────────┘ Power-Off
```

- **`AUFWÄRMEN`** gilt **ausschließlich** unmittelbar nach dem Einschalten
  (Systemstart). Der Übergang nach `BETRIEB` erfolgt, **sobald zuerst**
  eintritt:
  - die erste plausible Schaltflanke erkannt wird (Signal überquert
    `u_lambda1` mit ausreichendem Hub, siehe 3.3), **oder**
  - ein konfigurierbarer Timeout `t_warmup` abgelaufen ist (Default-Vorschlag:
    **90 s** – beheizte Sprungsonden, wie sie seit den 1990ern Standard sind,
    erreichen ihre Betriebstemperatur meist bereits nach ca. 20–60 s;
    ältere unbeheizte Sonden können je nach Last/Außentemperatur auch
    1–3 Minuten benötigen. 90 s liegt damit mit Sicherheitsmarge über dem
    typischen Fall, bleibt aber über die Konfigurationsseite änderbar,
    falls eine konkrete Sonde/ein konkretes Fahrzeug abweicht)
- **`BETRIEB`** wird für den **Rest des Betriebszyklus nicht mehr
  verlassen** – ein späteres Kleben des Signals wird ab hier immer als
  reguläre Messung gewertet und fließt normal in die Statistik
  ("zu mager"/"zu fett" über die Zeit) ein. Es gibt **keine** erneute
  Reinterpretation als "Sonde nicht bereit"
- Während `AUFWÄRMEN` wird auf Screen 1 der Status **"Sonde wärmt auf…"**
  angezeigt, der Gemisch-Index/die Statistik-Akkumulation pausiert in
  dieser Phase (siehe 4.1), da die Werte in dieser Zeit ohnehin nicht
  aussagekräftig sind

**Alternative Idee (zusätzlich, statt als Ersatz):** Da ein dauerhaftes
Kleben nach der Aufwärmphase ja durchaus ein *eigenständig meldenswertes*
Ereignis ist (Regelkreis offen, Sondendefekt, oder eben ein echter
Dauer-Mager/Fett-Fehler), wird dafür ein **separater, informativer
Hinweis** vorgeschlagen, der die Statistik nicht beeinflusst, sondern nur
zusätzlich angezeigt wird: z. B. "⚠ Kein Sondenwechsel seit 5 min" auf
Screen 1, sobald seit `t_warmup` keine Schaltflanke mehr erkannt wurde. Das
ist rein additiv (siehe auch 3.3/8.1) und ersetzt nicht die normale
mager/λ1/fett-Auswertung.

### 3.3 Sprungfrequenz als Zusatzmetrik (Vorschlag)

Die Schaltfrequenz der Sprungsonde (Anzahl der Nulldurchgänge um
`u_lambda1` pro Minute) ist in der Praxis ein guter Indikator für die
Dynamik der Gemischregelung (Closed-Loop). Vorschlag: Diese Frequenz
zusätzlich mitzuloggen (siehe 4) und auf Screen 1 anzuzeigen – ein
plötzliches Einfrieren der Sprungfrequenz deutet z. B. auf offenen
Regelkreis oder Sondenausfall hin.

## 4. Statistik & Langzeitspeicher

### 4.1 Datenhaltung im RAM (Laufzeit)

- **Ringpuffer** fester Größe für die Live-Kurve (Screen 2), z. B. 300–600
  Punkte, dekimiert/downgesampled auf eine konfigurierbare Zeitfenstergröße
  (z. B. 60 s) – die Puffergröße ist damit unabhängig von der Abtastrate
  konstant und läuft **nie über** (ältester Punkt wird überschrieben)
- **Akkumulatoren** für die Statistik: Zeit "mager"/"λ=1"/"fett" **und**
  Zeit "Aufwärmphase" in Sekunden (`uint32_t`, reicht > 100 Jahre bei 1 Hz
  Inkrement → praktisch kein Overflow-Risiko), Min/Max des Gemisch-Index
  inkl. Zeitstempel, Sprungfrequenz (gleitender Mittelwert). Die
  Aufwärmzeit wird als eigener Sekundenwert mitgezählt (siehe 3.2), damit
  sie in der Auswertung (8.1) als eigener Anteil sichtbar ist, aber nicht
  mit "mager"/"fett" vermischt wird
- Es werden **zwei Ebenen** von Statistik geführt:
  - **Session-Statistik**: seit dem letzten Einschalten (KL15 an)
  - **Langzeit-Statistik**: kumulativ über alle Sessions (persistiert)

### 4.2 Persistenz im Flash (NVS statt "EEPROM")

Der ESP32-S3 hat kein echtes EEPROM, sondern nutzt für sowas den
**NVS-Treiber** (Non-Volatile Storage, Flash-basiert, ESP-IDF), der bereits
eingebautes **Wear-Leveling** und **Power-Loss-Sicherheit** (atomare
Schreibvorgänge über ein Log-structured Format) mitbringt. Das ist die
richtige Wahl, um die geforderten Punkte "kein korrupter Zustand nach
Spannungsabfall" und "kein Speicherüberlauf" strukturell zu erfüllen:

- Es wird **nur die aggregierte Langzeit-Statistik** (feste, kleine
  Struct-Größe) persistiert – **nicht** der Zeitverlauf. Dadurch bleibt die
  Datenmenge im Flash konstant und wächst nie unbegrenzt (kein "endloses
  Log", das irgendwann volllaufen könnte)
- **Zyklisches Schreiben** statt Schreiben bei jeder Änderung: Commit auf
  NVS nur alle z. B. 30–60 s ("Dirty-Flag" + Timer) sowie bei signifikanten
  Ereignissen (z. B. neuer Min/Max-Wert). Das reduziert Flash-Verschleiß
  drastisch (Flash verträgt typischerweise ~100.000 Schreibzyklen pro
  physischer Zelle, NVS verteilt das zusätzlich über mehrere Sektoren)
- **Zusätzliche Absicherung** über NVS hinaus: Struct erhält ein
  `struct_version`-Feld und eine CRC32-Prüfsumme; beim Booten wird die
  Prüfsumme validiert. Bei Mismatch (z. B. durch Spannungsabfall exakt
  während des Schreibens) wird auf einen Default-/Nullzustand
  zurückgefallen, statt mit korrupten Werten weiterzurechnen
- Da während des Schreibfensters (wenige ms, NVS-Commit) ohnehin ein
  Spannungsabfall auftreten könnte: NVS selbst bleibt dabei konsistent
  (alte oder neue Version, nie ein Mischzustand) – das ist der Kernvorteil
  gegenüber einer eigenen, naiven "EEPROM-Emulation"

### 4.3 Reset des Langzeitspeichers

- Button auf Screen 1 ("Langzeit-Statistik zurücksetzen"), mit
  Bestätigungsdialog (da destruktiv)
- Setzt die Langzeit-Struct im RAM **und** im NVS zurück
- Session-Statistik ist davon unabhängig und immer per Definition "seit
  Einschalten"

### 4.4 Beispiel-Datenstruktur (konzeptionell)

```c
typedef struct __attribute__((packed)) {
    uint16_t struct_version;
    uint32_t t_warmup_s;      // Zeit in der Aufwärmphase [s]
    uint32_t t_mager_s;       // Zeit im Bereich "mager" [s]
    uint32_t t_lambda1_s;     // Zeit im Zielbereich [s]
    uint32_t t_fett_s;        // Zeit im Bereich "fett" [s]
    int16_t  index_min;       // kleinster je gemessener Gemisch-Index
    int16_t  index_max;       // größter je gemessener Gemisch-Index
    uint32_t total_runtime_s; // Gesamtlaufzeit seit erstem Einsatz
    uint32_t crc32;           // Prüfsumme über obige Felder
} lambda_longterm_stats_t;
```

## 5. Vermeidung von Speicherüberlauf (Zusammenfassung)

| Speicher | Maßnahme |
|---|---|
| RAM Ringpuffer (Live-Kurve) | feste Größe, älteste Werte werden überschrieben (FIFO) |
| RAM Akkumulatoren | ausreichend breite Integer-Typen (uint32/int16), kein unbegrenztes Array |
| Flash / NVS | nur aggregierte Struct fester Größe wird geschrieben (kein Rohdaten-Log), zyklisches statt permanentes Schreiben, NVS-eigenes Wear-Leveling |
| Konfiguration | ebenfalls feste, kleine Struct in NVS |

## 6. Verhalten bei Klemme 15 / Spannungsversorgung

- **Einschalten:** Init der Peripherie, Laden der Langzeit-Statistik +
  Konfiguration aus NVS (mit CRC-Prüfung, Fallback auf Defaults), Start des
  WLAN-Access-Points und HTTP-Servers, Session-Statistik wird auf 0
  initialisiert
- **Laufender Betrieb:** siehe oben, zyklische NVS-Commits
- **Abschalten (Spannungsabfall ohne Vorwarnung):** Da kein sauberes
  Shutdown-Signal existiert (KL15 kann jederzeit hart wegfallen), wird
  **kein** spezielles Shutdown-Handling vorausgesetzt – die Robustheit wird
  stattdessen dadurch erreicht, dass jeder persistierte Zustand **jederzeit
  gültig** ist (siehe 4.2: atomare NVS-Schreibvorgänge, CRC-Absicherung,
  keine mehrstufigen/abhängigen Schreibvorgänge, die nur "zusammen"
  konsistent wären)
- Optional (spätere Ausbaustufe): Nutzung des internen
  **Brown-Out-Detektors** des ESP32-S3, um bei erkanntem
  Spannungseinbruch einen sofortigen, außerplanmäßigen NVS-Commit
  auszulösen – ist aber aufgrund von 4.2 kein Muss, sondern nur eine
  Optimierung, um möglichst aktuelle Werte zu behalten

## 7. Netzwerk & Weboberfläche

- ESP32-S3 startet einen eigenen **WLAN Access Point (SoftAP)** mit
  konfigurierbarem SSID/Passwort; kein Internetzugriff nötig
- Optional: Captive-Portal-Redirect, damit sich Smartphones automatisch
  öffnen, sobald sie sich verbinden
- **HTTP-Server** (`esp_http_server`) liefert die statische Web-UI
  (HTML/CSS/JS werden **im Firmware-Image** als `const char[]` eingebettet,
  nicht auf einem separaten Dateisystem wie LittleFS/SPIFFS abgelegt) – das
  vermeidet ein zusätzliches, potenziell korrumpierbares Dateisystem für
  reine UI-Assets; nur die eigentlichen Messwerte/Konfiguration liegen in
  NVS
- **Live-Daten-Push:** WebSocket-Endpoint (vom `esp_http_server` unterstützt)
  für Screen 1 (aktueller Wert, ~5–10 Hz) und Screen 2 (Kurvenpunkte); REST-
  Endpoints (`GET /api/stats`, `POST /api/reset`, `GET/POST /api/config`)
  für Statistik-Abruf, Reset und Konfiguration

## 8. Screens

### 8.1 Screen 1 – Live-Wert

- **Zeiger-/Bogenanzeige** (SVG, analog Tacho) über den Gemisch-Index
  (-100…+100), farblich in Zonen **mager = rot / Zielbereich (λ = 1) = grün
  / fett = blau** unterteilt – auf einen Blick während der Fahrt ablesbar
  (dieselbe Farbzuordnung wird konsistent auch im Zeitverlauf-Screen (8.2)
  und in der Prozent-Balkenanzeige unten verwendet)
- **5-s-Mittelwert** als zweiter, dezenterer Zeiger/Marker auf derselben
  Skala (direkter visueller Vergleich Momentanwert vs. Trend)
- **Zusätzlich vorgeschlagene Elemente** für diesen Screen:
  - Statusanzeige "Sonde bereit / nicht bereit" (siehe 3.2)
  - Aktuelle Sprungfrequenz [Schaltungen/min] als Regelkreis-Indikator (3.3)
  - Session-Statistik: Zeitanteile **Aufwärmphase/mager/λ=1/fett** als
    gestapelter Prozent-Balken, Min/Max seit Einschalten
  - Langzeit-Statistik (persistiert): dieselben Kennzahlen kumulativ, plus
    Gesamtlaufzeit, mit "Zurücksetzen"-Button (4.3)
  - **Darstellung als Prozent-Balken (Umsetzungshinweis):** Die
    Datenstruktur speichert weiterhin ausschließlich Sekundenwerte (siehe
    4.4, inkl. `t_warmup_s`) – die Umrechnung in Prozent
    (`anteil = t_x_s / (t_warmup_s + t_mager_s + t_lambda1_s + t_fett_s) * 100`)
    erfolgt **ausschließlich im Frontend** (JavaScript), nicht auf dem
    ESP32. Der Balken wird als vierteiliger, gestapelter Balken (100 % =
    Gesamtlaufzeit) gerendert: Aufwärmphase (grau) / mager (rot) / λ = 1
    (grün) / fett (blau) – gleiche Farbcodierung wie die Zeigeranzeige
  - WLAN-/Verbindungsstatus, Uptime
  - Warnhinweis, falls Signal ungewöhnlich lange in "sehr mager"/"sehr
    fett" hängt (möglicher Sondendefekt oder Motorproblem)

### 8.2 Screen 2 – Zeitlicher Verlauf ("Oszilloskop")

- Liniendiagramm (z. B. Canvas-basiert, `uPlot` – leichtgewichtig genug für
  Live-Rendering im Browser) des Gemisch-Index über die Zeit
- Zeitfenster wählbar (z. B. 10 s / 30 s / 60 s), gespeist aus dem
  RAM-Ringpuffer (5.1)
- Horizontale Referenzlinie bei "λ = 1" (grün) sowie Markierung des
  `deadband`-Bereichs; Kurve/Hintergrund optional in den Zonenfarben aus
  8.1 (mager = rot, λ = 1 = grün, fett = blau) eingefärbt
- Pause/Freeze-Button, um einen Moment zur genaueren Analyse festzuhalten

### 8.3 Navigation

Einfache Tab-/Button-Navigation zwischen Screen 1 und 2 auf derselben
Single-Page-Web-App (kein Reload nötig, WebSocket-Verbindung bleibt aktiv).

### 8.4 Konfigurationsseite (implizit gefordert durch "parametrierbar")

Dritter, einfacher Bereich (z. B. Tab "Einstellungen"):

- ADC-Pin-Auswahl
- Spannungs-Kalibrierpunkte `u_min` / `u_max` / `u_lambda1` / `deadband`
  (manuell **oder** über den Kalibrierassistenten, siehe 8.5)
- Kategorie-Schwellen (3.1)
- Mittelwert-Fensterlängen (schnell/langsam)
- Aufwärm-Timeout `t_warmup` (3.2)
- WLAN-SSID/Passwort des Access Points
- Speichern schreibt sofort (nicht zyklisch) einen NVS-Commit, da
  Konfigurationsänderungen selten und bewusst ausgelöst sind

### 8.5 Kalibrierassistent (fester Bestandteil der Konfigurationsseite)

Statt nur manueller Zahleneingabe wird direkt ein geführter Assistent
vorgesehen, mit zwei Modi:

**a) Geführte manuelle Kalibrierung (Basisvariante, immer verfügbar):**
Der Assistent zeigt den aktuellen Live-Spannungswert an und bietet drei
Buttons:
1. "Motor im Leerlauf bei stabilem Gemisch (λ ≈ 1) laufen lassen →
   *Aktuellen Wert als λ=1-Punkt übernehmen*"
2. "Extremwert **mager** anfahren (z. B. kurzzeitig Nebenluft/Schub) →
   *als `u_min` übernehmen*"
3. "Extremwert **fett** anfahren (z. B. kurz Choke/Volllast-Anreicherung) →
   *als `u_max` übernehmen*"

Jeder Button übernimmt den aktuell gefilterten (5-s-Mittelwert-)Messwert
in das jeweilige Kalibrierfeld; `deadband` wird daraus mit einem
Standard-Vorschlag (z. B. 10 % des Abstands zu `u_min`/`u_max`) vorbelegt
und bleibt zusätzlich manuell änderbar.

**b) Automatische Kalibrierung ("Auto-Kalibrieren"-Button):** Bei
laufendem Motor im geschlossenen Regelkreis (Closed-Loop-Betrieb)
oszilliert die Sprungsonde ohnehin ständig zwischen ihren beiden
Extremwerten. Der Assistent beobachtet das Signal für ein konfigurierbares
Zeitfenster (Default 30 s) und leitet daraus automatisch ab:
- `u_min` ≈ 5.-Perzentil der beobachteten Werte
- `u_max` ≈ 95.-Perzentil der beobachteten Werte
- `u_lambda1` ≈ Mittelpunkt zwischen den beiden häufigsten Umschaltflanken
- `deadband` ≈ Streuung der Umschaltpunkte um `u_lambda1`

Die automatisch ermittelten Werte werden vor dem Übernehmen zur Kontrolle
angezeigt (nicht automatisch gespeichert), damit offensichtliche
Fehlmessungen (z. B. Motor stand während der Messung still) manuell
korrigiert oder verworfen werden können.

### 8.6 OTA-Firmware-Update über die Weboberfläche

Ein direktes Firmware-Update-Feature wird als fester Bestandteil der
Konfigurationsseite vorgesehen (kein separates Ausblick-Thema):

- Eigener Reiter "Firmware-Update" mit Datei-Upload (`.bin`) per
  `POST /api/ota`, Fortschrittsanzeige während des Uploads
- Umsetzung über die ESP-IDF-eigene OTA-Funktionalität
  (`esp_https_ota`/`esp_ota_ops`) mit **zwei OTA-App-Partitionen**
  (`ota_0`/`ota_1`) plus `otadata` in der Partitionstabelle, sodass immer
  ein funktionierendes Vorgängerimage erhalten bleibt
- **Rollback-Schutz:** neues Image wird nach dem Neustart zunächst als
  "pending verify" markiert; erst nach einem erfolgreichen
  Selbsttest (Boot abgeschlossen, WLAN-AP + HTTP-Server laufen,
  Konfiguration/NVS lesbar) wird es über
  `esp_ota_mark_app_valid_cancel_rollback()` als gültig bestätigt.
  Schlägt der Selbsttest fehl oder bootet das neue Image gar nicht
  sauber durch, fällt der ESP32-S3 beim nächsten Start automatisch auf
  das vorherige, funktionierende Image zurück (ESP-IDF-Bootloader-Feature)
- Da der Access Point ohnehin nur lokal (kein Internet) erreichbar ist,
  wird zusätzlich ein einfacher Schutz gegen versehentliches/fremdes
  Flashen vorgesehen: Bestätigungsdialog im Frontend + optionale
  Passwortabfrage vor dem Upload
- Persistierte Statistik/Konfiguration im NVS bleibt von einem
  OTA-Update unberührt (separate Partitionen), ein Update setzt also
  weder Langzeit-Statistik noch Kalibrierung zurück

## 9. Softwarearchitektur (Module/Tasks)

FreeRTOS-Tasks mit klarer Verantwortlichkeit, Kommunikation über Queues:

```
[ADC-Sampling-Task] --(raw mV)--> [Filter/Interpreter] --(Index, Kategorie)-->
    ├──> [Statistik-Task] --(zyklisch)--> [NVS-Persistenz]
    ├──> [RAM-Ringpuffer] --(für Screen 2)
    └──> [WebSocket-Broadcast-Task] --> Web-Clients
[HTTP/WS-Server-Task] <--(REST: /api/stats, /api/reset, /api/config)--> [Config/Stats-Module]
[HTTP-Server-Task] <--(POST /api/ota, Firmware-Binary)--> [OTA-Task] --> [esp_ota_ops, OTA-Partitionen]
```

- **Signalinterpreter** als austauschbares Modul (Interface), damit später
  z. B. eine Breitbandsonden-Variante mit echter λ-Berechnung ergänzt
  werden kann, ohne den Rest der App anzufassen (siehe 3.1)
- Klare Trennung: Erfassung/Interpretation (harte Echtzeit-Anforderungen,
  kleine Latenz) vs. Persistenz/Web (unkritischer bzgl. Timing)

## 10. Technologie-Stack (Vorschlag)

| Bereich | Wahl | Begründung |
|---|---|---|
| Framework | ESP-IDF (nativ, FreeRTOS) statt Arduino-Core | direkter Zugriff auf ADC-Kalibrierung, NVS, `esp_http_server` inkl. WebSocket, geringerer Overhead |
| Persistenz | NVS (`nvs_flash`) | Wear-Leveling & Power-Loss-Safety eingebaut (siehe 4.2) |
| Webserver | `esp_http_server` mit WS-Support | keine Zusatzbibliothek nötig, gut getestet, asynchron |
| Frontend | Vanilla HTML/CSS/JS + `uPlot` (Chart) + Inline-SVG (Zeiger) | keine Build-Pipeline nötig, alles in Firmware einbettbar, klein genug für Flash |
| Datenformat API | JSON (`cJSON`, in ESP-IDF enthalten) | einfache Interoperabilität mit Browser |
| OTA | `esp_https_ota`/`esp_ota_ops`, Partitionstabelle mit `ota_0`/`ota_1`/`otadata` | eingebauter Rollback-Schutz (siehe 8.6), kein Custom-Bootloader-Code nötig |
| Ziel-Hardware | Seeed Studio **XIAO ESP32-S3** + LED Driver Board | S3 statt C3: mehr ADC1-Pins (GPIO1–10), bewährtes Bring-up-Board mit Eingangs-Spannungswandler (siehe 2.1) |

## 11. Offene Punkte / spätere Ausbaustufen

- Export der Langzeit-Statistik (CSV-Download über HTTP)
- mDNS (`lambda.local`) statt fester IP im Hotspot
- Mehrere Client-Verbindungen gleichzeitig (WebSocket-Broadcast ist dafür
  bereits vorgesehen)
- Physische Verifikation der LED-Driver-Board-Pinbelegung anhand des
  Schaltplans (siehe Hinweis in 2.1) und ggf. Anpassung der
  Pin-Empfehlung
