Name nicht final.

Das Projekt beinhaltet Homebrew-Software für die Kombination aus Steuereinheit 68000 + Videocontroller + Spiel und Speichermodul bekannt als PROFITECH 3000.

Ziel ist es ein Testprogramm zur Diagnose der Geräte zu bauen, eventuell auch ein Spiel das interessanter ist als Karten, Tetris oder Slots.

Aktuell (Stand März 2026) startet die Software im MAME Emulator für Geräte des Typ Skat TV, auf Geräten mit Bt481 RAMDAC bleibt aktuell der Bildschirm noch schwarz.

TODO:
- RAMDAC konfigurieren damit das Bild nicht schwarz bleibt.
- DUART blockiert, eventuell ist der Sender/Empfänger nicht richtig eingeschaltet.
- Der Hauptloop ist zu langsam
- evtl Schrift im Video-ROM ablegen um hier CPU-Zeit zu gewinnen, idealerweise mit Erkennung ob diese da ist und funktioiniert
- DUART Timer fehlen komplett
- DUART Interrupt ist ein Dummy
- User Interrupt 64 & 65 fehlt einer, vermutlich irgendwas mit Sound
