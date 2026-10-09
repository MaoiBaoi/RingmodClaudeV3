# RingModSidechain - Version 3 (zwei VST3-Plugins)

## 1. RingModSidechain (der Effekt)
Der Sidechain (z.B. Kick) wird gleichgerichtet und senkt das Hauptsignal
(z.B. Bass) Sample fuer Sample ab:   Ausgang = Signal x (1 - |Sidechain| / Threshold)

Regler
- Threshold   Ab diesem Sidechain-Pegel wird das Signal komplett stumm.
- Attack      Wie schnell die Absenkung einsetzt (0 = sofort, reine Ringmod).
- Hold        Haelt die staerkste Absenkung fuer diese Zeit.
- Release     Wie langsam die Absenkung ausklingt.
- Wet/Dry     Tiefe der Absenkung (0 % = aus).
- Mode        Ringmod (folgt der Wellenform), Compress (folgt der Lautstaerke, RMS),
              Combined (beides hintereinander).
- Split       Nur der Bereich UNTER "Split Freq" wird abgesenkt, Hoehen bleiben.
- SC Hochpass / SC Tiefpass   Filtern nur das Sidechain-Signal fuer die Erkennung.
- Listen      Ausgang = gefiltertes Sidechain-Signal (zum Einstellen der Filter).
- St. Link    Beide Kanaele werden gleich abgesenkt (nach dem lauteren Sidechain-Kanal).
- Volume      Ausgangslautstaerke.

Wellenform-Anzeige: Signal (blau) und Sidechain (orange), halbtransparent, am Einsatz
des Sidechains ausgerichtet. "PHASE" zeigt die Korrelation (+1 = gleichphasig,
-1 = gegenphasig). NORM = jede Kurve einzeln auf Hoehe bringen.

## 2. SidechainCompare (reiner Analyzer, veraendert das Audio nicht)
Zeigt die Lautstaerke von Signal (blau) und Sidechain (orange) als zwei Linien.
Neueste Werte rechts, die Zeit laeuft nach links, 0 dB oben.

## Bauen lassen und installieren
1. CMakeLists.txt, Ordner Source und die Workflow-Datei auf GitHub hochladen/ersetzen.
2. Unter "Actions" den Build abwarten, Artifact "VST3-Windows" laden.
3. DAW schliessen, beide Ordner (RingModSidechain.vst3 und SidechainCompare.vst3) nach
   C:\Program Files\Common Files\VST3\  kopieren, DAW starten, Plugins neu scannen.
