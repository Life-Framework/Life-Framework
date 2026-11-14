# Emote

## Intent

A player can gesture and hold a pose (salute, point, halt, come here, sit on the ground, lean,
smoke, idle, push-ups) from a radial, and sit on the town's chairs and benches with the F key.
Everyone near them, late joiners included, sees the same thing.

## Interaction pattern

In-world first: a seat is the object. Walk up to a chair or bench and the F key offers *Sit*
(one per place on a bench); while seated the same key reads *Stand up*. Jump, sprint or raising a
weapon also stands up (base game loiter). The emote radial is the base game's radial (hold F1, or
hold right shoulder + view on a gamepad); nothing LF-drawn opens.

## Emote radial (#982)

- **Player sees:** hold F1 → a radial with two pages, *Gestures* and *Emotes*. Release on an entry to
  play it. Greyed with "Not while restrained" when cuffed or stunned.
- **Owner and side:** client only. `SCR_PlayerControllerCommandingComponent` (base game) opens
  `Configs/Emote/LF_EmoteMenu.conf` from the input pair in `LF_EmoteMenuActions.conf`
  (`LF_OpenEmoteMenu`, set on `PlayerControllerRoleplayMP.et`). The entries are the base game command
  confs, listed in `LF_EmoteCommands.conf` on the game mode's `SCR_CommandingManagerComponent`. They
  are local-only commands: the owner plays the gesture (`TryStartCharacterGesture`) or starts the loiter
  (`StartLoitering`), and the base game replicates both.
- **LF changes to the base game** (`LF_Emote.c`): `ExecuteCommand` runs a local-only, non-group command
  without the AI group the base game requires (roleplay has no groups manager); `SCR_EmoteBaseCommand`
  and `SCR_ContinuousLoiterCommand` refuse a restrained or stunned body; a dedicated server skips
  loading the command list, because the dedicated server build lacks those client classes and would
  log `Unknown class 'SCR_EmoteBaseCommand'` for every entry.
- **Data:** the menu and command list are data; nothing persists.
- **Proof:** `emote/sit` (LOGIC), `emote/sit-world` (WORLD), `node agent-box/session.mjs sit-proof`.

## Sit (#982)

- **Player sees:** *Sit* on a chair or bench; the character turns, sits in the base game chair pose
  (no prop chair spawns) and *Stand up* gets them up. A taken place reads "Someone is sitting here".
- **Owner and side:** `LF_SitAction` (a `SCR_LoiterUserAction`) is broadcast. The server writes the
  place (`OnSaveActionData`), the sitter's owner starts the loiter, and the base game sends it to the
  others (`Rpc_StartLoitering_BCNO`) and to late joiners (`SCR_CharacterCommandHandlerComponent`
  `OnRplSave`). The seat taken state is the base game occupancy (`SCR_UserActionWithOccupancy`).
- **Data:** each place is an action on the seat prefab: `m_vSeatOffset`, `m_fSeatYaw` and the pose
  (`m_SitAnimation`, the base game chair sit, command value 7). Seat prefabs:
  `Prefabs/Props/Seats/LF_Seat_*.et` inherit the base game benches and chairs. A map or pack makes any
  prop a seat by adding an `ActionsManagerComponent` with `LF_SitAction`s. Nothing persists.
- **Proof:** as above.
