# Phases

Phases are playable cuts, in order. A later phase does not start by skipping the proof on the earlier one.

| Order | Phase | Gate |
| --- | --- | --- |
| 1 | [Alpha](alpha.md) | A small group finishes a session on the loops already in the repo |
| 2 | [Release](release.md) | The release scopes are playable together on 1989 Everon with base-game assets |
| 3 | [After release](after-release.md) | Bugs from play are fixed, then the later scopes |

## How a phase is tested

1. **Alpha** is a small multiplayer group. The aim is to see that character, money, work, police, and persistence hold up on a dedicated server, including a save and a restart.
2. **Wider testing** opens only after that group can play those loops without them breaking.
3. **Release** is the public cut: the [release scopes](release.md), still on native assets.
4. **After release** keeps fixing what players hit, then adds the [later scopes](after-release.md).

In-session checks do not prove a restart. A host-only run does not prove a dedicated server. See [design principles](../../design-principles.md) on proof classes.

## What a phase is not

A phase is not a content pack. Weapons, cars, and aircraft stay the ones Arma Reforger already ships. Server owners can add asset mods on top of the framework; the base mod does not require them.
