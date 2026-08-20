# Konzept: Lambda-Monitor für ESP32-C3

Dieses Dokument beschreibt das technische Konzept für eine ESP32-C3-Anwendung,
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

- ADC-Pin ist konfigurierbar (z. B. `GPIO0`/`GPIO1`/`GPIO2`/`GPIO3`, die
  ADC1-fähigen Pins des ESP32-C3), per Konfigurationsseite oder
  Kompilierzeit-Konstante wählbar
- ESP32-C3 hat nur **ADC1** (kein ADC2) und einen 12-bit SAR-ADC (~0–3,3 V
  mit Dämpfung/Attenuation) → passt gut zum extern auf 0–3 V verstärkten Signal
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

Sprungsonden liefern erst ab ca. 300 °C ein gültiges Signal. Ohne
Temperaturfühler kann dies nicht direkt gemessen werden, aber ein
Plausibilitäts-Kriterium wird vorgeschlagen: Solange das Signal für eine
Mindestzeit (z. B. 10 s) dauerhaft in einem sehr engen Band um eine feste
Spannung "klebt" (keine Schaltaktivität, siehe 3.3), wird der Status
**"Sonde nicht bereit / kein Signal"** angezeigt, statt einen (falschen)
Gemisch-Index vorzugaukeln.

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
- **Akkumulatoren** für die Statistik: Zeit "mager"/"λ=1"/"fett" in Sekunden
  (`uint32_t`, reicht > 100 Jahre bei 1 Hz Inkrement → praktisch kein
  Overflow-Risiko), Min/Max des Gemisch-Index inkl. Zeitstempel,
  Sprungfrequenz (gleitender Mittelwert)
- Es werden **zwei Ebenen** von Statistik geführt:
  - **Session-Statistik**: seit dem letzten Einschalten (KL15 an)
  - **Langzeit-Statistik**: kumulativ über alle Sessions (persistiert)

### 4.2 Persistenz im Flash (NVS statt "EEPROM")

Der ESP32-C3 hat kein echtes EEPROM, sondern nutzt für sowas den
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
  **Brown-Out-Detektors** des ESP32-C3, um bei erkanntem
  Spannungseinbruch einen sofortigen, außerplanmäßigen NVS-Commit
  auszulösen – ist aber aufgrund von 4.2 kein Muss, sondern nur eine
  Optimierung, um möglichst aktuelle Werte zu behalten

## 7. Netzwerk & Weboberfläche

- ESP32-C3 startet einen eigenen **WLAN Access Point (SoftAP)** mit
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
  (-100…+100), farblich in Zonen mager (blau) / Zielbereich (grün) / fett
  (rot) unterteilt – auf einen Blick während der Fahrt ablesbar
- **5-s-Mittelwert** als zweiter, dezenterer Zeiger/Marker auf derselben
  Skala (direkter visueller Vergleich Momentanwert vs. Trend)
- **Zusätzlich vorgeschlagene Elemente** für diesen Screen:
  - Statusanzeige "Sonde bereit / nicht bereit" (siehe 3.2)
  - Aktuelle Sprungfrequenz [Schaltungen/min] als Regelkreis-Indikator (3.3)
  - Session-Statistik: Zeitanteile mager/λ=1/fett als Balken oder
    Prozentangabe, Min/Max seit Einschalten
  - Langzeit-Statistik (persistiert): dieselben Kennzahlen kumulativ, plus
    Gesamtlaufzeit, mit "Zurücksetzen"-Button (4.3)
  - WLAN-/Verbindungsstatus, Uptime
  - Warnhinweis, falls Signal ungewöhnlich lange in "sehr mager"/"sehr
    fett" hängt (möglicher Sondendefekt oder Motorproblem)

### 8.2 Screen 2 – Zeitlicher Verlauf ("Oszilloskop")

- Liniendiagramm (z. B. Canvas-basiert, `uPlot` – leichtgewichtig genug für
  Live-Rendering im Browser) des Gemisch-Index über die Zeit
- Zeitfenster wählbar (z. B. 10 s / 30 s / 60 s), gespeist aus dem
  RAM-Ringpuffer (5.1)
- Horizontale Referenzlinie bei "λ = 1" sowie Markierung des
  `deadband`-Bereichs
- Pause/Freeze-Button, um einen Moment zur genaueren Analyse festzuhalten

### 8.3 Navigation

Einfache Tab-/Button-Navigation zwischen Screen 1 und 2 auf derselben
Single-Page-Web-App (kein Reload nötig, WebSocket-Verbindung bleibt aktiv).

### 8.4 Konfigurationsseite (implizit gefordert durch "parametrierbar")

Dritter, einfacher Bereich (z. B. Tab "Einstellungen"):

- ADC-Pin-Auswahl
- Spannungs-Kalibrierpunkte `u_min` / `u_max` / `u_lambda1` / `deadband`
- Kategorie-Schwellen (3.1)
- Mittelwert-Fensterlängen (schnell/langsam)
- WLAN-SSID/Passwort des Access Points
- Speichern schreibt sofort (nicht zyklisch) einen NVS-Commit, da
  Konfigurationsänderungen selten und bewusst ausgelöst sind

## 9. Softwarearchitektur (Module/Tasks)

FreeRTOS-Tasks mit klarer Verantwortlichkeit, Kommunikation über Queues:

```
[ADC-Sampling-Task] --(raw mV)--> [Filter/Interpreter] --(Index, Kategorie)-->
    ├──> [Statistik-Task] --(zyklisch)--> [NVS-Persistenz]
    ├──> [RAM-Ringpuffer] --(für Screen 2)
    └──> [WebSocket-Broadcast-Task] --> Web-Clients
[HTTP/WS-Server-Task] <--(REST: /api/stats, /api/reset, /api/config)--> [Config/Stats-Module]
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

## 11. Offene Punkte / spätere Ausbaustufen

- Kalibrier-Assistent (Sprungpunkt automatisch aus beobachtetem Signal
  vorschlagen)
- Export der Langzeit-Statistik (CSV-Download über HTTP)
- mDNS (`lambda.local`) statt fester IP im Hotspot
- Mehrere Client-Verbindungen gleichzeitig (WebSocket-Broadcast ist dafür
  bereits vorgesehen)
- Optionale OTA-Update-Funktion über dieselbe Weboberfläche
