# Events

Phase: After release
Extends: notifications, the world markers used by jobs and missions

## Player loop

The server starts a timed world event. Players hear about it through the existing notification path and deal with it at a place in the world. When the timer ends, the event cleans up.

## In scope

- A scheduler: config list, duration, one active event.
- One example that reuses a release loop, such as a marked contraband drop or a disabled vehicle, using base-game props.
- Cleanup on end and on server start, so a crash does not leave a permanent event entity without an owner.

## Out of scope

- Natural-disaster simulation, custom event maps, or scripted cinematic scenes.
- Events that mint money without a player action at the site.

## Assets

One example can reuse a release site: `FuelStation_E_01`, the hideout stash `ShineStash.et`, or a `S105` left at a marker. Announce with `EL_NotificationManagerComponent`. LNGC has map markers for shops and dispatch, and no event scheduler. See [sources](../../sources.md).

## First slice

One event type, announced to civilians or police by job, despawned when the timer ends.

## Depends on

- `Notifications`
- A release loop to attach to (robbery site, convoy drop, or a job marker)

## Proof

- A second event does not start while one is active.
- Restart with no active event leaves no orphan props from the last run.
