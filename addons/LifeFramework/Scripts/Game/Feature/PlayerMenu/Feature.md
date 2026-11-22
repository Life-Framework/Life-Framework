# PlayerMenu: the personal file

## Intent

A player can see where they stand without walking anywhere: who they are, their money, their papers,
their job levels and their errands. The maintainer asked for it (2026-11-22). It is a read-only record;
everything it prints already lives in another feature.

## Interaction pattern

Rung 1 screen on a key: **F2** (gamepad: hold **RB + Y**; a tap of RB + Y stays the welcome guide), listed in
Settings > Controls. It is a kit sheet (`LF_PlayerMenuSheet`, paper skin) with two tab keys:

- **PERSON**: name and role (civilian, police or medic, on or off duty); a FINANCES ledger (cash in hand,
  bank); a STATUS ledger (houses owned, garage slots used of held, account playtime, wanted level, licences
  and permits held); SKILLS, one list row per job track of the `jobs` config with its level. **Read** on a
  row prints the job's name, what it is, an XP bar (the kit's `BAR` part) and "Level N · X / Y XP".
- **PROGRESS**: the guided arrival chain (quests with `m_sTargetKind`) in chain order with DONE / CURRENT /
  LATER badges and a done count, then any other quest the character took. **Read** prints the title, the
  description and the objective progress. **Set waypoint to the current errand** sends the welcome guide's
  ask (`LF_AskWelcomeGuideWaypoint`), so the arrival guide marks the current step on the map and screen.

Footer: **Welcome guide** (`LF_WelcomeGuideSheet.Open`), **Settings** (the game's settings menu), **Close**.
Esc, death and losing the body close it. The key does nothing over another sheet or menu, or while down.

## Data path

Client opens the sheet and sends `RpcAsk_LF_PlayerMenu` (no argument) on its own `SCR_PlayerController`, at
most once every 5 s. The server builds `LF_PlayerMenuServer.Build(GetPlayerId())` for that player only
(behind `LF_RateLimit` COMMS) and answers `RpcDo_LF_PlayerMenu(wire)` to the owner. The wire
(`LF_PlayerMenuData`, version 1) is one string of `;` records and `|` fields; names lose both characters.
The client keeps the last answer so a reopen prints at once; a changed answer reprints the open tab.

Sources: `LF_PlayerAccount` (name of the active character, faction, duty, wanted), `LF_MoneyUtils.GetCash`,
`LF_ATMManager` / `LF_BankAccount` (record key), `LF_HouseManagerComponent.Live()` owner keys,
`LF_GarageSlots.UsedBy` / `CapOf`, `LF_Playtime.Total` (account uid), the body's
`LF_LicenseManagerComponent` (else `LF_Progression.GetLicenses`), `LF_JobLevels.XpOf` over
`LF_JobsConfig.m_aTracks`, `LF_QuestManager` definitions with the body's `LF_QuestTrackerComponent` state.

## Notes

- Level gates elsewhere still never print a number (`LF_JobLevelWords`); this file is the one place a
  player reads their levels, at the maintainer's request.
- Job names and descriptions are `LF-PlayerMenu_Skill_<JOB>` rows for the eight shipped tracks; a track a
  pack adds without rows prints its id in plain case and a generic description.
- Tests: `LF_Test_PlayerMenuXp` (bar math, skill row), `LF_Test_PlayerMenuData` (wire round trip, quest
  order and badges). Proof: `node agent-box/session.mjs player-menu-proof` (both tabs, UI shots). Harness
  verb `playermenu open [person|progress]|pick <n>|close|state`.
