# How to join the police in the alpha

The path from a civilian to an officer on "Life Framework | Everon 1989
(Alpha)": the application, the play-time gate, what a rejection costs, how the
maintainer decides, the trial, and going on duty. Issue: #1467. For getting
onto the server in the first place, read [the alpha page](alpha.md).

Police missions are not part of the alpha opening. They come after it, so this
page promises none. Officers do the police work that is already in the game:
arrests, fines, the wanted board, and site alarms.

## 1. Apply at the station desk

Walk up to the police application desk at the station and use **Apply to the
police**. You must stand within `applyDeskRange` (6 m) of it for the form to open.

You need a registered character first; without one the desk says "Register at
the civil registry first".

The form has three blanks, and all three are required:

| Blank | Limit |
|---|---|
| Your name | 48 characters |
| Why you want to serve | 240 characters |
| When you can be on duty | 80 characters |

You get one application at a time. If you already have one waiting, or you are
already on the police roster, the desk refuses a second one.

There is no Discord account linking. Nothing on Discord is part of the
application, and the maintainer does not need your Discord name to decide it.

## 2. The play-time gate

An account needs `applyMinPlaytimeHours` hours of play, all jobs together,
before it may apply. The shipped default is **50 hours** (`m_iApplyMinPlaytimeHours`
in `LF_PoliceConfig`), and the shipped config `LF_Police.conf` sets the same 50. A server that overrides the owner config sets it under `police` in
`config.json` (see `docs/config/config.example.json`). 0 turns the gate off.

If you are short, the desk tells you the time left, for example "You need more
play time before you can apply. Time left: 12h 30m." Play, then come back.

## 3. The rejection cooldown

If the maintainer rejects your application, you wait `applyRejectCooldownHours`
before you may apply again. The default is **72 hours** (3 days), the same in
`LF_PoliceConfig` and `LF_Police.conf`. 0 turns the cooldown off.

During the cooldown the desk says "Your last application was rejected. You can
apply again in" and the time left. The play-time gate still applies after it.

## 4. The maintainer reviews it

Only the maintainer is staff for this at the opening, so the maintainer reads
every application and approves or rejects it. The desk review takes the application
that has waited longest. No bot or vote decides, and an unanswered application stays open. There is
no promised reply time: ask in #general on [Discord](discord.md) if it has been
a while.

What helps your application: a real reason in "Why you want to serve", and
hours you can really keep in "When you can be on duty".

When it is approved you see "Application approved. Clock in at the duty desk."
and you are on the police roster. If it is rejected, the cooldown above starts.

## 5. The trial

A new officer is on trial for `trialHours`. The default is **168 hours**
(7 days, counted from the approval; the clock runs whether you are online or
not), the same in `LF_PoliceConfig` and `LF_Police.conf`. 0 means no trial.

A trial ends in one of two ways:

- **You are promoted** (patrol to senior is the first step of the rank ladder)
  before the 168 hours are up. The trial ends for good and you stay on the roster.
- **The time runs out unpromoted.** The roster entry ends. You are a civilian
  again and must apply from step 1, with the play-time gate and no rejection
  cooldown (the application was approved, not rejected).

## 6. Going on duty at the locker

The duty desk at the station is where you clock in and out. **Clock in** swaps
your whole carried inventory, clothes included, for your police gear. Your
civilian things wait in a stash. **Clock out** swaps them back. The first time you
clock in you are issued the starter police kit.

Two things to know:

- If you die on duty, the shift ends. You respawn with your civilian
  inventory and the next clock-in issues the starter kit again.
- If you disconnect on duty you stay on duty, with the police gear, when you
  return. The civilian stash waits until you clock out.

## Numbers in one place

| What | Config field (`police` in `config.json`) | Class field | Default and pack |
|---|---|---|---|
| Desk range | `applyDeskRange` | `m_fApplyDeskRange` | 6 m |
| Play time to apply | `applyMinPlaytimeHours` | `m_iApplyMinPlaytimeHours` | 50 h |
| Wait after a rejection | `applyRejectCooldownHours` | `m_iApplyRejectCooldownHours` | 72 h |
| Trial length | `trialHours` | `m_iTrialHours` | 168 h |

The code: `LF_PoliceConfig.c`, `LF_PoliceApplications.c` and
`LF_PoliceApplicationRules.c` under
`addons/LifeFramework/Scripts/Game/Feature/Police/`; the pack values are in
`addons/LifeFramework/Configs/Police/LF_Police.conf`. If the server changes a
number, this page is out of date until it is edited. Check the desk's own
message, which always shows the live wait. The server can be wiped with 24
hours' notice on Discord; see the [wipe policy](wipe-policy.md).
