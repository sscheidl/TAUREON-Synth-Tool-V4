# TAUREON 2 – GUI Concept & Clickable Prototype

**Status:** GUI-Vorplanung / klickbarer Workflow-Prototyp  
**Datum:** 2026-08-23  
**Projekt:** TAUREON 2 Clean C++20 / Qt 6 Rebuild  
**Zweck:** Workflow, Informationshierarchie und Optik testen, bevor die eigentliche Qt-GUI in Stage 5 implementiert wird.

---

## 1. Grundidee

Der aktuelle Prototyp ist **keine Implementierung der späteren GUI** und enthält **keine MIDI-, Datei- oder Backend-Verdrahtung**.

Er dient ausschließlich dazu:

- Navigation und Hauptbereiche zu testen;
- die Informationsdichte auf 1920×1080 einzuschätzen;
- typische Workflows vorab durchzuklicken;
- die Aufteilung der Funktionen zwischen Tabs zu prüfen;
- Bedienfehler und unnötige Komplexität früh zu erkennen;
- Codex später für Stage 5 ein klareres GUI-Zielbild zu geben.

Die technische TAUREON-2-Architektur bleibt davon unberührt.

---

## 2. Gestaltungsprinzipien

Der Prototyp folgt den Vorgaben des TAUREON-2-Rebuilds:

- professionelle Desktop-Anwendung statt dekorativem Synth-Skin;
- dauerhafte Connection Bar;
- klare funktionale Trennung der Arbeitsbereiche;
- große Tabellen statt vieler kleiner Panels;
- keine winzigen Fonts;
- High-DPI-taugliche Struktur;
- Status nicht ausschließlich über Farbe;
- Rohdaten bleiben sichtbar;
- Gerätewissen gehört in Profile, nicht in den Transport;
- MIDI-/SysEx-Kern bleibt unabhängig von der GUI.

Visuell verwendet das Mockup:

- dunkle neutrale Grundfläche;
- sparsame goldene/amberfarbene τAUREON-Akzente;
- Windows-/Qt-artige Controls;
- klare Tabellen und Status-Badges;
- möglichst wenig dekorative Elemente.

---

## 3. Permanente Connection Bar

Die Connection Bar bleibt unabhängig vom gewählten Tab immer sichtbar.

Aktuell vorgesehen:

- Backend:
  - Auto
  - Windows MIDI Services
  - WinMM
- Connection State
- MIDI Input
- MIDI Output
- Connect / Disconnect
- Panic

Der Prototyp simuliert Zustandsänderungen nur visuell.

Später können bei Bedarf WMS-Gruppen bzw. Function-Block-Auswahl ergänzt werden, sobald die Transport- und Endpoint-Struktur aus Stage 1–2 feststeht.

---

## 4. Hauptbereiche

### 4.1 MIDI Monitor

Der MIDI Monitor ist als großer tabellarischer Arbeitsbereich geplant.

Aktuelle Spalten:

- Time
- Direction
- Port
- Channel
- Type
- Parameter / Event
- Value
- Raw

Oben befinden sich:

- MIDI Profile
- Edit JSON
- Profilstatus
- Channel Filter
- Notes
- CC
- NRPN/RPN
- SysEx
- Clock
- Pause
- Clear
- Export

Grundprinzip:

> Ein ausgewähltes Device Profile darf MIDI-Daten nur interpretieren und verständlicher benennen. Die Rohinformation bleibt erhalten.

Beispiele:

- CC → verständlicher Parametername;
- CC-Paar → 14-Bit-Parameter;
- NRPN/RPN → Profilname + Wert;
- unbekannter Parameter → rohe Controller-/NRPN-Nummer.

Das eigentliche Profilmodell wird erst in Stage 4 implementiert; die GUI-Anbindung folgt in Stage 5.

---

### 4.2 SysEx Transfer

Dieser Bereich ist für den eigentlichen Empfangs-/Sende-Workflow einer Datei vorgesehen.

Aktueller Prototyp:

- geladene Datei;
- Hersteller;
- Device Profile;
- Framezahl;
- Gesamtgröße;
- Framing-Status;
- File Open;
- Frame Inspector;
- Output Route;
- Profile;
- Pacing;
- Receive;
- Send;
- Cancel;
- Fortschrittsanzeige;
- Transfer Log.

Die derzeitige Send-/Receive-Funktion ist nur eine Animation.

Später gilt weiterhin:

- keine automatische Payload-Reparatur;
- bytegenaue Behandlung;
- Pacing gehört in Transfer Engine / Device Profile;
- kein automatischer Hardware-Send;
- destruktive Aktionen müssen explizit bestätigt werden.

---

### 4.3 SysEx Manager

Zusätzlich zum reinen Transfer-Tab wurde ein eigener Bereich zur **SysEx-Verwaltung** ergänzt.

Ziel:
Mehrere SysEx-Dateien unabhängig von einem aktiven Transfer verwalten, untersuchen und vorbereiten.

Aktuelle Dateiansicht:

- File
- Device
- Frames
- Size
- Status

Aktuelle Einzeldatei-Aktionen:

- Inspect Frames
- Open in Transfer
- Split into Frames
- Merge Selected
- Compare Files
- Duplicate
- Rename
- Export Copy

Aktuelle Collection-/Batch-Ideen:

- Scan / Identify
- Calculate Hashes
- Find Duplicates
- Validate Framing
- Sort by Device
- Create Collection

Diese Funktionen sind derzeit ausschließlich klickbare UI-Platzhalter.

### Abgrenzung

**SysEx Manager**
= Dateien organisieren, prüfen, vergleichen und vorbereiten.

**SysEx Transfer**
= eine konkrete Datei empfangen oder an ein ausgewähltes Gerät senden.

Diese Trennung erscheint für den Workflow sinnvoller als alle SysEx-Funktionen auf einem einzigen Bildschirm unterzubringen.

---

## 5. Library / Files

Der erste Entwurf bleibt bewusst klein.

Aktuelle Tabelle:

- Name
- Device
- Type
- Frames
- Size
- Modified

V1 soll daraus noch keinen vollständigen universellen Synth-Librarian machen.

Eine umfangreiche Preset-/Library-Verwaltung kann später auf dieser Struktur aufbauen, ohne den stabilen MIDI-/SysEx-Kern zu belasten.

---

## 6. Device Profiles

Device Profiles bilden die datengetriebene Geräteebene.

Aktueller Prototyp trennt:

- Generated Profiles
- User Profiles

Profilansicht zeigt beispielhaft:

- Manufacturer
- Model
- Profile Status
- Profile Type
- Source
- Firmware Scope
- Auto Detect Hinweise
- Control Model

Aktionen:

- Edit JSON
- Duplicate
- Validate
- Open Folder

### Edit JSON

Der Prototyp enthält einen einfachen JSON-Editor-Dialog mit:

- Raw JSON Text
- Pretty Print
- Validate
- Save User Copy
- Close

Konzept:

- recherchierte/generated Profile bleiben unangetastet;
- manuelle Korrekturen werden als User Profile gespeichert;
- ungültiges JSON soll später nicht stillschweigend gespeichert werden;
- jedes Profil soll eine kurze `profile_info`-Sektion enthalten.

---

## 7. Diagnostics

Der Diagnostics-Tab soll später die technische Wahrheit über den aktuellen Zustand zeigen.

Im Mockup enthalten:

- Application Version
- Build
- Platform
- Backend
- Connection
- Input
- Output
- Stable Identity
- Group
- Protocol
- RX Events
- TX Events
- Dropped Events
- Queue / Activity Placeholder
- Export Diagnostic Bundle

Der Prototyp zeigt nur simulierte Werte.

---

## 8. Settings

Der Mockup-Stand enthält erste Gruppierungen für:

### General
- Theme
- Default Backend
- Monitor History

### MIDI Monitor
- Auto-select Device Profile
- Active Sensing Default
- MIDI Clock Default
- Raw Bytes sichtbar

### SysEx Defaults
- Pacing
- Send Confirmation

Noch keine Einstellungen werden tatsächlich gespeichert.

---

## 9. Aktueller Klickumfang

Der HTML-Prototyp simuliert bereits:

- Tab-Navigation;
- Connect / Disconnect;
- MIDI Input / Output Wechsel;
- Backend-Auswahl;
- Monitor-Profilwechsel;
- Pause / Resume;
- Clear;
- Edit JSON Modal;
- JSON Validate;
- Pretty Print;
- Save User Copy;
- Device-Profile-Auswahl;
- Profile Validate / Duplicate;
- simulierten SysEx-Sendefortschritt;
- Cancel;
- SysEx-Manager-Aktionen;
- Open in Transfer;
- Toast-/Statusmeldungen.

Es werden **keine echten MIDI-Daten gesendet**, keine echten Dateien verändert und keine Systemressourcen geöffnet.

---

## 10. Verhältnis zum TAUREON-2-Stufenplan

Der Prototyp läuft bewusst parallel zur Implementierung, ohne die technische Reihenfolge zu verändern.

### Stage 0–3
Noch keine echte GUI-Abhängigkeit.

### Stage 4
Device/Profile-Datenmodell und erste Profile.

### Stage 5
Qt-6-GUI auf Basis des stabilen Core und des getesteten Bedienkonzepts.

Der HTML-Prototyp ist daher als **Designreferenz**, nicht als zu portierender Quellcode zu behandeln.

Codex soll die spätere Qt-GUI nicht aus HTML/CSS übersetzen, sondern die bestätigten Workflows und Strukturen nativ mit Qt 6 Widgets / Model-View umsetzen.

---

## 11. Noch offen / nächste GUI-Entscheidungen

Vor Stage 5 können wir iterativ testen:

- Ist die Connection Bar in dieser Form sinnvoll?
- Braucht SysEx Manager einen eigenen Tab oder soll er langfristig mit Library verschmelzen?
- Welche Aktionen gehören wirklich in SysEx Manager?
- Wie viele Informationen sollen Device Profiles auf einen Blick zeigen?
- Ist MIDI Monitor bei 1920×1080 ausreichend lesbar?
- Welche Filter gehören direkt in den Monitor und welche in ein Filter-Popup?
- Braucht der Transfer-Tab getrennte Receive- und Send-Ansichten?
- Welche Statusinformationen sind permanent nötig?
- Welche Einstellungen sollten aus Settings direkt in den jeweiligen Arbeitsbereich wandern?
- Soll Diagnostics stärker technisch oder stärker nutzerorientiert aufgebaut werden?

---

## 12. Dateien

### Klickbarer Prototyp

`TAUREON2_GUI_Clickable_Mockup.html`

Direkt im Browser öffnen.  
Keine Installation erforderlich.

### Dieses Dokument

`TAUREON2_GUI_CONCEPT_STATUS.md`

Dient als kompakte Design- und Workflow-Zusammenfassung für den Projektordner.

---

## 13. Wichtigste Leitlinie

> Der Prototyp soll uns helfen, den Workflow zu verbessern, bevor echter GUI-Code geschrieben wird. Er darf die technische Architektur nicht vorwegnehmen oder Codex dazu verleiten, HTML/CSS als Implementierungsbasis zu übernehmen.

