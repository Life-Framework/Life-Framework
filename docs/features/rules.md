# Rules

### `LF_HandbookScreen` — `Feature/Rules/LF_HandbookScreen.c` (#1773, 2026-11-16)
- ⚠️ The pause menu has a Handbook button under Field Manual. It opens a paper sheet listing the handbook pages (`LF_Handbook.GetPages`); picking a row shows that page's title and paragraphs with a button back to the list; Back, Escape, Backspace, Close and death close it. No pages: one line says so. Client only, kit parts, nothing sent to the server. Merged, in-dev: the part builders are tested; the screen itself ran only in the `ui-handbook` flow. Test `rules/handbook-screen` (LOGIC); flow `ui-handbook` (harness verb `pausemenu`).
