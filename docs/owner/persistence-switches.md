# Persistence switches

The `persistence` section of `config.json` decides which player state survives a server restart.
Every switch is on by default, which is how the server behaved before the switches existed. Leave a
key out to keep it on.

```json
{
  "schemaVersion": 1,
  "persistence": {
    "keepCash": true,
    "keepCarriedGear": true,
    "keepSurvival": true,
    "keepWanted": true,
    "keepJail": true,
    "keepLicences": true
  }
}
```

| Key | Off means, after a restart |
|---|---|
| `keepCash` | Cash on hand is gone from every character. The bank balance, and cash in a locker or stash, stay. |
| `keepCarriedGear` | Every character respawns with the starting loadout. Cash on hand goes with the gear. Lockers and stashes stay. |
| `keepSurvival` | Hunger and thirst start full. |
| `keepWanted` | Every warrant is cleared, on every character of every account, and the wanted posters come down. |
| `keepJail` | Every prisoner is free. This includes a sentence started by logging out cuffed ([disconnect rules](disconnect-rules.md)). |
| `keepLicences` | Characters keep only their starter licences; every other licence is gone. |

How it works: the save always holds every field. When the server loads it, each switched-off
field is reset to its default, and the next save writes the default. So:

- Changing a switch takes effect at the next restart. Nothing changes while the server runs.
- Turning a switch off and back on before the next save brings the field back: the save still
  had it. After a save with the switch off, the field is gone for good.
- Your existing saves load with any combination of switches. No save is invalidated.

Bank balances, vehicles you own, organizations and everything else in the save always persist. To
wipe them, delete the save (`WipeSave`). Position, vehicle fuel, damage and trunk contents are not
saved today, so they have no switch.

The server logs one `[LFDebug:Persistence] keep<Field> off: ...` line per reset field at each load,
with how many records it reset.
