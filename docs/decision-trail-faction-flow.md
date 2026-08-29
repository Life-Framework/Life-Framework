# Decision trail: faction pick + character creation flow

Running record for the "Police vs Civilian menu then spawn at a different area" work in the
DebugWorld. Each decision names the evidence that drove it. Read together with
`docs/session-handoff-2026-08-29.md`.

## The failure ladder (what the logs proved, in order)

1. **Stuck on the mission title splash.** The splash is `SCR_RespawnSystemComponent`'s loading
   placeholder. The only vanilla path that destroys it (`HandlePlayFromCamera` inside the audit
   chain) never runs: `SCR_BaseGameMode` self-invokes `OnPlayerAuditSuccess` for dedicated servers
   and listen-server remote players only, and the offline Workbench local player gets neither.
   Evidence: `logs_2026-08-29_20-50-41/console.log` - world reaches GAME state, zero
   `[LifeFramework]` lines, no character entity ever spawned.
2. **The faction menu was opened during loading.** Menus opened before the game state reaches GAME
   never appear. Evidence: same log, no widget creation for the menu layout.
3. **The old "temp character" was never possessed.** `CreateTemporaryCharacter` spawned an entity
   and left a note saying possession was TODO. Even a completed flow handed over nothing.
4. **`OpenMenu` left the menu root orphaned.** `parent=0` with `root=1 civilianBtn=1 policeBtn=1`:
   the tree existed, every lookup worked, nothing rendered. Evidence: the diagnostic
   `OnMenuOpen: name=FactionRoot parent=0` run.
5. **Runtime slot patching loses to the engine.** After a manual `AddChild`, the layout file's
   stored stretch anchor kept being re-applied by the engine (`Position/Size works only when min
   and max anchor is the same in given direction`, size back to `0x0` within one second). Evidence:
   two instrumented runs with `FrameSlot.SetAnchorMin/Max/SetOffsets` and later with point anchor +
   explicit size, both reverted by the engine.
6. **Conclusion: the MenuManager path is unfixable for this environment.** Overthrow was pointed
   out as the reference. `OVT_UIContext.ShowLayout` never touches the MenuManager:
   `workspace.CreateWidgets(m_Layout)` attaches the layout directly and buttons are wired through
   component invokers (`SCR_ButtonTextComponent.m_OnClicked`, `SCR_InputButtonComponent.m_OnActivated`).

## Decisions

- **Drop the MenuManager for the flow menus.** `EL_FactionSelectionMenu` and
  `EL_CharacterCreationMenu` are plain controllers that call `workspace.CreateWidgets` and remove
  themselves with `RemoveFromHierarchy`. The FactionSelection + CharacterCreationMenu presets were
  removed from `chimeraMenus.conf` and `ChimeraMenuPreset`. Trade-off: no menu stack, no ESC
  handling, mouse-driven only for now.
- **Per-frame input context activation.** A plain widget tree activates no input context, so no
  cursor appears and clicks die. While open, the menu activates vanilla `MenuContext`
  (Priority 50, Flags 0x4 - cursor plus the Menu* gamepad actions) every frame via a 0ms repeating
  callqueue call, removed on close. The creation menu switches to `MenuTextEditContext`
  (Priority 990) while one of its edit boxes has workspace focus, so keystrokes reach the box.
  Evidence: `chimeraInputCommon.conf` context definitions; Overthrow's `OVT_UIContext.EOnFrame`
  does the same for its contexts.
- **Buttons are `PauseMenuButton.layout` + `SCR_ButtonTextComponent`.** Overthrow's chooser uses
  exactly this pair; the component self-labels from `m_sText` and exposes `m_OnClicked`. The
  faction layout dropped the inherited `DialogButtonConfirm` buttons (labels defaulted to CONFIRM)
  and the dead EMS button (no `EL_Faction.EMS` exists).
- **The spawn logic no longer auto-creates characters.** `EL_SpawnLogic.CreateCharacter` used to
  invent a default character for an empty account, which let the vanilla audit chain bypass the
  faction pick entirely (observed: `Black_Male_02` spawned at the civilian point 33ms after
  connect, marking the account as "has character"). An empty account now logs
  `[ELDebug:Spawn] player N has no character yet` and waits; the flow owns first spawn.
- **ATM/survival component init moved to `EL_SpawnLogic.OnCharacterCreated`.** The creation flow
  used to init them on a controlled entity that did not exist. The post-spawn hook runs on every
  spawn path (first spawn and death respawn).
- **Splash destruction on every handover.** `EL_SpawnLogic.HandoverToPlayer` calls
  `DestroyLoadingPlaceholder` because the audit chain that normally does it never runs offline.
- **The survival HUD opening was removed.** Opened via `OpenMenu` it is a full-screen menu that
  captured input and trapped the player (observed: giant unsized hunger/thirst/health rectangles,
  "can't move"). It needs a proper HUD-element integration before returning.
- **All flow logging goes through `EL_Debug`** with the feature tags `CharacterCreation`,
  `FactionMenu`, `Spawn`. Verified via `tools\cli dev --tier fast` (validate + fast suite +
  `[ELDebug:*]` dump); no more arbitrary waits on Workbench play.

## Verification state

- `tools\cli validate` 4/4, `tools\cli dev --tier fast` 26/26 with the feature dump working.
- Observed live by the user before this document: the faction menu renders with both buttons, the
  spawn logic spawns at the correct faction area, and the placeholder/HUD traps above are gone.
- Still manual: the actual mouse click on a faction button (no MCP screenshot/click tooling
  exists), and the restart/late-joiner proof classes per AGENTS.md.

## Known parked items

- Character name/age entry: ported to the widget pattern with `MenuTextEditContext` typing support,
  but unproven in-game at the time of writing.
- Survival HUD as a real HUD element (EHudLayers) instead of a menu.
- The three test vehicles in the DebugWorld throw vanilla `SCR_ResourceEncapsulator` /
  `SCR_DamageManagerComponent` VMEs on init in every session (pre-existing, harmless so far).
- `m_aDefaultCharacterPrefabs` on the spawn logic prefab is now unused config (the flow creates
  characters explicitly); candidate for removal.
