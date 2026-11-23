# PlayerMenu: the personal file

## Intent

A player can see where they stand without walking anywhere: who they are, their money, their papers,
their job levels and their errands. The maintainer asked for it (2026-11-22). It is a read-only record;
everything it prints already lives in another feature.

## Interaction pattern

Rung 1 screen on a key: **F2** (gamepad: hold **RB + Y**; a tap of RB + Y stays the welcome guide), listed in
Settings > Controls. It is a kit sheet (`LF_PlayerMenuSheet`, the `LF_UISkin.Dashboard` skin: 80% of the
screen, centred, the world dimmed) built from kit columns and cards. A header bar (the name large, a toned
role badge, "Day N on the island · playtime") sits over the tab keys:

- **PERSON**, three columns: a PERSON card (a live preview of the local character, `LF_WantedLook.Capture`
  drawn by `LF_WantedPortrait`; profession from the replicated job component; wanted; the locals' and the
  law's word from `LF_ReputationSync` client tiers); FINANCES and STATUS cards of icon rows, values flush
  right (cash, bank; houses, garage used of held, island day, playtime, licenses and permits with each one
  listed); a SKILLS card with every job track as a compact row (level chip, **Read**, XP bar under it) and
  the picked one's record: what it is, the bar, "Level N · X / Y XP" and "Next level opens: ..." (chains
  with a step gated at exactly that level, `LF_ChainCatalog`).
- **PROGRESS**, two cards: the guided arrival chain (quests with `m_sTargetKind`) in chain order with DONE /
  CURRENT / LATER chips and a done count, then any other quest taken; beside it the picked errand (the
  current one by default): title, description, objective progress and **Set waypoint to the current
  errand** (`LF_AskWelcomeGuideWaypoint`).
- **WHAT TO DO NEXT**: a slot, `LF_PlayerMenuNextTab`. Its key shows only when `HasContent` says so; the
  next-steps feature fills that one file.
- **GANG**: only for a player in a gang (`LF_PlayerMenuGangTab`, read only): the gang's name and flag, your rank, who is
  online, whether your radio is on the gang channel and the sites held. Its facts come in their own answer
  (`RpcAsk_LF_PlayerMenuGang`), built for the asker's own gang only. No standing is printed.

Footer: key-cap chips **Esc Close**, **F3 Welcome guide** (`LF_WelcomeGuideSheet.Open`), **Settings**.
While the file is open the HUD, the vanilla hint card and the notification toasts (`LF_NotificationHold`)
are put away; they come back when it closes. Esc, death and losing the body close it. The key does nothing over another sheet or menu, or while down.

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
