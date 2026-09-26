# TAUREON V4 — Rekonstruktion und sichere Verifikation, 26.09.2026

**Empfehlung: PASS für die Fortsetzung des eng begrenzten Block-B-Abschlusses; HOLD für Merge, Stage-5-Abnahme, Stage 6 und physische SysEx-Sends.** Die vorhandenen lokalen P1-Korrekturen bauen und bestehen erneut. Sie sind noch uncommittet, nicht unabhängig nachgeprüft und in den kanonischen Statusdokumenten nicht abgebildet. Dieser Bericht ist ein Befund, keine Änderung des Projekt-Gates.

## 1. Kanonischer Stand und Git-Abgleich

| Ebene | Tatsächlich verifizierter Stand | Konsequenz |
|---|---|---|
| V4 `main`, lokal und live auf GitHub | `d04e99c08d9d7f4134b7040a3e8abba9654045eb`, Merge PR #11 vom 02.09. | Kanonischer integrierter Stand; Block A und Ferienabschluss enthalten |
| Aktiver lokaler Branch | `codex/stage5-block-b-windows-evidence`, HEAD `7dc09c9bd241cb773b25b41402d1aea892926d3f` | Ein Commit vor `main`; identisch mit dem live geprüften Remote-Branch und PR #12, zusätzlich sechs lokale Änderungen |
| [PR #12](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/pull/12) | Offen, `mergeable=true`, Basis `d04e99c`, drei ungelöste P1-Reviewthreads | Konfliktfrei ist keine fachliche Mergefreigabe |
| [PR #13](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/pull/13) | Offen, `mergeable=true`, HEAD `cdc38f6489d6067df6145a45e44c6e7950cc5cf1`, Basis PR-12-Branch `7dc09c9` | Zwei zusätzliche Commits: Revisionsanzeige und internes Windows-Testpaket; ein ungelöster P2 zur exakten Buildrevision |
| `claude/reference-archive-v1` | Remote-HEAD `69dadd3a51c30deba5216e20016d9bffd88c11de`, zwei Commits nach `main` | Separat erhaltene, noch nicht integrierte Arbeit; kein zugehöriger PR gefunden |
| `claude/stage5-gate-cleanup` | Remote-HEAD `2c6b2cfd65782c3e87931a0740e91bb4d3bc131a` | Nicht in `main`-Ancestry, aber N-1/N-2/N-3/S7-3/S7-4 inzwischen durch Block A erledigt; nicht blind nachziehen |

V4 hat genau ein Remote (`origin`, privates `sscheidl/TAUREON-Synth-Tool-V4`), zwei lokale Branches, fünf Remote-Branchköpfe, keine Tags, keine Stashes und nur den Haupt-Worktree. `git ls-remote` stimmt mit allen vorhandenen Remote-Zeigern überein; ein Fetch war dafür nicht nötig. PR #1 bis #11 sind gemergt; #12 und #13 offen. Keine shallow history. `git fsck --full --no-reflogs --unreachable` findet acht alte Dokument-Blobs, aber keinen unerreichbaren Commit. Die Blobs betreffen Stage-1–3-Berichte und ADRs; sie wurden nicht gelöscht oder als neue Entwicklungsarbeit interpretiert.

Die GitHub-Evidenz von PR #12 wurde anhand des Runs **34393355315 / Windows CI #251**, `head_sha=7dc09c9...`, und der tatsächlichen Logs nachgeprüft: Configure/Debug-Build erfolgreich, **24/24 Tests, 0 fehlgeschlagen**, 8,15 s, keine registrierten `local-midi`-Tests. Das ist historische Windows-CI-Evidenz des veröffentlichten HEADs, keine Prüfung des heutigen Dirty-Worktrees und keine Hardwareevidenz. Siehe [Run](https://github.com/sscheidl/TAUREON-Synth-Tool-V4/actions/runs/34393355315), `pr12-ci.log` und `pr12-ci-run.json`.

## 2. Mobile, Claude- und WORK-Spuren

Im ChatGPT-Projekt **tAUREON Synth Tool** wurden die zugänglichen Aufgaben gelesen:

- [Cloudbasierte Weiterentwicklung](https://chatgpt.com/c/6a913139-9550-83ed-9efa-c14a71756feb): Cloud-CI-Umstellung und frühe Claude-Reviews; die tatsächlichen jüngsten Nachrichten dieser Aufgabe betreffen PR #2, trotz eines späteren App-Aktualisierungsdatums.
- [Stage-5 Follow-up CI](https://chatgpt.com/c/6a96c0e8-0ae0-83eb-8e92-581dc4ce5896): eng begrenzter Claude-Delta-Reviewauftrag für PR #8 bei `cdd72d9...`, Schwerpunkt F-4; spätere Bestätigung der Merges #8/#9.
- [Stage 6 Block B Status](https://chatgpt.com/c/6a97ad88-52f8-83eb-b86e-7b4c5bb50db4): Titel irreführend; Inhalt stellt ausdrücklich klar, dass Block B zu Stage 5 gehört. Enthält den lokalen 09.09.-Handoff und das zugängliche Dokument `TAUREON_V4_STAGE5_HANDOFF_2026-09-02.md`.
- [EXE für Hardwaretests](https://chatgpt.com/c/6aa26051-4b94-83ed-a595-da8fde77da6e): Auftrag und Bericht zum internen ZIP, das über PR #13 vorbereitet wurde. Der dortige historische Download ist keine heute erneut verifizierte Binärdatei.
- Codex-Aufgabe **Validate Stage 5 Block-B**, ID `01a0925a-ae6e-74f2-bd9e-db71f01edada`: entscheidende lokale Fortsetzung vom 11./12.09.; P1-Korrekturen, Testverkürzung und Kontingentabbruch rekonstruiert.

Die zugängliche ChatGPT-Archivliste enthält keinen weiteren einschlägigen Treffer. Lokale Claude-Projektordner liefern keinen TAUREON-Chat. Die App-Liste ist auf 50 jüngere Aufgaben begrenzt; daher wird keine Vollständigkeit aller mobilen oder Claude-Cloud-Gespräche behauptet. Das ältere lokale Validierungspaket ist im Chat nur als damaliger Sandbox-Link erwähnt, sein Inhalt war dort nicht beigefügt. Eine zusätzliche darin enthaltene Geräte-/Befehlsfreigabe kann deshalb nicht bestätigt werden.

**Letzter belastbar überlieferter abgeschlossener Claude-Review:** Der PM-Handoff vom 02.09. dokumentiert den umfassenden PR-#7-Review als `PASS WITH NON-BLOCKING FOLLOW-UPS`; F-1 bis F-5 wurden anschließend über PR #8 erledigt. Derselbe Handoff sagt ausdrücklich, dass der kurze PR-#8-Delta-Review durch die spätere PO-Mergeentscheidung nicht erneut gefordert wurde. Ein neuerer abgeschlossener Claude-Review für #12/#13 wurde nicht gefunden. Die drei P1-Kommentare zu #12 und der P2 zu #13 stammen nachweislich von **chatgpt-codex-connector**, nicht von Claude.

**Neueste Claude-Implementierung:** 03.09., `df40483` und `69dadd3`, Referenzarchiv mit Schema, Summit-Referenzeintrag, eigenem Test-JSON-Helfer und Tests, insgesamt sieben Dateien / 1.016 hinzugefügte Zeilen. Das ist keine Hardwarefreigabe und kein Review. Der ältere Cleanup-Branch enthält ausdrücklich die Erklärung, dass Claude wegen der Android-only-Situation auf PO-Auftrag implementiert hat und deshalb kein unabhängiger Reviewer seiner eigenen Änderungen ist.

**Integrationsoptionen:** Zuerst PR #12 fachlich abschließen; anschließend #13 einschließlich P2 überarbeiten und gegen den dann gültigen Basestand prüfen. Das Referenzarchiv separat auf Scope, Testhelfer, Provenienz und Ausschluss aus Auslieferung prüfen. Den alten Cleanup-Branch nur als Referenz behalten; seine veralteten Offen-Listen dürfen den abgeschlossenen Block A nicht wieder öffnen. Keine dieser Optionen wurde umgesetzt oder durch einen Probemerge verändert.

## 3. Lokale Änderungen und exakt nächster Schritt

Der Worktree enthält unverändert **121 hinzugefügte / 17 entfernte Zeilen in sechs Dateien**:

| Datei | Bereits begonnene Korrektur |
|---|---|
| `src/transports/IMidiTransport.hpp` | Diagnosefeld für beobachtetes WMS-Worker-MTA |
| `src/transports/wms/WmsTransport.cpp` | Laufzeitbeobachtung und Prüfung des MTA mit `CoGetApartmentType` |
| `src/transports/wms/CMakeLists.txt` | COM-Linkabhängigkeit |
| `tests/integration/Stage5ProductHostLocal.cpp` | Reale Loopback-RX-Aktivität vor Close, erfolgreiche Close-Aufrufe und final `closed`, GUI-Apartment; getrennte Backend-Prozesse; Handle-Gate nur im expliziten 100-Zyklen-Soak |
| `tests/integration/CMakeLists.txt` | Regulärer Product-Host-Test mit fünf Zyklen; Stage-2-WMS-Timeout 120 → 180 s |
| `tools/RunStage5ProductHostLoopback.ps1` | WMS und WinMM in getrennten Testprozessen |

Im rekonstruierten lokalen Auftrag autorisierte der PO die Verkürzung ausdrücklich. Der fünf-Zyklen-Test bestand schon am 12.09. in 33 s; sein Original-Log wurde aus dem alten untracked Build gerettet (`recovered-product-host-20260912.log`). Der damalige Gesamtlauf war wegen eines Stage-2-Timeouts **kein** vollständiger neuer 6/6-Erfolg. Ein langer separater Soak bestand laut Aufgabenverlauf; daraus wird hier keine neue Langzeitfreigabe abgeleitet. Die heute geprüfte Kurzvariante setzt `handle_growth_gate_applied=false` und kann keinen Langzeit-Leaknachweis ersetzen.

**Schutz:** `local-changes-before.patch` ist eine explizite Sicherung; `local-source-hashes-before.json` enthält die SHA-256 aller sechs Dateien. Nach den Tests waren alle sechs byteidentisch. Index unverändert/leer; kein Commit, Checkout, Stash, Reset, Rebase, Merge oder Push. Das bereits vorhandene `.work/stage5-p1-build` und ältere ignorierte Build-/Dependency-Bestände bleiben unangetastet. Die eigens erzeugte neue Build-/Preview-Struktur ist zur Entfernung vorgesehen; deren Löschung wurde von der automatischen Freigabeprüfung blockiert (siehe Abschluss).

**Exakt nächster offener Arbeitsschritt:** Diesen nun erneut verifizierten P1-Fix samt neuem Kurztest-/Soak-Umfang in `PROJECT_STATE`, Stage-5-Report und Evidenz nachvollziehbar abschließen und gezielt unabhängig prüfen lassen; erst danach den PR-#12-Abschluss vorbereiten. Wegen der Änderung am gemeinsamen Diagnosevertrag und am nativen WMS-Initialisierungspfad empfiehlt sich ein enger Claude-Review dieses Deltas. Die aktuelle Aufgabe autorisiert keinen Push oder Merge; die kanonischen Dateien wurden deshalb nicht vorweg auf PASS umgeschrieben.

Dokumentationskonflikte: `PROJECT_STATE` und Stage-5-Report sagen noch B-3 PASS mit 20 Zyklen; GitHub hat drei offene P1, der Worktree verwendet fünf Zyklen. Das Risk Register enthält zusätzlich alte Formulierungen wie R-003 „targeted re-review pending“, R-005 „UI follow-up“ und R-008 „Open“, obwohl spätere Berichte bereits einschlägige Softwareevidenz enthalten. Risiken sind nicht automatisch geschlossen. Das Decision Log endet bei D-021; die spätere Testverkürzung ist nur im Aufgabenverlauf belegt. Die akzeptierten ADR-0001 (exakte backendbezogene Identität) und ADR-0002 (Callback-/Header-Eigentum) bleiben verbindlich.

## 4. Heute ausgeführte Verifikation

Frischer projektinterner VS-2022-x64-Debug-Build mit MSVC 19.44, Qt 6.10.3, vorhandenem WMS-SDK/Console 1.0.17-rc.4.25 und Windows SDK 10.0.26100.0. Host: Windows 11 Pro, Build 26200. Keine Installation und keine globale Konfigurationsänderung.

| Nachweis | Ergebnis | Grenze |
|---|---|---|
| Configure und vollständiger Build | PASS | Dirty-Snapshot `7dc09c9` + gesicherter Patch |
| `ctest -C Debug -LE local-midi --output-on-failure` | **28/28 PASS**, 10,29 s | Enthält vier optionale Stage-1-Tests; darunter zwei WMS-Diagnose-Loopback-Tests. Daher nicht mit den 24 reinen Cloudtests gleichsetzen |
| `stage5_local_product_host_lifecycle` | **1/1 PASS**, 34,65 s | Temporäre WMS-Loopback-Paarung, fünf Zyklen je Backend; kein Synth |
| WMS Product-Host | RX callbacks/delivered 1/1, TX 1, drops/late 0/0, max. Shutdown 63 ms | `closed`, erfolgreiche Closes und Worker-MTA werden durch Assertions geprüft |
| WinMM Product-Host | RX callbacks/delivered 3/1, TX 1, drops/late 0/0, max. Shutdown 37 ms | Gleiche Close-Assertions; GUI `main_sta` für beide Prozesse protokolliert |
| Bereinigung | PASS | Temporäre Endpunkte entfernt, WMS- und WinMM-Enumeration exakt wie vorher; keine offenen WMS-Verbindungen danach |
| Native Windows-Geometrie | 1920×1200 Bildschirm, DPR 1,25; Mindestgröße 758×419; alle sieben Workspace-Raster 1920×1080 | Produktions-MainWindow mit Fake-Transport; geometrische Messung, keine neue manuelle GUI-Abnahme und keine 150/200-%-Evidenz |
| `git diff --check` | PASS | CRLF-Hinweise sind keine Whitespace-Fehler |

Die fünf älteren langen lokalen Regressionen wurden heute nicht erneut ausgeführt. Es wird weder ein neuer 6/6-Lauf noch eine physische Gerätevalidierung behauptet. Die aktuellen Testlogs, Portlisten und die historische CI-Evidenz liegen neben diesem Bericht.

## 5. Hardwarebereitschaft und sichere Reihenfolge

Live-Enumeration: **21 WinMM-Eingänge, 25 Ausgänge**, keine doppelten vollständigen WinMM-Kompositidentitäten. Indizes sind nur Momentaufnahmen und wurden nicht als persistente Route verwendet.

| Erkanntes Interface/Gerät | Sichtbare WinMM-Ports | Aussage |
|---|---|---|
| ESI M8U eX | `M8U eX 1` bis `16`, je RX und TX | Interface eindeutig; angeschlossene Synthesizer und Verkabelung unbekannt |
| KONTROL S61 MK3 | Hauptport plus `MIDIIN2/3` und `MIDIOUT2/3` | Drei getrennte Portpaare; ihre konkrete Rolle wird nicht geraten |
| Arturia AF16Rig | RX: DIN 1, Usb Host; TX: DIN 1, DIN 2, Usb Host, Clock, Mixer Control | DIN-/Host-Ziel unbekannt; Clock und Mixer Control sind keine beliebig verwendbaren Synth-Ports |
| Microsoft GS Wavetable Synth | Ein Software-TX | Kein externer Synthesizer |

WMS zeigt drei physische KSA-Endpunkte sowie die drei internen Microsoft-Diagnoseendpunkte. Physische IDs (jeweils einschließlich vollständiger GUID-Endung in den gespeicherten Portlisten): `midiu_ksa_6080130142804196953` (M8U), `midiu_ksa_7042593043182509155` (KONTROL), `midiu_ksa_10735019151625753892` (AF16Rig). Keine automatische WMS↔WinMM-Routenumsetzung. Kein Summit oder minilogue xd ist unter eigenem USB-Namen sichtbar; ein Gerät kann dennoch hinter DIN angeschlossen sein.

**Rekonstruierte genehmigte Matrix:** D-014 erlaubt die temporäre Software-Loopback-Paarung samt Entfernung. Stage 5 verlangt Product-Host-Lifecycle/Active-Close, Port-/Runtime-Evidenz und native GUI-Skalierung. Der PM-Handoff sieht zusätzlich beaufsichtigte physische MIDI-/SysEx-Prüfungen vor, enthält aber keine konkrete Geräte-, Port-, Firmware- oder sichere Befehlsliste. Die aktuelle Benutzeranweisung erlaubt nur bereits dokumentierte sichere Realtests. Eine ausführbare freigegebene physische SysEx-Matrix ist deshalb **nicht belegt**.

Der Summit ist im aktiven Profil ausschließlich **Detect**; Read, Transfer und Validated Restore sind false. D-021 erlaubt `Crazy Sine.syx` nur für private Read-only-Dateitests, ausdrücklich keinen Hardwareversand. Die drei unvollständigen Legacy-Dumps sind als V4-Fixtures ausgeschlossen. Legacy-Capture-Erfolge für minilogue xd, Artemis, Subsequent 37, TEO-5, Summit, Virus A und K5000S sind historische Referenz; Waldorf Protein hat zusätzlich eine ungeklärte Inhaltssemantik. Keine dieser Angaben erteilt V4 eine Sendefreigabe.

Empfohlene Reihenfolge:

1. P1-Dokumentation/gezielten Review abschließen; konkreten Teststand durch Commit und Patch-/Binärhash eindeutig identifizieren.
2. Native Windows-Sichtprüfung bei 150 % und 200 % durch den PO; sieben Workspaces, reale Fenster-/Arbeitsflächenmaße, Überlappung, Scrollbarkeit und Bedienbarkeit protokollieren. Keine Simulation als Ersatz.
3. Ein konkretes Instrument samt Firmware und vollständiger RX-/TX-Zuordnung bestätigen. Zunächst ausschließlich RX öffnen, TX unverbunden; manuell am Gerät erzeugte MIDI-Ereignisse beobachten.
4. Erst mit dokumentiert sicherer, am Gerät ausgelöster Dump-Funktion einen Receive-only-SysEx-Test planen. Rohbytes, Framing, Taint/Verlust, Abschluss und Speicherung in neuer Datei nachweisen.
5. Ein hostseitiger Request erfordert zusätzlich verifizierte Befehlsbytes und bestätigte reine Lesewirkung. Restore, Preset-/Speicherwrite, Firmware, Panic/Test-CCs, automatische Identity Requests und unbekannte SysEx bleiben außerhalb dieses Auftrags.

**Stop/Ask-Vorlage:** Erwartet war ein sicher dokumentierter physischer Test. Tatsächlich sind nur Interfaces eindeutig; Instrument-/DIN-Zuordnung und sichere Befehlsmatrix fehlen. Es wurde daher keine physische Route geöffnet und kein physisches MIDI gesendet. Ursache ist fehlende Testfreigabe/Zuordnung, kein nachgewiesener Treiberdefekt. Optionen: (A) empfohlen — zunächst den P1-/GUI-Abschluss und anschließend einen konkreten Receive-only-Test; (B) dokumentierten Read-only-Request nach zusätzlicher Geräte-/Protokollprüfung. Benötigte PO-Angabe: Instrument, Firmware, tatsächlich verkabelter Eingang und gewählter sicherer Empfangsablauf. Grundlage: `QUALITY_POLICY.md` §9/§10, `STAGE_5_BRIEF.md` §20 und ADR-0001 sowie die aktuelle Benutzeranweisung. Der offene P1-Abschluss bleibt eine zusätzliche Voraussetzung für Gerätevalidierung.

## 6. Vorgänger und Archive

`TAUREON-Synth-Tool` ist ein eigenständiger Python/Tkinter/mido-Vorgänger mit C++17-WinMM-Capture-PoC, kein zweiter Checkout von V4. Lokal sauber, `main` und live `origin/main` auf `6315136bb25f44dd0c5f3abf914074343257e1a4`; keine offenen oder historischen PRs gefunden. Ein lokaler Freeze-Tag `freeze-v1.13-2026-06-01`, keine Remote-Tags, keine Stashes und ein Worktree. Die Git-Wurzeln von Legacy und V4 sind verschieden. Seine aktuelle MIT-Lizenz ist vorhanden; die V4-Provenienztabelle „No root license file found“ ist insoweit historisch veraltet. Das klärt nicht automatisch jede fremde Quelle oder jedes Datenset.

`TAUREON-Archives` ist kein Git-Repository. Es bewahrt sechs vormals untracked Planungs-/GUI-Dateien inklusive ZIP vom 09.09. auf. `TAUREON2_GUI_CONCEPT_STATUS.md` ist nach SHA-256 tatsächlich byteidentisch mit der V4-Kopie (`79F55D79...D563`). Übrige Unterlagen sind nach `SUPERSEDED_DOCUMENTS.md` historische Quellen, keine konkurrierenden V4-Spezifikationen. Keine Übernahme oder Löschung empfohlen, bevor ein konkreter fehlender Inhalt benannt ist.

## 7. Separat: eine gemeinsame versionierte MIDI-Bibliothek

**Technisch geeignet, aber noch kein konsumierbares Bibliothekspaket.** `core/midi`, `core/sysex`, `core/transfer`, `IMidiTransport` und die getrennten statischen WinMM-/WMS-Targets haben die passende fachliche Grenze. Die generischen Kern-/Transportquellen hängen nicht von Qt-GUI oder einem Synthmodell ab. Exakte Routen, geordnete Loss-/Taint-Ereignisse, SysEx7-Konvertierung, Cancellation und deterministischer Shutdown müssen als Paketverträge erhalten bleiben.

Aktuelle Hindernisse:

- `taureon_midi_core` bündelt zusätzlich App-Dienste, Profile, Manager/Librarian, Settings, Diagnostics und Fake-Transport. Ein kleiner Client würde derzeit zu viel V4-Inhalt mitziehen.
- Das Buildsystem fordert Qt Widgets immer an, auch wenn nur der Kern gewünscht wäre. Öffentliche Includes zeigen auf den Sourcebaum; es fehlen installierbare Headers, CMake-Exports/Config/Version, Namespaces für importierte Targets und eine eigenständige Consumer-Prüfung. Projektversion derzeit `0.0.0`; keine V4-Tags.
- WMS hat einen Windows-/MSVC-/SDK-gebundenen C++/WinRT-Generierungspfad und einen projektspezifischen Dependency-Pfad. WinMM bleibt separat; WMS darf später optional sein, ohne parallele Engine oder verdeckten Backendwechsel.
- Öffentliche C++-Typen enthalten STL-Objekte, Callbacks und Layouts wie `MidiTransportDiagnostics`. Schon das neue bool-Feld betrifft die Binärgrenze. Zunächst eine versionierte Quell-/statische CMake-Bibliothek mit gemeinsamem Toolchain-Build bevorzugen; keine stabile DLL-ABI behaupten.
- V4 hat **keine eigene LICENSE**; `PROVENANCE.md` sagt „License not selected“. WMS-NuGet und C++/WinRT enthalten MIT-Lizenzangaben; die Qt-Auslieferungsentscheidung ist im Projekt noch ungeklärt. Private Summit-Fixture und Profildaten dürfen nicht ungeprüft ins allgemeine Paket oder öffentliche Tests gelangen. Vor einer Verteilung ist eine ausdrückliche Lizenz-/Provenienzentscheidung erforderlich.
- Der vorhandene minilogue-xd-Librarian nutzt laut README noch den älteren nativen TAUREON-Capture-Helfer plus separaten mido-Sendepfad. Bei späterer Wiederaufnahme sollten Empfang und Sendung über dieselbe neue Bibliothek laufen; falls Python bleibt, ein schmaler Binding-/C-Adapter über genau diesen Kern. Korg-Befehle, Speicherlogik und Firmwarefunktionen gehören weiterhin oberhalb der generischen Bibliothek.

**Empfehlung:** Neustart des Librarians pausiert lassen; zuerst V4-Block-B/P1-Abschluss, dann ein abgegrenztes ADR für Paketumfang, Versionierung, API/Lifetime, CMake-Consumer, optionale Backends und Lizenz. Erst nach dessen Annahme extrahieren. Eine Codekopie in das kleinere Projekt würde die gewünschte einzelne Engine wieder aufspalten. Es wurde keine Extraktion und keine projektübergreifende Änderung vorgenommen.

## Reproduktion und erhaltene Ergebnisse

Die heutigen Tests verwendeten eine neue `.work/reconstruction-20260926`-Buildstruktur mit `TAUREON_BUILD_STAGE1_SPIKES=ON`, `TAUREON_ENABLE_LOCAL_MIDI_INTEGRATION_TESTS=ON`, `TAUREON_ENABLE_WMS_TRANSPORT=ON` und `CMAKE_PREFIX_PATH=C:/Qt/6.10.3/msvc2022_64`. Die bereits vorhandenen gepinnten SDK-Dateien wurden nur gelesen.

```powershell
cmake -S . -B .work/reconstruction-20260926 -G 'Visual Studio 17 2022' -A x64 '-DCMAKE_PREFIX_PATH=C:/Qt/6.10.3/msvc2022_64' -DTAUREON_BUILD_STAGE1_SPIKES=ON -DTAUREON_ENABLE_LOCAL_MIDI_INTEGRATION_TESTS=ON -DTAUREON_ENABLE_WMS_TRANSPORT=ON
cmake --build .work/reconstruction-20260926 --config Debug --parallel 6
ctest --test-dir .work/reconstruction-20260926 -C Debug -LE local-midi --output-on-failure
ctest --test-dir .work/reconstruction-20260926 -C Debug -R '^stage5_local_product_host_lifecycle$' -V
```

Die Logs sind gesichert. Die automatische Freigabeprüfung blockierte die Entfernung der neuen Buildstruktur samt Wegwerf-Previews zweimal mit `blocked by policy`, auch nach separater Prüfung des absoluten Pfads und einem eng begrenzten `Remove-Item -LiteralPath`. Eine genauere Begründung wurde nicht geliefert. Deshalb verbleibt ausschließlich der neu erzeugte entbehrliche Build unter `D:\Eigene Dateien\Eigene Dokumente\Playground\TAUREON-Synth-Tool-V4\.work\reconstruction-20260926` zur späteren Bereinigung. Keine Testprozesse liefen bei der Prüfung mehr. Die erfolgreiche Entfernung der temporären MIDI-Endpunkte ist davon unabhängig und vollständig verifiziert.

Dieser Evidenzordner enthält Bericht, nachvollziehbare Logs und die explizite Sicherung der bestehenden Änderungen. Es wurden keine bestehenden Quell-/Prozessdokumente überschrieben, keine Branches zusammengeführt und keine externen Nachrichten oder Reviewkommentare veröffentlicht.

## 8. Live-Update nach Einschalten des Summit, 26.09.2026

Der PO bestätigte, dass der Summit über einen USB-Hub mit eigenem USB-Port verbunden ist, und schaltete ihn ein. Eine neue ausschließlich lesende Enumeration zeigt nun den **eindeutigen** Novation-Endpunkt `Summit`: USB-Gerät `USB\VID_1235&PID_0133\3484375A3337`, PnP `OK`; WMS `\\?\swd#midisrv#midiu_ksa_7469713634660520082#{e7cce071-3c03-423f-88d3-f1045d02552b}`, eine Input- und eine Output-Block/Gruppe 1. WinMM zeigt genau eine Eingangsroute (`Summit`, `wMid=1`, `wPid=25`, Treiberversion 256) und eine Ausgangsroute (`wMid=1`, `wPid=26`, Version 256). Die WinMM-Indexhinweise 23/27 sind flüchtig und nicht Teil der Routenidentität. Die neue WinMM-Gesamtzahl ist 24 Eingänge und 28 Ausgänge. Gegenüber der Vorher-Aufnahme sind außerdem `MiniFreak` und `MODEL D` hinzugekommen; das wird nicht dem Summit zugeschrieben. Die frühere 21/25-Inventur in Abschnitt 5 gilt nur für den Zeitpunkt vor dem Einschalten.

Ein **passiver WMS-Eingangsmonitor** wurde mit genau der Summit-WMS-ID gestartet, ohne TX-Route oder Sendefunktion. Der erste Start schlug wegen der vom Konsolentool verlangten interaktiven Eingabe fehl; dieses Toolproblem steht in `summit-passive-wms-monitor.txt` und ist kein Hardwarebefund. Der zweite Start in einer interaktiven Konsole lief ungefähr 45 Sekunden, wurde kontrolliert mit Escape beendet (`exit 0`) und meldete `No messages received`. Es ist nicht belegt, dass während dieses Fensters eine Taste gespielt oder ein Dump am Gerät ausgelöst wurde. Deshalb folgt daraus **weder PASS noch FAIL für den realen Empfangspfad**. Es wurde nichts an den Summit gesendet, keine Datei vom Synth gelesen oder geschrieben und kein V4-Produktempfang behauptet. Die nachträgliche WMS-Sitzungsliste zeigt keine Summit-Sitzung des Monitors; eine unabhängige MidiSrv-Sitzung an `Iridium` war sichtbar und blieb unangetastet.

Der nächste gefahrlose Diagnoseschritt ist ein zeitlich koordinierter reiner Empfangsversuch: Monitor sichtbar starten, der PO spielt danach eine Taste am Summit, und nur das eingehende Note-On/Off-Ereignis wird protokolliert. SysEx und Stage-5-Hardwareabnahme bleiben an die oben genannten Gate- und Sicherheitsbedingungen gebunden.

## 9. Koordinierter passiver Summit-Empfangstest, 26.09.2026

Nach dem vorzeitig beendeten ersten Fenster wurde ein neuer, ausdrücklich koordinierter Test gestartet. Verwendet wurde ausschließlich `midi.exe endpoint <exakte Summit-WMS-ID> monitor --verbose --include-timestamp` gegen `\\?\swd#midisrv#midiu_ksa_7469713634660520082#{e7cce071-3c03-423f-88d3-f1045d02552b}`. Erst nachdem die Konsole `Monitoring incoming messages on Summit` angezeigt hatte, erhielt der PO das Startsignal. Es wurde kein Ausgang geöffnet, kein MIDI- oder SysEx-Request gesendet und keine Capture-, Preset- oder Synth-Datei geschrieben.

Der PO spielte bzw. bewegte Bedienelemente am Summit. Der WMS-Monitor empfing und dekodierte **905 Nachrichten** auf Gruppe 1 / MIDI-Kanal 1. Im sichtbaren Konsolenprotokoll sind unter anderem Note On/Off mit unterschiedlichen Noten und Anschlagstärken, Pitch Bend sowie Control Changes belegt; beispielsweise eine absteigende Notenfolge von B5 bis C1, CC 1 und später CC 86/87. Nach dem PO-Signal `fertig` wurde der Monitor mit Escape kontrolliert beendet. Das Tool meldete `Escape key pressed. Monitoring terminated.`, `905 messages received` und Exitcode 0. Die anschließende aktive WMS-Sitzungsliste enthält keine Summit-Sitzung mehr; die unabhängige bestehende Iridium-Sitzung blieb unangetastet.

**Belastbarer Befund:** Der konkrete Summit-USB-Endpunkt kann über Windows MIDI Services passiv geöffnet werden, und vom PO am Gerät erzeugte MIDI-1.0-Kanalnachrichten erreichen diesen WMS-Empfangspfad in großer Zahl und werden als UMP/MIDI-1.0-Nachrichten dekodiert. Damit ist der zuvor offene koordinierte Basis-RX-Nachweis positiv.

**Grenzen:** Dies war das Microsoft-WMS-Konsolentool, nicht der V4-Produktpfad. Wegen der reinen Konsolenausgabe ohne Capture-Datei und der begrenzten Tool-Ausgabeaufbewahrung wird keine exakte Typverteilung, lückenlose Bytefolge oder Verlustfreiheit behauptet. Die teils unplausible bzw. umgebrochene `Receive Delta`-Anzeige wird nicht als Latenznachweis gewertet. Es wurde kein SysEx ausgelöst oder als vollständiger Dump nachgewiesen, kein automatischer Reconnect provoziert und kein Langzeit-, WinMM-, Transfer-, Restore-, Firmware- oder Presettest durchgeführt. Der Befund ändert daher weder die Summit-Profilgrenzen (nur `Detect`) noch die HOLD-Gates für physische Sends und Stage-5-Abnahme.

Die sechs bereits vorhandenen lokalen Quelldateien waren nach dem Test SHA-256-byteidentisch mit `local-source-hashes-before.json`. Kein Commit, Checkout, Stash, Reset, Merge oder Push wurde ausgeführt.

## 10. Weitere passive Hardwarediagnose, 26.09.2026

### Summit über WinMM

Der Summit blieb mit exakter WinMM-Eingangsidentität `Summit`, `wMid=1`, `wPid=25`, Treiberversion 256 eindeutig vorhanden und PnP-seitig `OK`. Das Microsoft-SDK-Hilfsprogramm `midi1monitor.exe` öffnete den aktuell enumerierten Eingang 22 in zwei getrennten, zeitlich koordinierten Läufen. Im zweiten Lauf drückte der PO nach dem bestätigten Startsignal ausdrücklich dreimal dieselbe Taste. Beide Läufe endeten kontrolliert mit Exitcode 0, meldeten aber jeweils `Total Bytes Received: 0`, `Status Bytes Received: 0` und keine Zeitstempel.

Damit ist der Summit-WinMM-Empfang **nicht bestätigt**. Wegen des unmittelbar zuvor erfolgreichen WMS-Laufs mit 905 Nachrichten ist dies kein allgemeiner Summit-/USB-RX-Ausfall. Der Befund grenzt das Problem auf den WinMM-Kompatibilitätspfad bzw. das ältere Hilfsprogramm ein; ohne zweiten unabhängigen WinMM-Empfänger wird noch kein Fehler im V4-Transport oder Treiber behauptet. Ein irrtümlich durch den nicht unterstützten `--help`-Aufruf kurz geöffneter `M8U eX 1`-Monitor empfing nachweislich keine angezeigten Daten, sendete nichts und wurde gezielt beendet.

### Native Instruments KONTROL S61 MK3 über WMS

Auf PO-Wunsch wurde anschließend ausschließlich der exakte WMS-Endpunkt `KONTROL S61 MK3` (`\\?\swd#midisrv#midiu_ksa_7042593043182509155#{e7cce071-3c03-423f-88d3-f1045d02552b}`) passiv geöffnet. Dieser Endpunkt deklariert drei Eingangsgruppen: Main, DAW und Ext. Erst nach der bestätigten Monitoranzeige spielte bzw. bediente der PO das Gerät.

Der Monitor empfing und dekodierte **232 Nachrichten**, im beobachteten Lauf ausschließlich auf Gruppe 1 / MIDI-Kanal 1. Sichtbar waren Note On/Off mit unterschiedlichen Anschlag- und Release-Werten, Pitch Bend sowie unter anderem CC 1 und CC 11. Damit ist der passive WMS-RX-Pfad des Main-Ports positiv belegt; für DAW und Ext folgt aus diesem Lauf keine Aussage. Nach `fertig Kontrol` endete der Monitor kontrolliert mit Escape, `232 messages received` und Exitcode 0. Es wurde kein Ausgang geöffnet und nichts an das NI-Gerät gesendet.

Auch dieser Befund betrifft das Microsoft-WMS-Konsolentool, nicht den V4-Produktpfad, und belegt keine lückenlose Aufzeichnung, SysEx-Funktion, WinMM-Funktion oder Langzeitstabilität.

Ein vorbereiteter, aber wegen des Gerätewechsels nicht ausgeführter unabhängiger WinMM-Wegwerfmonitor wurde wieder aus dem Quellbestand entfernt. Die rekursive Entfernung seines generierten CMake-Unterbaus unter `.work/reconstruction-20260926/summit-winmm-passive-build/out` wurde trotz voriger absoluter Pfadprüfung von der automatischen Freigabe blockiert; daraus wird kein Testergebnis abgeleitet.

## 11. MiniFreak: koordinierter passiver WMS-Empfangstest, 26.09.2026

Der Arturia MiniFreak wurde frisch und eindeutig als WMS-KSA-Endpunkt `MiniFreak` mit der ID `\\?\swd#midisrv#midiu_ksa_8745784369346682120#{e7cce071-3c03-423f-88d3-f1045d02552b}` enumeriert. Er deklarierte genau eine Message-Source- und eine Message-Destination-Gruppe, jeweils `MiniFreak MIDI`, Gruppe 1. Für den Test wurde ausschließlich der Eingang passiv geöffnet. Erst nach der bestätigten Anzeige `Monitoring incoming messages on MiniFreak` spielte bzw. bediente der PO das Gerät.

Der Monitor empfing und dekodierte **789 Nachrichten**, im sichtbaren Protokoll auf Gruppe 1 / MIDI-Kanal 1. Nachgewiesen sind Note On/Off mit unterschiedlichen Anschlag- und Release-Werten sowie dichte Control-Change-Verläufe, unter anderem CC 117 und CC 74. Nach dem PO-Signal `fertig` wurde der Monitor mit Escape kontrolliert beendet; das Tool meldete `789 messages received` und Exitcode 0. Es wurde kein Ausgang geöffnet, kein Request gesendet und nichts am Gerät verändert.

Damit ist der passive MiniFreak-WMS-RX-Basispfad positiv belegt. Wie bei den anderen Läufen ist dies keine V4-Produkt-, WinMM-, SysEx-, Verlustfreiheits-, Latenz- oder Langzeitvalidierung. Wegen der begrenzten Konsolenausgabeaufbewahrung wird keine vollständige Nachrichtentypverteilung behauptet.

Ein anschließend erwogener Iridium-Lauf wurde nicht durchgeführt: Bei der frischen Prüfung war kein aktiver Iridium-PnP-/WMS-Endpunkt vorhanden, und der PO entschied ausdrücklich, den weiteren gleichartigen Test zu überspringen, weil er keinen wesentlichen neuen Nachweis erwarten ließ. Daraus folgt weder PASS noch FAIL für Iridium.

## 12. Gezielte P1-Nachprüfung, 26.09.2026

Die unabhängige Nachprüfung akzeptiert alle drei P1-Codekorrekturen: echte native
Empfangsaktivität wird vor dem Close nachgewiesen, Close-Erfolg und finaler Zustand
`closed` werden erzwungen, und GUI-STA/MAINSTA sowie WMS-Worker-MTA werden auf den
richtigen Threads beobachtet. Weitere blockierende Codeänderungen wurden nicht verlangt.

Die Empfehlung bleibt dennoch **HOLD**. Vor einem B-3-PASS müssen die Änderungen an
eine unveränderliche gepushte Commit-SHA gebunden und durch die nicht-lokale Windows-CI
bestätigt werden. Danach ist auf genau dieser SHA ein neuer 100-Zyklen-Lauf mit
vollständigem Rohlog erforderlich; beide Backends müssen den aktiven Handle-Growth-Gate
ohne anhaltendes Wachstum bestehen, und die temporären Endpoints müssen nachweislich
entfernt sein. Der normal registrierte Fünf-Zyklen-Lauf ist bewusst kein Langzeit-
Ressourcennachweis. Details stehen in `P1_REVIEW_RESULT.md`.
