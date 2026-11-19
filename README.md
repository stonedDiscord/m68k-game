Name nicht final.

Das Projekt beinhaltet Homebrew-Software für die Kombination aus Steuereinheit 68000 + Videocontroller + Spiel und Speichermodul bekannt als PROFITECH 3000.

Ziel ist es ein Testprogramm zur Diagnose der Geräte zu bauen, eventuell auch ein Spiel das interessanter ist als Karten, Tetris oder Slots.

Aktuell (Stand März 2026) startet die Software im MAME Emulator für Geräte des Typ Skat TV, auf Geräten mit Bt481 RAMDAC bleibt aktuell der Bildschirm noch schwarz.

TODO:
- Schrift im Video-ROM ablegen um hier CPU-Zeit zu gewinnen, idealerweise mit Erkennung ob diese da ist und funktioiniert
- User Interrupt 64 & 65 checken ob das so stimmt

Serielle Diagnose (Kanal A, 9600 Baud, 8N1):
- `p <Adresse>` liest ein Byte (Peek), z. B. `p 000400`.
- `w <Adresse> <Wert>` schreibt ein Byte (Poke), z. B. `w fc0080 12`.
- Adresse und Wert sind hexadezimal, optional mit `0x`; mit Enter abschließen.
  Großbuchstaben `P`/`W` funktionieren ebenfalls. Escape bricht die Eingabe ab,
  Backspace korrigiert sie. Adressen sind auf 24 Bit und Werte auf 8 Bit begrenzt.
  Zugriffe erfolgen direkt auf den CPU-Adressraum, einschließlich I/O; nur gültige
  Adressen verwenden. Poke bestätigt den geschriebenen Wert ohne erneutes Lesen.

Bauen:
```shell
docker run -v .:/opt/m68k_bare_metal/src/ ghcr.io/stoneddiscord/m68k_bare_metal:master make split
```
