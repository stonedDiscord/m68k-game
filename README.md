Name nicht final.

Das Projekt beinhaltet Homebrew-Software für die Kombination aus Steuereinheit 68000 + Videocontroller + Spiel und Speichermodul bekannt als PROFITECH 3000.

Ziel ist es ein Testprogramm zur Diagnose der Geräte zu bauen, eventuell auch ein Spiel das interessanter ist als Karten, Tetris oder Slots.

Aktuell (Stand März 2026) startet die Software im MAME Emulator für Geräte des Typ Skat TV, auf Geräten mit Bt481 RAMDAC bleibt aktuell der Bildschirm noch schwarz.

TODO:
- RAMDAC konfigurieren damit das Bild nicht schwarz bleibt.
- Der Hauptloop ist langsam
- evtl Schrift im Video-ROM ablegen um hier CPU-Zeit zu gewinnen, idealerweise mit Erkennung ob diese da ist und funktioiniert
- User Interrupt 64 & 65 checken ob das so stimmt
