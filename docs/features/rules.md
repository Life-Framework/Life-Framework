# Rules

### `LF_HandbookScreen` — `Feature/Rules/LF_HandbookScreen.c` (#1773, 2026-11-16)
- ⚠️ The pause menu has a Handbook button under Field Manual. It opens a paper sheet listing the handbook pages (`LF_Handbook.GetPages`); picking a row shows that page's title and paragraphs with a button back to the list; Back, Escape, Backspace, Close and death close it. No pages: one line says so. Client only, kit parts, nothing sent to the server. Merged, in-dev: the part builders are tested; the screen itself ran only in the `ui-handbook` flow. Test `rules/handbook-screen` (LOGIC); flow `ui-handbook` (harness verb `pausemenu`).

### Everon 1989 handbook pages — `LF_Everon1989/Configs/Rules/LF_Handbook_Everon1989.conf` (#1776, 2026-11-16)
- ⚠️ The flagship handbook lists seven pages: the core "how to read" page, then your first wages (post office parcel run, bank, town board), work in town, the police, the hospital, if you turn to crime, and the law and the permit (the core rules page patched in the overlay). The overlay adds five pages and patches `find-the-rules`; the words are `LF-Handbook_Everon_*` keys in the pack string table. Merged, in-dev: the maintainer reads each page against the game. Test `everon/handbook-pages` (pack leg, WORLD); flow `ui-handbook-everon` (boot with `--pack LF_Everon1989`; the core `ui-handbook` flow expects the core page titles).
