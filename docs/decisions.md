# Decisions

Product and design calls that set the taste of Life Framework. Each entry is a
decision someone could reasonably have made the other way. Read the relevant
entries before building in that area; argue with a decision by opening an issue,
not by quietly building the alternative.

**Framework vs flagship.** Where a decision says "framework: configurable", the
system must support the listed range through configuration, and the flagship
default is what 1989 Everon ships with. See `docs/direction.md`.

Format: date — decision. Why. What it means for code.

---

## Identity

**2026-11-07 — A framework proven by a flagship.** Other communities build on
the framework; our 1989 Everon server proves it. Core systems are era-agnostic;
1989 Everon is a content pack. Code: no era strings, prices, or prefabs
hard-coded in systems; they come from config or content.

**2026-11-07 — Pure open source (MIT).** We grow through adoption and
reputation, never monetization.

**2026-11-07 — AI-first contribution.** Conventions assume the contributor is an
agent; humans can read and reason about everything. Agents improve the tooling
(MCP, harness, filming, validators) as they work.

## Gameplay

**2026-11-07 — In-world first.** Systems live in the environment, not behind a
UI. Things that matter are real items: cash, keys, ID and license cards, a fine
ticket, a paycheck, goods on a shelf, a notice on a board. Feedback comes from
the world first (an item changing hands, a sound, an animation, a door that
opens) and a UI toast second. A UI is right only where the in-world version
adds friction without adding play: reading a sign's detail, typing an amount, a
confirmation. When a UI is used, make it feel like an object in the world (a
paper form, a terminal screen, a card), not a game menu. The test for every
design: "how would this exist in the world, and does doing it in the world make
it more fun or just slower?" Every feature PR says how it applied this.

**2026-11-07 — Death drops what you carry.** On death, carried cash and items
stay on the body and can be looted. Bank balance, vehicles, and property are
safe. Why: it makes robbery, the ATM, and police stakes real. Framework:
configurable (drop all / cash only / nothing plus a hospital bill). Flagship:
drop all carried.

**2026-11-07 — Major crimes need police online.** Bank, hospital, and convoy
jobs require a minimum number of on-duty officers; small crimes (till robbery)
stay open. Start with a single threshold per crime, not tiers. AI police that
respond when officers are offline is on the roadmap, after M1. Framework:
configurable threshold per crime. Superseded 2026-11-08 by "Major crimes are
always allowed; no police means a louder alarm".

**2026-11-07 — Progression is pluggable.** The framework supports a range of
progression styles (visible levels, hidden experience, none). Flagship: jobs use
hidden experience (you get better and paid more without an XP bar); professions
(police rank, licensed trades, medic) are exposed as titles and certifications.
No arcade text ("LEVEL UP!", "+5 XP") on the flagship.

**2026-11-07 — Weapons are available but licensed.** Civilians can buy legal
firearms with a license at a gun shop; heavier and military weapons are illegal
and come from the black market, armory robberies, or contraband. Framework:
the legal/illegal split and license requirements are config.

**2026-11-07 — Signs are diegetic, with a "Read" for detail.** Signs are 1989
boards, posters, and notices that belong in the scene. Each can be read for a
short card: what this is, how to use it, what it needs, the rules. The card is
the depth; the board is the world.

**2026-11-07 — Short guided arrival, then free.** A 5-10 minute arrival chain
(ID card, first job, first paycheck, bank it) built on the quest system, then
the world opens. It proves the loop to every new player.

**2026-11-07 — The flagship is a life sandbox.** Life systems are the game; RP
is welcome and emergent, not enforced. A few server rules (no random killing at
spawn) are admin-enforced. Framework: tone is the server owner's call.

**2026-11-07 — NPC prices move with supply and demand.** NPC buyers and shops
adjust prices from recent volume (selling 500 apples lowers the apple price,
which recovers over time). Why: stops grind exploits, creates trade routes.
Framework: curves and recovery are config; a fixed-price mode is a curve of 0.
Scheduled after M1 unless an exploit forces it earlier.

**2026-11-07 — Garage it to keep it.** A vehicle persists across restarts only
when parked in a garage or an owned spot. Anything left on the street at
restart goes to the impound lot, collected for a fee. Why: mostly in-world,
controls clutter and car-camping. Framework: impound fee and owned spots are
config.

**2026-11-08 — No passive paycheck; unemployment benefit at the counter.** The
timer paycheck ($700 every 300 s to every job, unemployed included) is gone.
Civilians are paid cash at the drop for each completed run and by selling to
traders. Police are paid duty pay in cash at the station desk when they go off
duty (on-duty seconds times a rate). A player whose bank balance is below a
threshold, who is not on duty as police and has no active delivery run, collects
a cash benefit in person at the job counter, once per cooldown. Why: removes the
invisible mint while keeping a safety net for broke players. Framework: duty
rate, benefit threshold, amount, and cooldown are config
(`Configs/Jobs/LF_Jobs.conf`). Flagship: $0.50 per duty second; $100 benefit
below a $500 balance, once per 20 minutes. (Maintainer decision.)

**2026-11-08 — Sign titles are printed on the board.** A sign shows its title on
the physical prop, not only in the `Read: <title>` prompt. The maintainer chose
this over the plan default (prompt only, render-target text as a later spike).
Why: a player reads the street from a distance and knows what a place is before
walking up to it. What it means for code: `LF_SignComponent` draws the title
into a paper sheet through an `RTTextureWidget` on `LF_Sign_Board` and
`LF_Sign_Notice` (#119). `LF_Sign_Pub` and `LF_Sign_Chalkboard` ship with the
face off until #116 places their sheets. The Read card stays the depth. A
dedicated server draws nothing.

**2026-11-08 — No faction screen; police clock in at the duty desk.** Everyone
spawns as a civilian. A whitelisted player becomes police by clocking in at the
station duty desk. The whitelist is checked there, and going on duty sets the
police loadout. The maintainer chose this over the plan default (keep the
screen for M1, move in M2). Why: police is a job you go to work at, so it
belongs in the world, and nothing stands between joining and playing. What it
means for code: M1-11 (#96) deletes the faction screen and its code path. The
registration form (M1-08, #93) asks for name and age only. Spawn points and
loadouts no longer depend on a faction pick. Framework: the whitelist source
and the duty loadout are config.

**2026-11-08 — A fine is a paper ticket paid at the police desk.** The officer
writes a ticket face to face and the suspect carries it. It is paid in cash at
the police desk within N minutes. A ticket unpaid at the first deadline doubles
once. Unpaid at the second deadline, the player gets +1 wanted. Fines never
come out of the bank. The maintainer chose this over the plan default (pay on
the spot or refuse and become arrestable). Why: money stays physical, and the
ticket gives the player a reason to walk into the station. What it means for
code: the ticket item records amount, issuing officer, and issue time. The
server owns the deadlines and the wanted change (M1-19, #104). Framework: both
deadlines and the multiplier are config. Flagship: double once, then +1 wanted.

**2026-11-08 — Major crimes are always allowed; no police means a louder
alarm.** Bank, hospital, and convoy jobs run even when no officer is on duty.
With nobody on duty, the alarm at the site is louder and more noticeable in the
world, so other players hear and see it. Supersedes "Major crimes need police
online" (2026-11-07). The maintainer chose this over the plan default (bank and
hospital need 1 or more on duty). Why: a town with few players still has its
heists, and the world answers the crime when no officer can. What it means for
code: the alarm (M1-20, #105) picks its level from the on-duty count when the
crime starts. Framework: a configurable minimum per crime stays. Flagship:
minimum 0 for every crime, with the louder alarm. AI police that respond when
officers are offline stays on the roadmap, after M1.

**2026-11-08 — You wake up in hospital.** After death, the player respawns in a
hospital bed and pays a small hospital bill from the bank. A player who cannot
pay goes into debt: the balance goes negative and later deposits clear it
first. What it means for code: the server picks the bed and charges the bill;
the client never sends an amount (M1-26, #121). The bill shows in the world
first (a paper bill or a card on waking). Framework: the respawn point and the
bill amount are config. Flagship: a hospital bed and a small bill.

**2026-11-08 — Names show up close, after an introduction.** Once someone has
introduced themselves, their name shows only when you look at them from a few
metres away. Names never float over a crowd. Introductions are remembered
across sessions. What it means for code: the player interaction menu (M1-12,
#98) persists who knows whom per account and shows the name on a short-range
look check, not as a name tag. Strangers still show nothing.

**2026-11-08 — Police gear is a second inventory.** Clocking in stores the
player's civilian inventory and restores the police inventory they had when
they last clocked out. The first clock-in gives a starter police kit. Clocking
out swaps back. Why: duty gear cannot be smuggled into civilian life. This
replaces the once-per-shift armory idea for M1. What it means for code: the
duty desk (M1-11, #96) swaps the two inventories on the server, and both
persist across a restart through the carried-inventory save (M1-04, #89).
Framework: the starter police kit is config.

**2026-11-07 — One player interaction menu.** Walking up to another player
offers a short, simple list of person-to-person actions: introduce yourself,
show ID, give cash or an item, and role actions (search, cuff, fine) when the
role allows. Names: strangers show nothing; after someone introduces
themselves, their name shows for you. The ID card is a real item police can
ask for. Keep it simple: one menu, few options, every option one press.

**2026-11-07 — Radios, payphones, and pagers.** Period comms: handheld and
vehicle radios on frequencies (reuse Reforger's radio system), payphones at
fixed spots (anonymous tips, calling a service), and a pager item that receives
short dispatch messages for jobs and police. No phone UI.

**2026-11-07 — Buy and furnish property.** Selected real buildings are for
sale (for-sale sign out front, real keys); owners place furniture and storage
inside, and stored items exist physically in the building. Big system: after
M1, built on the existing house lock and key code.

## Delivery

**2026-11-07 — Milestone 1 is a signposted DebugWorld playtest town.** Every
loop playable end to end with in-world signs explaining it, before any Everon
placement.

**2026-11-08 — Every M1 site moves onto one street now.** DebugWorld gets one
proper town street: the spawn plaza at one end, then the sites in the order of
the arrival chain. The maintainer chose this over the plan default (signpost the
sites where they stand, x 94 to 243, then tidy). Why: a stranger walks one
street and meets the loop in order. What it means for code: M1-25 moves the
`Street_*` layer entities, keeping their names and GUIDs and changing positions
only. Sign placement (M1-09), the police station (M1-11), and the town tidy
(M1-22) build on the new street.

**2026-11-08 — The playtest town is a 1989 Everon village with a test annex.**
The loop lives in a small village built from vanilla Everon buildings, on a
real road, with a square at the spawn. Dev fixtures and test-only content move
to a fenced test annex off to the side. Why: a stranger sees a real place, and
the tests keep their fixtures. What it means for code: M1-25 (#120) builds the
village and the annex. The town tidy (M1-22, #107) moves dev fixtures into the
annex instead of deleting them, keeping names so tests still find them.

**2026-11-07 — Playtests are hosted from the maintainer's PC.** No rented
hosting yet.

**2026-11-07 — 3D proof and filming are part of the dev loop.** The visual
harness and a filming harness run on any machine with the game; clips are PR
evidence and showcase material.
