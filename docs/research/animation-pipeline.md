# Animation pipeline: reuse, clone, create (epic #340)

Research spike, 2026-11-10. Evidence is from `api_search`, `game_read`, `wiki_read`
and the base game's own animation files read straight out of the paks (the
workspaces under `anims/workspaces/player/`, `.asi` instances, the `.anm` list).
"Seen" means read in a file; "unproven" means no in-game run yet. No claim here
comes from memory.

## Verdict

1. **Reuse covers more than expected, with no Workbench work.** The base game
   ships CPR clips, a kneeling revive loop, unconscious poses, push/pull-vehicle
   loops, shovel and repair work loops, and seven commanding gestures. Script can
   play a gesture (`TryStartCharacterGesture`), a looping emote (`StartLoitering`
   with custom data), or any `.anm` directly on top of the graph
   (`PlayPostGraphAnimation`).
2. **Clone is graph-level only.** Base `.anm` files are binary and ship with no
   `.txa` source; nothing in Workbench or EBT turns an `.anm` back into a `.txa`
   or into a Blender action. We can retime, blend, mask and add procedural
   offsets in the Animation Editor, not edit keyframes.
3. **Create works and is documented, but needs a person in the loop.** Blender
   (not installed on this PC, EBT pins 4.5) exports `.txa`; Workbench with the
   net API on compiles it to `.anm`; a small text graph plays it. There is no
   documented way to bring a character rig into Blender from the base game
   (`wiki`: EBT imports FBX/P3D/ASC and *prefabs whose source FBX exist*, which
   excludes read-only Reforger assets).
4. **Recommendation:** ship M2 on reuse (revive, downed, hold-up, working loops),
   buy carry/drag/surrender/zip-cuff from ACE by license grant, and keep
   "create" for the pickaxe-style gaps. Section 4 has the table.

## 1. Reuse: what script can trigger today

### 1.1 The four routes

| Route | Call | Replication | Seen in |
|---|---|---|---|
| Gesture (arms/upper body, timed) | `SCR_CharacterControllerComponent.TryStartCharacterGesture(int gesture, int durationMS)`; `CanPlayItemGesture()` first | Native on the controller. LF already sends the call to the owning client (`LF_PlayInteractionGesture`, `Feature/Interaction/LF_PlayerInteractionController.c`) | `api_search TryStartCharacterGesture`; `Gestures.agf` |
| Emote / loiter (full body, loops until stopped) | `SCR_CharacterControllerComponent.StartLoitering(null, type, holster, rootMotion, align, matrix, disableInput, customAnimData)`, `StopLoitering(bool fast)` | Owner only (`if (rplComp && !rplComp.IsOwner()) return;`), then the owner RPCs `Rpc_StartLoitering_S` -> `Rpc_StartLoitering_BCNO` to everyone else | `SCR_CharacterControllerComponent.c` ~1560-1660 |
| Any `.anm` on top of the graph | `CharacterAnimGraphComponent.PlayPostGraphAnimation(ResourceName anm, string boneMask, float blendIn, float blendOut)`, `StopPostGraphAnimation`, `GetPostGraphAnimationRemainingTime` | Local to the machine that calls it; we would broadcast it ourselves (unproven) | `CharacterAnimGraphComponent.c` (generated API) |
| Item-use command (medic at a patient) | `ItemUseParameters.SetCommandID(...)` then `TryUseItemOverrideParams`; commands bound by name: `BindCommand("CMD_HealOther")`, `BindCommand("CMD_Revive")` | Engine item-use path, the same one the bandage uses | `SCR_ConsumableEffectHealthItems.c` 118-119, `SCR_ConsumableBandage.GetAnimationParameters` |

Bone masks for `PlayPostGraphAnimation` come from the character graph
(`player_main.agr`): `Body`, `Root`, `Camera`, `RProp`, `FacialMask`, `FacialBottom`,
`VO`. An unknown mask name errors and does not play (API doc).

### 1.2 Gesture IDs (graph `Gestures.agf`, state `CMD_Gestures`)

| ID | Gesture | Commanding config |
|---|---|---|
| 1 | Finger point | `Gesture_Point.conf` |
| 2 | Halt (raised open hand) | `Gesture_Halt.conf` (LF hold-up uses this) |
| 3 | Move out | `Gesture_MoveOut.conf` |
| 4 | Follow / move | `Gesture_Follow.conf` |
| 5, 6 | Mount, Mount2 (get in) | `Gesture_GetIn.conf` uses 6 |
| 15 | Salute (full-body at rest, additive when moving) | `Gesture_Salute.conf` |

Clips exist per stance: `anims/anm/player/actions/gestures/p_{erc,cro,pne}_gest_*`.
`EItemGesture` has only `None` and `PickUp`.

### 1.3 Loiter (continuous emote) types

`ELoiteringType`: `NONE 0, SIT 1, LEAN_LEFT 2, LEAN_RIGHT 3, SMOKING 4, LOITERING 5,
PUSHUPS 6, CUSTOM 7`; the rally DLC adds `CHEERING 9, BOOING 10, SIGN_HOLDING 11`.
`Configs/Commanding/Emotes/*.conf` are the ready-made `SCR_ContinuousLoiterCommand`
entries. `CUSTOM` plus `SCR_LoiterCustomAnimData` is the supported door for a
mod's own graph (section 3.3).

### 1.4 The clips worth knowing (all in `anims/anm/player/`)

- `injury/p_cro_revive_CPR_{in,loop,out}.anm`: **CPR.** Not referenced by any
  `.agf`, `.asi`, `.agr`, `.et`, `.conf` or script in the paks (searched `CPR`
  across all of them), so it is an orphaned asset. Playable only through
  `PlayPostGraphAnimation` or a graph we add.
- `injury/p_cro_revive_{in,loop,out}.anm`: kneeling treatment. Wired for
  `Bandage_US/USSR`, `Gauze_01` (`Assets/Items/Medicine/Workspaces/*_player.asi`);
  the bandage graph (`Bandage_US.agf`) has a `Revive` state driven by
  `CMD_Revive == 1` with in/loop/out, and `CMD_HealOther`. `p_cro_revive_tourniquet_*` likewise.
- `injury/p_unconscious_{back,front_up,front_down,Lside,Rside}.anm` plus `*_to_prone`:
  the patient poses. `Unconscious.agf` has `UnconsciousPose` 1-5 and the tags
  `TagRevive` / `TagCanRevive`, so the patient already lies still and reacts to a revive state.
- `actions/vehicleRecovery/npc_{push,pull}_car_*`: two-handed push and pull loops (NPC rig, same skeleton family).
- `actions/supplies/build/*shovel*`, `*_demolition*_loop`, `supplies/repair/p_erc_supplies_repair_{low,high}_loop`: bent-over working loops (item-IK variants need the item).
- `actions/sit_bed/*`, `NPC/male/pushup/*`, `Tutorial/...`: other loops; `editor_poses/poses/4_wounded.anm`.
- **Not in the game:** carry, drag, surrender or hands up, handcuffed or tied, kneeling, lockpick, drill, hammer, saw
  (asset searches for each returned nothing).

## 2. Clone: copy and modify a base animation

| Want | Possible? | How / evidence |
|---|---|---|
| Slow down or speed up | Yes | `AnimSrcNodeTimeScale` (`TimeExpr` multiplier) in the graph; also `PlaybackSpeed` in the editor preview (wiki `Animation Editor: Nodes`) |
| Blend, layer, mask | Yes | `AnimSrcNodeBlend`, `BlendT`, `BlendTAdd` (additive), bone masks in the control template (used 95+83+15 times in the player graphs) |
| Nudge bones (raise an arm, offset a hand) | Yes | `AnimSrcNodeProcTransform` / `ProcTrBoneItem` (hundreds in `Locomotion.agf`), plus IK targets |
| Mirror left to right | **Not found** | No mirror node type appears in the player graphs or the node reference page; unproven whether any node mirrors |
| Edit keyframes, cut or join clips | **No** | Paks hold 13,167 `.anm` and zero `.txa`; EBT has export only (`ebt.export_txa*`, no import operator in `txa/` or `core/txa/`); the Animation Editor "rebinarise TXA" tool compiles `txa` to `anm`, never the reverse |
| Duplicate a workspace to our addon | Yes | Animation Editor: `File > Duplicate Workspace` (copies `.aw/.agr/.agf/.ast/.asi`); `Duplicate to addon` on a template (wiki `Weapon Animation/Setup`) |

So "clone" means: duplicate a small workspace such as `player_rally_emotes`
(`.aw` 561 B, `.agr` 460 B, `.ast` 508 B, `.agf` 7.9 KB, `.asi` text), point the new
`.asi` at base `.anm` GUIDs, and shape them with time-scale, masks and procedural
nodes. The base `.anm` stays a dependency on the base game, not a copy. Cutting
keyframes needs a re-animation (section 3).

## 3. Create: new clips from outside

### 3.1 The chain that exists

1. Pose a rig in Blender 4.5 with Enfusion Blender Tools. EBT in the Reforger Tools
   install (`Blender/scripts/addons/EnfusionBlenderTools`, build 37) has
   `Enfusion Tools > Export > Enfusion Animation (.txa)`, per-action export
   profiles, additive "diff pose" export, an NLA strip baking tool, and a Rig and Skeleton updater.
2. Workbench with **Enable net API** on. EBT writes the `.txa`, registers it
   and Workbench compiles the `.anm` (re-compiles on every `.txa` change).
3. Put the `.anm` in an `.asi` slot of a workspace, wire it into a graph state,
   or skip the graph and call `PlayPostGraphAnimation` on the `.anm` directly.

### 3.2 Blockers (found, not guessed)

| Blocker | Evidence | Effect |
|---|---|---|
| **No character rig in Blender.** | EBT imports FBX, P3D, ASC and prefab models only where source FBX exist (read-only vanilla excluded, wiki `Enfusion Blender Tools`); the paks hold `.xob` (no FBX) and no `.blend` rig; `AnimTestChar_US_01.xob` is the editor preview model | We need a Reforger-skeleton rig. Routes: the Workbench model export/FBX route, ACE's rig, or EBT's own rig tools (**unverified**). This is the first thing to prove |
| Blender is not installed here | `docs/research/asset-pipeline.md`: only a stray `4.3` folder; EBT needs exactly 4.5 | 0.5 day to install and doctor, shared with the asset pipeline |
| Export profiles are per rig | wiki: "selecting proper export profile is crucial... custom rig may need custom profiles" | Use the shipped player profiles; they are dozens |
| Net API needs a live Workbench | wiki + EBT | The same hidden Workbench `tools/film` and `agent-box` already run |
| Authoring needs eyes | No renderer-free check of a pose | Agent can render Blender previews and a Workbench Animation Editor capture; the final check is a film in third person (the epic's own done-when) |

### 3.3 Wiring a new clip into the character (two routes, both seen in vanilla)

- **Post-graph (cheapest).** `PlayPostGraphAnimation(anm, "Body", 0.2, 0.2)`
  needs no graph at all. Unproven for a looping working pose and for other
  clients; the call plays locally so a server-to-all RPC must trigger it.
- **Attachment graph (the supported way for loops).** The player graph has a
  built-in `AnimSrcNodeAttachment` bound to `"NPC"` (queue `NPCAttachmentQ`,
  interrupted by `Gestures == -2`), made for exactly this. `SCR_LoiterCustomAnimData`
  carries `m_sGraphName` (`.agr`), `m_sGraphInstanceName` (`.asi`),
  `m_sGraphBindingName` (default `NPC`), `m_sCommandName` (`CMD_Gestures`),
  `m_sControlVariableName/Value`, `m_bAllowRootMotion`, item preset. The rally
  emotes are the worked sample: `Emote_HoldSign.conf` sets `m_eLoiterType
  SIGN_HOLDING`, graph `player_rally_emotes.agr`, instance `player_rally_emotes.asi`,
  variable `Gestures` = 502. A mod copies that triple of files plus its own `.anm`
  and a `SCR_ContinuousLoiterCommand` config. `CharacterAnimGraphComponent.SetAttachment(binding, agr, asi, node, managed, copyInputs)`
  does the same from script.

### 3.4 Proven in game (#512, 2026-11-10)

- **Post-graph does not work on the player.** `PlayPostGraphAnimation` lives on
  `CharacterAnimGraphComponent`, which `Character_Base.et` does not carry (only
  `SCR_CharacterAnimationComponent`). The "cheapest" route in section 3.3 is closed for characters.
- **The attachment graph works and replicates.** `Anims/Workspaces/Medic/LF_ReviveCPR.{agr,ast,agf,asi,aw}`
  copies the rally emote layout (`MasterControl` state machine, `Gestures` control variable,
  in/loop/out) and points at the CPR clips. `StartLoitering(CUSTOM, ...)` on the owner showed
  the medic kneeling on both clients of a two-client session (head bone 0.99 m against 1.6 m
  standing). The rally graph itself is not usable: the `rally` addon is not loaded on our
  server (`Wrong GUID/name` for every rally resource).
- **Base clip GUIDs** come from `addons/data/resourceDatabase.rdb`: each path is followed by
  6 bytes and then its GUID as 8 little-endian bytes. CPR: in `53DD1932ED812D70`, loop
  `0CCED80870DD8150`, out `226002617FC7B0AB`.
- **A clip without a `Gesture_End` event never ends a soft loiter exit**
  (`SCR_CharacterCommandLoiter` waits for it while a graph is attached). The CPR clips carry
  none, so stop with `StopLoitering(true)` (Gestures -2 detaches at once).

## 4. Recommendation per M2 need

| Need | Choice | What | Effort | Risk |
|---|---|---|---|---|
| **Medic revive / treatment (#427)** | **Reuse** | A. Revive through the vanilla item-use path: build `ItemUseParameters` with `CMD_Revive` (arg 1) for the field dressing the revive already consumes, giving the kneeling `p_cro_revive_*` in/loop/out on the medic, in sync with the item. B. For CPR proper, `PlayPostGraphAnimation` on `p_cro_revive_CPR_in/loop/out` (orphaned clips) via a broadcast RPC | A: 1 day. B: 1 day after A | A: `CMD_Revive` has no script caller in the base game (only the binding), so the graph path is proven only on paper. B: CPR clip rig fit is unproven. Proof: one session, film both |
| **Downed player lying still** | **Reuse (already)** | Vanilla unconsciousness already plays `p_unconscious_*` poses (`UnconsciousPoseMaster`, tags `TagCanRevive`). Nothing to build; check the posture on a film and keep `TagRevive` in mind for the revive handshake | 0.25 day to film and confirm | Low |
| **Carry / drag pose (#430)** | **Create, or buy** | Nothing in the base game. ACE-Anvil has carried and dragged in/loop/out (GPL-2.0; docs/ecosystem.md says ask for a permissive grant). Interim: reuse `npc_pull_car_*` for the carrier's drag grip while the body keeps its unconscious pose | Buy: ask now, wire 2 days. Create both sides (carrier plus patient clips, attachment graph, attach points): 5-8 days after the rig blocker is cleared | Two-body sync is the hard part, not the clips. Grant may not come |
| **Hold-up gesture (#408)** | **Reuse** | Already live: gesture 2 (Halt). A "hands up" for the *victim* does not exist; ACE has surrender. If the victim pose matters, treat it as the captive set below. Option now: Halt on the robber, victim unchanged | 0 (done) | None for Halt |
| **Drill / lockpick working loop (#273, #411)** | **Clone** | Compose from reused clips: `p_erc_supplies_repair_low_loop` (hands-in-work, bent), or the demolition loop, via `PlayPostGraphAnimation(..., "Body")` or a small attachment workspace duplicated from `player_rally_emotes`. Retime with `AnimSrcNodeTimeScale`; add a hand offset with `ProcTransform` | 2-3 days (workspace + two states + film) | The repair/demolition loops expect a tool; a tool-less look may be off, in which case spawn the vanilla repair kit as a prop in hand or create |
| Captive set (surrender, tied, zip-cuff) | **Buy, else create** | ACE-Anvil captives; not in the base game. Create: 3 loops plus in/out, 6-10 days | Same grant question as carry | GPL: do not copy without a grant |
| A pickaxe swing (epic text) | **Create** | No melee tool swing exists beyond the entrenching shovel set; start from the shovel `*_IK` clips only if the swing can be a retime | 3-5 days after the rig works | Rig blocker |

**Order of work.** (1) One session proving `PlayPostGraphAnimation` with a
CPR clip, `TryStartCharacterGesture(2)` and `CMD_Revive` item use, filmed
(5 below). (2) Ask ACE for the `.anm` grant now; its answer decides carry,
drag and captives. (3) In parallel, an agent proves the Blender leg: install
Blender 4.5, get a Reforger-skeleton rig, export one action as `.txa`, and see
a compiled `.anm` in the Animation Editor. That one run is the epic's gate.

## 5. Proof status

The optional session proof was skipped: `session.mjs up --clients 2` reported
`Life-Framework-ws-area-7-hospital has a live session; queued (up to 30 min)`.
`session.mjs interaction` (`use LF_IntroduceAction`) already drives Halt (gesture 2)
between two clients, so the gesture route is the
lowest-risk part. The follow-up proof should add one harness verb that calls
`PlayPostGraphAnimation` on a CPR clip for every client and films the medic.

## Open questions (each is one cheap run)

- Does `PlayPostGraphAnimation` with `Body` look right for a CPR/working clip on a player rig in third person, and do late joiners see it?
- Does `ItemUseParameters` with `CMD_Revive` start the kneeling revive on a downed player who is held `TagCanRevive`?
- Which tool exports the Reforger character skeleton to Blender (Workbench, EBT rig updater, or ACE's rig)?
- Can a mod's workspace attach on `NPC` while LF's own state (downed, carried) is active?

## Source index

- Pak files read: `anims/workspaces/player/{Gestures,Unconscious,Locomotion}.agf`, `player_main.{agr,asi}`, `player_rally_emotes.{agr,agf,asi,ast,aw}`, `Assets/Items/Medicine/Workspaces/Bandage_US.agf` and `*_player.asi`, `Configs/Commanding/{Gestures,Emotes}/*.conf`, `scripts/Game/Character/SCR_CharacterControllerComponent.c`, `.../Commands/SCR_CharacterCommandLoiter.c`, `SCR_LoiterCustomAnimData.c`, `scripts/Game/Commanding/Commands/SCR_EmoteBaseCommand.c`, `SCR_BaseContinuousEmoteCommand.c`, `scripts/Game/Components/Gadgets/SCR_Consumable*.c`, `scripts/GameLib/generated/Components/Character/CharacterAnimGraphComponent.c`, `Prefabs/Characters/Core/Character_Base.et`.
- Wiki: `Animation Editor`, `Animation Editor: Nodes`, `Animation Editor: Character Action Commands`, `Weapon Animation/Setup`, `Weapon Animation/Basic Tutorial`, `Enfusion Blender Tools`, `Enfusion Blender Tools: Import/Export Animation`.
- Local: EBT `animation_export`, `txa`, `core/txa`; `docs/ecosystem.md`; `docs/research/asset-pipeline.md`; `LF_PlayerInteraction.PlayGesture`.
