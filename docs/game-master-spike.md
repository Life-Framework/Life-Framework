# Game Master in the playtest world (spike #617)

Why a tester could not enter Game Master, what changed, and what #145 (moderation, GM tools) should reuse.

## Why it did not open

Not a missing prefab or mission header. The base game's `SCR_EditorManagerCore` (`Configs/Core/EditorManagerCore.conf`) already creates one editor manager per player, and the game mode needs no extra component. What gates it is the mode list:

- The Game Master mode is `EDIT`. Its prefab entry carries `m_Flags 62` = `ADMIN | EDITABLE | AUTO_SELECT | WORKBENCH | PERSISTENT`. There is no `DEFAULT` flag, so a normal player gets no mode at all.
- `OnPlayerRoleChange` adds the `ADMIN` flagged modes only for players holding the administrator role (`SCR_Global.IsAdminRole`), and only when replication is running.
- `CanOpen` needs at least one mode (`EEditorCanOpen.MODES`), so with no mode the editor stays shut and the toggle key does nothing.
- A playtest server has `passwordAdmin ""` (`server/configs/test-server.json`) and no admin list, so nobody is an administrator. `LF_PlaytestTown.conf` also sets `m_bLF_DevToolsOff`.
- `addons/LifeFramework/Configs/Core/EditorManagerCore.conf` is a stripped copy (PHOTO only) from the EveronLife fork. It has its own GUID, not the base file's, so it does not override the base config and is inert. Do not "fix" it by giving it the base GUID.

Nothing in LF code blocks the editor (`SetCanOpen`, editor settings entity: none).

## What changed

`LF_DevGameMaster` (`Feature/Moderation/LF_DevGameMaster.c`), installed from `LF_GameModeRoleplay.OnGameStart` with `AreDevToolsEnabled()`. When the game mode opts into dev tools (`GameMode_Roleplay_Debug.et`) and the mission header does not close them, each editor manager gets `EEditorMode.EDIT` on the `BASE` access layer, half a second after creation (the core rewrites `BASE` right after its created event, so granting inside the event is lost). The production game mode never installs it: there only administrators get Game Master, as in the base game.

Harness verb `gm [close]` (`agent-box/harness.mjs`) opens or closes the editor for the local player and prints the modes it holds.

## Using it

- Debug mission (`session.mjs up --mission debug`, or the DebugWorld game mode in Workbench): every player can open Game Master (hold the editor toggle key, or `harness.mjs gm`). Fly with the editor camera, place from the content browser, move with the transform gizmo.
- Playtest town (`m_bLF_DevToolsOff`): dev tools stay closed on purpose, so Game Master is administrators only. Make the tester an administrator: set `passwordAdmin` in the server config and `#login <password>` in chat, or list their identity in the config's admin list.

## For #145

- Reuse the role gate, not a new permission: `SCR_Global.IsAdmin(playerId)` / `EPlayerRole.ADMINISTRATOR`, checked on the server per request (`LF_IncidentGm` and `LF_AdminAction` already do).
- Staff tools hook the vanilla `EDIT` mode UI (`SCR_EditModeEditorUIComponent`), as `LF_IncidentGmPanel` does. Do not add a second editor mode.
- A moderator who is not a full administrator needs a new grant: `AddEditorModes(EEditorModeAccess.BASE, EEditorMode.EDIT)` after the manager exists (the same delayed pattern as `LF_DevGameMaster`), plus a server-side allow list. Only `BASE` and `ROLE` access layers exist.
- Limited GM: `SCR_EditorManagerEntity.IsLimitedInstance()` is true when no mode gives full powers; useful for a trimmed moderator view.

## Proof

Debug mission session on lf-testbox: `harness.mjs gm` opened Game Master for a non-admin client (`canopen=1`, EDIT mode entity spawned, the incident panel from #240 showed). Screenshot: `website/public/docs-images/617/gm-edit-mode.jpg`. Not covered: the playtest town as an administrator (`#login`), and a restart.
