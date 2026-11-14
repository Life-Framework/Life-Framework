# Disconnect rules

A player must not escape an arrest, a death or a sentence by logging out. The `disconnect` section
of `config.json` decides what logging out does in each state. The shipped values are below; leave
a key out to keep it.

```json
{
  "schemaVersion": 1,
  "disconnect": {
    "cuffedRule": "jail",
    "escortedCountsAsCuffed": true,
    "cuffedWantedAdd": 1,
    "cuffedMinJailSeconds": 300,
    "cuffedFine": 1000,
    "cuffedFineAllowsDebt": true,
    "downedDies": true,
    "wantedLogoutAdd": 0,
    "jailClockOffline": false
  }
}
```

| State at logout | Shipped rule | Keys |
|---|---|---|
| Cuffed | Jailed. The sentence starts at logout and is served from the rejoin, which spawns in the cell with the sentence slip. One wanted level is added for leaving custody. The sentence is the one for the new wanted level, never below `cuffedMinJailSeconds`. With no cell on the map the fine applies instead. The cuffs go back to the officer. | `cuffedRule` (`jail`, `fine` or `release`), `cuffedWantedAdd`, `cuffedMinJailSeconds`, `cuffedFine`, `cuffedFineAllowsDebt` |
| Escorted by an officer (on foot or in a car) | Same as cuffed | `escortedCountsAsCuffed` |
| Downed | Dies. The death counts as if they bled out: a murder goes to whoever downed them if that player is online, and the rejoin wakes at the hospital. A stunned player never dies this way. | `downedDies` |
| Wanted | The warrant stays on the character and is saved. | `wantedLogoutAdd` raises it further |
| Jailed | The time left stays and is saved. Only online time is served. | `jailClockOffline: true` lets the clock run while they are away |

`fine` takes `cuffedFine` from the bank; with `cuffedFineAllowsDebt: false` it takes only what the
bank holds. `release` takes the cuffs off and nothing else.

## What you see

Each flagged logout writes one line to the server log and one row in the Game Master incident view
(kind "Logout in custody"):

```
[LFDebug:Disconnect] <uid> left cuffed,wanted1: jail min=300s, wanted+1, warrant kept
[LFDebug:Disconnect] <uid> jailed for leaving custody: 300 s, served from the rejoin in the cell
```

A server restart counts as a logout: a player cuffed when the server stops is jailed on the next
join under the jail rule.
