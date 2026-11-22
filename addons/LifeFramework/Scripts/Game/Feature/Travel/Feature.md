# Travel

## Intent

A player who has no car yet can still get around: walk up to a bus stop sign, pay a small cash fare, wait at the stop for the bus and get off at another stop. It is the simple version: no bus drives the road; a fade covers the trip.

## Interaction pattern

In-world first. The player walks up to a bus stop sign (`Prefabs/Travel/LF_BusStop.et`, the vanilla `SignBusStop_01` with `LF_BusStopComponent` and `LF_BusStopAction`). F offers *Take the bus*, which opens the timetable: a UI-kit sheet listing every other stop, nearest first, with its name, fare and wait. *Buy ticket* asks the server. The wait is a hint on screen; walking off, a fight, damage, going down, cuffs or getting into a vehicle cancel it and refund the fare.

## Bus stop

- **Player sees:** *Take the bus* on the sign; the timetable; "Ticket paid ($15). The bus comes in 20 s: wait at the stop."; a fade to black and back at the far stop, standing on the pavement beside the sign.
- **Owner and side:** `LF_BusStop` decides on the server (fare, refusals, the wait, the refund, the move through `LF_Jail.MoveBody`). `LF_BusArrival` finds the set-down spot (ground by `LF_PlaceSnap.Down`, off the road by `LF_PlacementZones.IsOnRoad`, a 0.35 m capsule with head room, rings out to 2.25 m). `LF_BusStopSheet`, `LF_BusStopAction` and the fade (`SCR_FadeInOutEffect`) are client only. RPCs: `RpcAsk_LF_BusTrip` (stop ids, so a sign needs no RplComponent), `RpcDo_LF_BusResult`, `RpcDo_LF_BusFade` on `SCR_PlayerController`.
- **Data:** `Configs/Travel/LF_BusStops.conf` (`LF_BusStopConfig`: base fare, fare per km, fare cap, wait, wait per km, wait cap, stop reach, cancel radius, ask and trip cooldowns, pursuit radius, fade), overlaid by a pack (`LF_Everon1989/Configs/Travel/LF_BusStops_Everon1989.conf`). No owner section: the client prices the timetable from the same files the server charges by. Stops register in `LF_Places` as kind `BusStop` (id: the placed `m_sStopId`, else the entity name); the map draws that kind from the pack's `LF_MapPois_BusStop.conf` row. System switch `Travel`. Nothing is persisted; the cash change is the only lasting effect.
- **Refusals:** dead, downed, cuffed, escorted or jailed, in a vehicle, in a fight (`LF_CharacterSwitch` combat window), wanted with an on-duty officer within the pursuit radius, not within reach of the sign, not enough cash, a ticket already waiting, inside a cooldown, an unplaced or same stop. Each logs `[LFDebug:Travel]` and shows its own line on the sheet.
- **Proof:** `travel/bus-stop` (LOGIC), `travel/bus-stop-world` (WORLD), `node agent-box/session.mjs bus-stop-proof` on `--mission grid` (cell `bus-stops`, two stops).
