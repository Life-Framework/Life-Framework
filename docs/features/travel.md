# Travel

### `LF_BusStop` — `Feature/Travel/LF_BusStop.c` (2026-11-22)
- ✅ Bus stops: at a bus stop sign, *Take the bus* lists the other stops with fare and wait; pay cash, wait 20 to 30 s at the stop, fade, and you stand at the far stop. Walking off, a fight, damage, downed, cuffs or a vehicle cancel the wait and refund it. Fare and wait are config (`Configs/Travel/LF_BusStops.conf`, pack overlay). Tests: `travel/bus-stop` (LOGIC), `travel/bus-stop-world` (WORLD); session proof `bus-stop-proof` (grid cell `bus-stops`).
