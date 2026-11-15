# Day cycle

The `dayCycle` section of `config.json` decides how fast the in-game clock runs. There are three modes.
The default is `vanilla`: leave the section out and the base game's clock runs untouched.

| `mode` | The in-game clock |
|---|---|
| `vanilla` | The base game's own. Nothing is changed. |
| `accelerated` | One in-game day lasts `dayLengthMinutes` real minutes. |
| `realtime` | Follows the server's real clock, shifted by `clockOffsetMinutes`. |

## Vanilla

The default. Every other field is ignored.

```json
{
  "schemaVersion": 1,
  "dayCycle": {
    "mode": "vanilla"
  }
}
```

## Accelerated

A day lasts `dayLengthMinutes` real minutes. The clock runs `1440 / dayLengthMinutes` times as fast as
the base game's 24-hour day, so a 60-minute day is 24x and a 120-minute day is 12x.

```json
{
  "schemaVersion": 1,
  "dayCycle": {
    "mode": "accelerated",
    "dayLengthMinutes": 60,
    "nightSpeed": 1,
    "startHour": 8
  }
}
```

`nightSpeed` multiplies the clock's speed at night. With a 120-minute day, `"nightSpeed": 2` runs the
night at 24x, so the dark passes in half the real time. `1` keeps one speed all day.

`startHour` is the in-game hour a fresh world starts at, `0` to `24`. A save that already has a time
of day is a different case: see [what a restart does](#what-a-restart-does).

## Real time

The in-game clock follows the real clock of the machine the server runs on. `clockOffsetMinutes` is added
to it, so a server hosted in one timezone can show another's evening.

```json
{
  "schemaVersion": 1,
  "dayCycle": {
    "mode": "realtime",
    "clockOffsetMinutes": -300
  }
}
```

`dayLengthMinutes`, `nightSpeed` and `startHour` are ignored: the real clock sets the hour and a day lasts a
real day. A negative offset shows an earlier hour than the machine's clock, a positive one a later hour.

## Fields

| Field | Used by | Range | Default |
|---|---|---|---|
| `mode` | all | `vanilla`, `accelerated`, `realtime` | `vanilla` |
| `dayLengthMinutes` | `accelerated` | 1 to 10080 | 1440 |
| `nightSpeed` | `accelerated` | 0.1 to 10 | 1 |
| `startHour` | `accelerated`, a fresh world | 0 to 24 | 8 |
| `clockOffsetMinutes` | `realtime` | -1440 to 1440 | 0 |

A bad value never stops the server. An unknown `mode`, or a number outside its range, is refused: the log says
which field, and the default stays. Leave a key out to keep its default.

## The flagship's day

The Life Framework | Everon 1989 server runs a 2-hour day, in `server/flagship/config.json`:

```json
"dayCycle": {
  "mode": "accelerated",
  "dayLengthMinutes": 120
}
```

- One real minute is 12 in-game minutes; one in-game hour is 5 real minutes.
- `nightSpeed` and `startHour` are not set, so they keep their defaults (1 and 8).
- The server restarts at `06:00` and `18:00`, 12 real hours apart, which is six in-game days.

The core's default stays `vanilla`. Only this config chooses 120 minutes, so a server of your own does not
inherit it.

## What a restart does

*(planned, #1302)* The restart behaviour below lands with [#1302](https://github.com/Life-Framework/Life-Framework/issues/1302)
and is not yet in the game.

- `accelerated`: the time of day is saved and the clock resumes from it, so a 06:00 restart does not send the
  world back to `startHour`.
- `realtime`: the real clock wins on load. The saved time of day is not used.
- `vanilla`: nothing is saved or applied by this section.

Until #1302 merges, do not rely on the time of day surviving a restart; assume an accelerated day begins
again at `startHour`.

Changing `dayCycle` takes effect at the next restart. Restart the server after editing the file.
