# Interaction — Design Requirements

> Context file for person-to-person actions. Written against `docs/decisions.md`
> ("One player interaction menu", "Names show up close, after an introduction",
> "In-world first").

## Intent

Walking up to another player offers one short list, every entry one press:
Introduce yourself, Show ID, Give cash, Give what you hold, then role actions
when the role allows (on-duty police: Search, Arrest; `fine` and `org_invite`
slots stay hidden until a handler registers). Strangers show no name. After
someone introduces themselves you see their name only while you look at them
from a few metres away. Introductions survive a restart.

## Interaction pattern (in-world)

Rung 1-2. The list is the vanilla action menu on the other character's body
(`Character_Roleplay.et`, context `LF_ID` at chest height). Cash and items are
real inventory items that change hands; the giver's arm moves (base-game
gesture) and the receiver hears the hand-over. The only UI is the amount
sheet, because typing an amount is a correct UI. The name is a small line
under the crosshair, never a tag over heads.

## Current state

- `LF_PlayerInteraction` — reach rule (`REACH` 2 m, both alive, a player,
  not yourself), the offer table, amount parsing, role registry
  (`RegisterRole(slot, handler)` / `UnregisterRole`), and the server flows
  (introduce, open the cash sheet, give cash, give item, role perform).
- `LF_HoldUpRole` fills the `holdup` slot (#244, `Feature/Moderation/LF_HoldUp.c`): offered with a weapon in hand.
- `LF_CashHandover` / `LF_CashWallet` — the hand-over rule; what the receiver
  cannot take goes back to the giver.
- `LF_Introductions` — who knows whom by player uid; the `introductions`
  persistence payload (save version 8).
- `LF_KnownNames` — client cache of names this player was told and the 200 ms
  look check (`NAME_RANGE` 4 m, 15 degree cone) that shows one name.
- `LF_GiveCashSheet` — the amount sheet (`GiveCashAmount`, `GiveCashButton`).
- `LF_IntroduceAction`, `LF_GiveCashAction`, `LF_GiveItemAction`,
  `LF_PlayerRoleAction` — the entries. `LF_ShowIDAction`,
  `LF_ConfiscateAction`, `LF_JailArrestAction` use the same reach rule.
- Bridge: `SCR_PlayerController` RPCs in `LF_PlayerInteractionController.c`;
  names re-sync on every spawn (`LF_GameModeRoleplay.OnPlayerSpawned`).

## Animation and sound

- Introduce: `Gesture_Halt` (id 2, raised open hand), configurable per action
  (`m_iGestureId`). No vanilla greeting voice line fits a 1989 civilian; none
  is played.
- Give cash / give item / show ID: `Gesture_Point` (id 1, arm out; plays with
  an item in hand) and `SOUND_PICK_UP` at the receiver.
- A gesture plays only when the owner's controller allows it
  (`CanPlayItemGesture`); otherwise it is skipped and logged.

## Iteration path

- M1-19 (#104) registers `fine`; M1-17 (#102) registers `org_invite`. Neither
  edits `Character_Roleplay.et`.
- V2: hand-over animation with the item visible; a greeting voice set.
