# Workbench play (wb-fast-play, 2026-11-16)

### `LF_PlayFromCamera` — `Feature/Character/Spawning/LF_PlayFromCamera.c`
- ✅ Workbench "Play from camera" keeps the player at the camera: the first spawn takes the pose of
  the editor body it replaces (vanilla `SCR_SpawnLogic` does the same). `#ifdef WORKBENCH` only;
  normal Play, dedicated servers and published builds use spawn points as before.
  `LF_Test_PlayFromCamera` (LOGIC).
- ✅ HUD root layout carries every vanilla HUD slot (screen effects, 2D optics, chat, VON,
  notifications, hints, game version): no screen-effect VME on Play (`validate-hud-root-slots`).
- ✅ Layout keywords are checked against the vanilla list (`validate-layout-keywords`).
- ✅ Sound slots with no sound yet are one summary warning, not an error per slot and per emitter.
- 🧪 (2026-11-16, #1827, epic #1826) Car colours, mechanism only: `LF_VehicleColoursConfig` (`Configs/Vehicles/LF_VehicleColours.conf`, empty in core, a pack overlay fills it by catalog prefab) lists each model's colours (name key, swatch, locked child, price delta). A colour is a thin child of our locked prefab, so the colour is the record's `m_sSpawnPrefab`: no new save data (version stays 34), no RplProp. `LF_AskBuyVehicle` and `RpcAsk_LF_BuyVehicle` carry a colour index; the server refuses one the model does not sell (`LF_Debug.Error`, nothing charged); the price is the model's plus the colour's delta before the cash-ceiling rule; the chop shop pays a colour child as its model; a respawn whose saved colour no longer loads falls back to the base colour. The sheet still sends colour 0 (the colour page is cc-3). Tests `vehicles/colours`, `vehicles/colours-overlay`.
- 🧪 (2026-11-16) **LF NPC Dresser (World Editor plugin).** `Scripts/WorkbenchGame/LFTools/LF_NpcDresserPlugin.c` (Workbench-only, never in the game module): select NPCs, Plugins > Life Framework > LF NPC Dresser (Ctrl+Shift+D), pick an outfit preset and/or a clothing prefab per slot (head, torso, legs, feet, vest, backpack), Dress. It overrides each slot's `Prefab` in the NPC's `BaseLoadoutManagerComponent` through `WorldEditorAPI.SetVariableValue` inside one `BeginEntityAction`, so Ctrl+Z undoes it; "Back to prefab" clears the overrides; a non-garment pick (no `BaseLoadoutClothComponent`) is refused. Presets are `LF_NpcOutfitPreset` confs in `Configs/NpcOutfits/`: police and doctor in the core, thirteen 1989 Everon locals in the pack. `tools\cli npc dress` runs the same code head-less (`--proof` reads back, undoes, redoes). Proof 2026-11-16 on lf-cloud: Bank_Teller dressed with the Mechanic preset, all 4 slots read back, undo restored the suit, redo re-applied, play-mode picture shows it. Guide: `docs/tooling/npc-dresser.md`. Merged, in-dev.

### `LF_WorkbenchPlay` — `Core/LF_WorkbenchPlay.c` (wb-fast-play-2, 2026-11-16)
- ✅ **LF Fast Play** (World Editor plugin, Ctrl+Shift+F5): play as a registered dev character in
  town, with the starting bank and an ID card, skipping the rules quiz and form R-89. Plain Play keeps
  the full onboarding. `#ifdef WORKBENCH` only (`docs/tooling/fast-play.md`).
- ✅ Workbench play with no mission header loads the pack overlays from the open world's header,
  and its missing persistence system is a warning, not an error. `LF_Test_WorkbenchPlay` (LOGIC).
- 🧪 (2026-11-16) **Tests do not read the town.** `validate-no-town-layout-asserts` fails a new test, harness proof or flow, the town manifest, the harness selftest or a validator that quotes a town entity name or writes a position within 6 m of a placed town entity (outside the annex). Today's debt is `tools/validation/town-layout-baseline.json`, which only shrinks (`--write-baseline` refuses to add); a new row in a file the branch did not change only warns, so moving things in the town never fails it. `town-layout-ok: <reason>` on a line marks a whole-world sweep. The town manifest's distance rules are warn-only everywhere; the harness selftest no longer asserts where live sites stand. Ten tests lost arbitrary town coordinates, `LF_Test_HoldUp` registers its own safe zone and `LF_Test_Roadmap_Contraband` measures from the drop it finds. Tests and tooling only.
