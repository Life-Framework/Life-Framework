# Player menu (personal file)

### `LF_PlayerMenuSheet` — `Scripts/Game/Feature/PlayerMenu/`
- ✅ **F2** (gamepad: hold RB + Y) opens a read-only kit sheet, a full-screen dashboard (80% of the screen,
  centred, the world dimmed behind it). **Header**: the name in large type, a role badge (Civilian plain,
  police blue, medic red, with duty), and "Day N on the island · X h Y min played" (island days from the
  day cycle's length). **Tabs**: Person, Progress, and a "What to do next" slot (`LF_PlayerMenuNextTab`)
  that stays hidden until a feature fills it. **Person**, three columns of cards: a live preview of the
  character as dressed now (`LF_WantedPortrait` on a sized `PORTRAIT`), profession, wanted and standing
  with the locals and the law; Finances (cash, bank) and Status (houses, garage x / y, island day,
  playtime, licenses and permits) as icon rows with the value flush right; every job skill with a level chip
  and an XP bar (scrolls past ten), the picked one's record under the list: description, XP bar,
  "Level N · X / Y XP" and "Next level opens: ..." (chain steps gated at that level). **Progress**: the
  errand list (First errands / Other, DONE / CURRENT / LATER chips, done / on file) beside the picked
  errand's record (the current one until a row is picked) and **Set waypoint to the current errand**.
  **Footer**: key-cap chips Esc Close, F3 Welcome guide, Settings. While open the HUD, the vanilla hint card
  and the notification toasts are put away (`LF_NotificationHold`) and come back on close.
  Focus starts on the open tab; MenuUp/Down walk tabs, skills or errands, and the footer.
- ✅ Data comes from one owner RPC per open (`RpcAsk_LF_PlayerMenu`, no argument, throttled 5 s on the
  client and by `LF_RateLimit` on the server); the server reads only the asking player's own state.
- ✅ New UI kit pieces: `COLUMNS` (side-by-side columns, `.Cards()` puts each on a plate; columns nest),
  `PAIR` (label, optional badge with a tone, value flush right, optional kit icon), compact `LIST` rows with a
  badge chip and `AddListBarRow` (a thin XP bar under the row), key-cap chips on `KEYS` (`WithCaps`), a sized
  `PORTRAIT` (whole figure), `LF_UISkin.Dashboard` (width share, centred, darker backdrop), `FocusPart`, and
  the kit icon set `LF_UIIcons.imageset` (white glyphs tinted with the theme; `tools/ui-icons/build-ui-icons.mjs`).
- ✅ New UI kit part `BAR` (`LF_UIBar.layout`, `LF_UIScreen.SetBar`): a thin bar in the skin's accent.
- ✅ Tests: `LF_Test_PlayerMenuXp`, `LF_Test_PlayerMenuData`, `LF_Test_PlayerMenuLayout` (LOGIC). Proof: `player-menu-proof`
  session verb; harness `playermenu seed` (dev gate) grants XP, cash, savings and a free house for pictures.
- ⬜ Quest chapters: quests carry no chapter field yet, so the list groups the guided chain and the rest.
- ✅ **Next tab** (2026-11-22): what the player can start now, with pitch, pay an hour, distance and state; **Start** / **Guide me** run a guided starter (`docs/features/activities.md`). Harness `playermenu open next`, `activities state|pick <n>|go`.
- 🧪 **Licences row names what is held (2026-11-22, review B2).** The Person tab's "Licenses and permits" row shows "None", the names of one or two licences ("Field hand"), or "N held" with one row per licence from three up (`LF_PlayerMenuSheet.LicencesValue`); the orphan "· Field hand" line under the count is gone. The HUD cash figure re-reads the pockets every second (`LF_BetaHud.PollCash`), so a desk purchase that took cash on the server always lands on screen.
