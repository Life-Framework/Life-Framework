# Player menu (personal file)

### `LF_PlayerMenuSheet` — `Scripts/Game/Feature/PlayerMenu/`
- ✅ **F2** (gamepad: hold RB + Y) opens a read-only kit sheet with two tabs. **Person**: name and role
  with duty, cash and bank, houses owned, garage slots, playtime, wanted level, licences and permits, and
  one row per job track with its level; **Read** shows the job's text, an XP bar and "Level N · X / Y XP".
  **Progress**: the arrival chain in order with DONE / CURRENT / LATER badges and counts, then other
  errands taken; **Read** shows text and progress; **Set waypoint to the current errand** reuses the
  welcome guide's server ask. Footer opens the welcome guide and the settings. Listed in Settings > Controls.
- ✅ Data comes from one owner RPC per open (`RpcAsk_LF_PlayerMenu`, no argument, throttled 5 s on the
  client and by `LF_RateLimit` on the server); the server reads only the asking player's own state.
- ✅ New UI kit part `BAR` (`LF_UIBar.layout`, `LF_UIScreen.SetBar`): a thin bar in the skin's accent.
- ✅ Tests: `LF_Test_PlayerMenuXp`, `LF_Test_PlayerMenuData` (LOGIC). Proof: `player-menu-proof` session verb.
- ⬜ A portrait of the character (the ID card's `PORTRAIT` part needs the look from the server).
- ⬜ Quest chapters: quests carry no chapter field yet, so the list groups the guided chain and the rest.
