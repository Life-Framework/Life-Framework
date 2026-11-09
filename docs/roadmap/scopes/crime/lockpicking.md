# Lockpicking

Milestone: M2
Extends: door locks (`Houses`, `LF_LockComponent`), vehicle locks and keys, crime alerts, [forced entry](forced-entry.md)

## Player loop

A player with a lockpick works a lock by hand, quietly, instead of blowing it open. It is slower than a key and quieter than a charge, but it can fail and it leaves a trace. It works on doors and on vehicles.

## In scope

- A lockpick item, bought from the fence or a similar seller, with a use count that wears out.
- A held action at a locked door or vehicle door: the lock opens after a skill-based time. A better skill is faster. A failed attempt can snap the pick.
- Doors opened this way stay opened for that visit; the owner's lock is not destroyed. A vehicle opened this way can be entered but needs its start handled by the [chop shop](chop-shop.md) loop (a hotwire step) before it can be driven away.
- Noise: an attempt can be heard nearby and can raise witnesses and a crime report, using the existing notify path. It does not alert the police by itself unless a witness reports it.
- Targets configure whether they can be picked (a bank vault or jail door cannot).
- The server decides success, wear and noise; the client only asks.

## Out of scope

- A lockpicking minigame screen. The action is physical and held, with animation and sound.
- Picking containers and safes (later).
- Skill trees beyond one lockpicking skill.

## Assets

The pick is a base-game item prefab chosen in config. Animation and sound start from the base game; a missing animation falls back and is logged.

## First slice

One DebugWorld house door and one parked vehicle. A player with a pick opens each, the pick wears, a nearby NPC witness hears it, and a locked bank vault door refuses the pick with a logged reason.

## Depends on

- `Houses` lock state, vehicle locks and keys
- `Crime` notify and, for witnesses, the M2 investigation system
- A skill system entry for lockpicking

## Proof

- Logic: skill changes the time; a worn pick breaks; a non-pickable target refuses.
- World: both targets on DebugWorld.
- Manual: the witness reaction and the held-action animation.
