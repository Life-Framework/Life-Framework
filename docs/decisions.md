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
reputation, never monetization. Amended 2026-11-08: the Workshop build is APL, and the
flagship may take cosmetic donations (entries below).

**2026-11-07 — AI-first contribution.** Conventions assume the contributor is an
agent; humans can read and reason about everything. Agents improve the tooling
(MCP, harness, filming, validators) as they work.

**2026-11-08 — We take leverage from community open source, within its
licence.** Permissive work (MIT, BSD, Apache, CC0, CC-BY, OFL for fonts) may be
copied and modified. We keep its notices and credit it in the README and in
`THIRD_PARTY.md` or `assets-source/LICENSES.md`. Share-alike work (APL-SA,
CC-BY-SA, GPL) and Workshop mods with no licence are never copied into our MIT
code. We use them as a Workshop dependency, take them as inspiration for a
clean reimplementation, or ask the author. Base-game content is referenced and
inherited freely. Why: other people have solved problems we would otherwise
solve again, and the project stays MIT. What it means for code: every copied
file or asset arrives with its notice and a credit line in the same PR.
`docs/ecosystem.md` lists the candidates and their licences.

**2026-11-08 — A very small number of Workshop dependencies is acceptable.** A
dependency must be maintained, widely used, on terms that allow depending on
it, and carry something we cannot reasonably build (for example thebonbon's
animations, under evaluation in `docs/ecosystem.md`). The framework core works
without it where practical; the flagship content pack may depend on it. Each
dependency is its own entry in this file. Why: polish we cannot build alone
(see "Animation and sound are part of done") is worth a small dependency cost.
What it means for code: core systems check for the dependency and degrade
instead of failing, and the addon's dependency list changes only with a
decision entry.

**2026-11-08 — The Workshop build is APL; the GitHub repo stays MIT.** The
build we publish on the Workshop is licensed under the Arma Public License
(APL). The source on GitHub stays MIT. Why: section 3 of Bohemia's Workshop
Terms of Use requires uploaded content to carry an APL-family licence (or a
custom non-commercial one), and MIT is not on that list. What it means for
code: nothing in the source changes. The README's License section says which
licence covers which copy, the Workshop description names APL, and the M1
playtest release (M1-24, #109) publishes under APL. A server owner who wants MIT
terms builds from GitHub.

**2026-11-08 — The framework is always free; the flagship may take cosmetic
donations.** The framework is never monetised. The 1989 Everon flagship may take
donations for cosmetics only, within Bohemia's server monetisation policy. It
never sells money, gear, vehicles, property, queue priority, or a faster
whitelist. Amends "Pure open source (MIT)" (2026-11-07) for the flagship server
only. Why: paid advantages broke trust on Altis Life and GTA RP servers, and
Bohemia forbids them; cosmetics can pay for hosting at M4 without touching
play. What it means for code: no donor flag ever reaches a gameplay system.
Cosmetic unlocks, if built, are flagship content, and nothing before M4 needs
them.

**2026-11-08 — The project takes donations to pay its running costs.** Anyone
may give money to the project. It pays the Claude subscription the AI-first
development runs on, the website and domain, and later a game server, and the
costs are listed in public on the site's support page
(`website/content/costs.json`). A donation buys nothing in the framework or in
any game: no money, gear, vehicles, property, queue priority, or role. The mod,
its Workshop page, and its UI carry no donation prompt. Why: the bills are real
and small, and paying them in the open keeps the project honest; Bohemia's
Workshop terms forbid monetising the mod itself, so the money is tied to the
project's costs, not to the content. What it means for code: nothing in the
mod reads donor status. The support page, `.github/FUNDING.yml`, and the
README section stay off until a way to give exists
(`docs/community/support.md`).

**2026-11-08 — Overthrow code may be used and relicensed under MIT, with
credit.** The owner of Overthrow (`ArmaOverthrow/Overthrow.Arma4`) gave us
explicit permission to use its code and relicense it under our MIT licence
(APL on the Workshop build, like the rest of ours). The maintainer holds the
record of that permission. This overrides the repo's APL-SA licence for Life
Framework only; anyone else still takes Overthrow under APL-SA. Why: Overthrow
is a mature Reforger mod with persistence, economy, AI, and UI code that solves
problems we would otherwise solve again. What it means for code: a copied or
derived file keeps the header
`// Derived from Overthrow (https://github.com/ArmaOverthrow/Overthrow.Arma4), used with the author's permission, relicensed MIT.`
and its path is listed under Overthrow in `THIRD_PARTY.md`. The four files
already ported from Overthrow (money format and delta, resource defs, shop
rules) carry that header. New classes are `LF_` (AGENTS.md rule 5). `docs/ecosystem.md` lists the systems worth
pulling.

**2026-11-08 — A permissive licence on GitHub governs the code in that repo.**
When a project's copyright holder publishes its code on GitHub under MIT, BSD,
Apache-2.0, CC0, or CC-BY, that licence governs our use of the code in that
repo, even if the same mod's Workshop page carries an APL-family licence. An
author may license the same work differently in different places, and Bohemia
requires an APL-family licence on every Workshop upload anyway. Two conditions:
the repo is published by the copyright holder (an author or official account,
not a third-party re-upload), and the grant covers only what is in the repo; an
asset that exists only in the Workshop build stays under the Workshop licence.
Why: the Workshop licence is often a requirement of the upload, not the
author's choice for the source. What it means for code: read the `LICENSE`
file on the commit we copy from (a badge or README line does not count), then
follow the copy rules in `docs/ecosystem.md` section 5.

**2026-11-09 — Two Workshop items, two brands.** The core is
"Life Framework (Core)", addon ID `LifeFramework`: clean, modern, neutral, aimed
at server owners and pack authors. The flagship pack is
"Life Framework: Everon 1989", addon ID `LF_Everon1989`: period-set, with its
own narrative, art direction, and Workshop page, aimed at players. The pack
depends on the core, so a player who subscribes to the pack gets both. The core
page opens with a pointer to the pack for players; the pack page names the
core. Versions are semver per item; the pack states the core range it needs; a
schema or save-format bump is a major version. Why: the core must stay usable
for a modern or fictional pack, and the flagship needs a personality that a
neutral engine page cannot carry. What it means for code: no period branding,
copy, or art in the core (the existing core/pack red line); pack-only strings
and art live in the pack.

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
`LF_Sign_Notice` (#119). `LF_Sign_Pub` and `LF_Sign_Chalkboard` print it too
(#116). The Read card stays the depth. A
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

**2026-11-08 — Wanted is automatic; police get a description, not a name.**
Committing a crime makes you wanted at once, with no officer needed. The police
learn only a vague description: clothing, vehicle, and place. They learn the
name when they identify the suspect, by an ID check or by catching them in the
act. Why: police work becomes finding and identifying someone, and a criminal
who changes clothes or cars has a real way out. What it means for code: crimes
already raise wanted on the server (`EL_RobAction`, `LF_Breach`, `LF_Hospital`).
Each crime report also records a description snapshot, and the account keeps
an "identified" flag per wanted entry that an ID check or an arrest in the act
sets. The wanted board (M1-11, #96) and dispatch (M1-20, #105) show the
description until the suspect is identified.

**2026-11-08 — Killing anyone makes you wanted for murder; the spawn is a safe
zone.** A kill of a player or a civilian is murder, reported with the same vague
description as other crimes. The spawn plaza is a safe zone where players
cannot hurt each other. Admins handle repeat offenders; there is no automatic
punishment. Why: killing has a cost in the world, and a new arrival is never
shot before they can play. What it means for code: M1-27 (#125) adds the kill
hook and the zone. Framework: the murder wanted amount, the on-duty officer
exemption, and the zone radius are config.

**2026-11-08 — Survival needs are light.** Hunger and thirst drain slowly. At
worst they cap health at about 75% and reduce stamina. They never kill. Why:
eating is a reason to visit the shop, not a chore that ends a session. What it
means for code: today `EL_SurvivalStats` drains health to zero below 20 hunger
or thirst, and the rates are hard-coded. M1-28 (#126) moves them to config and
adds the cap. Framework: drain rates, the health cap, and the stamina penalty
are config. Flagship: slow drain, about a 75% cap.

**2026-11-08 — Jail time is online time, and work shortens it.** A sentence
counts down only while the prisoner is connected. In-jail activities (cleaning,
labour) take time off. Why: logging off does not serve a sentence, and a
prisoner has something to do. What it means for code: `LF_Jail.Tick` advances
every serving sentence today without checking the connection; M1-21 (#106)
pauses offline prisoners. In-jail activities are after M1 (#127): each needs
its own interaction with animation and sound, and M1's loop is proven by a
prisoner who serves visible online time and walks out. Framework: activities,
seconds per task, and cooldown are config.

**2026-11-08 — Money leaves through fuel, repairs, food, medicine, property,
and the law.** The economy direction: money drains through fuel and vehicle
repairs; food, medical supplies, and hospital bills; property upkeep, rent, and
taxes; licences, fines, and impound fees. Why: income from jobs and crime needs
places to go, or prices lose meaning. What it means for code: every sink is a
config price, never a literal in a system. In M1: food and medicine at the
general store (`EL_ShopComponent`, exists), the hospital bill (M1-26, #121),
paper fines (M1-19, #104), and impound fees (M1-16, #101). After M1: paying for
fuel at the pump, vehicle repairs, property upkeep, rent, and taxes (with
property), and licences as paid cards (with the gun shop). Licences cost skill
points today, not money.

**2026-11-08 — Apply to the police at the station desk.** A player applies with
a paper application form at the police station desk. Admins or police chiefs
approve it, in game at the desk or by editing the whitelist file. Why: joining
the police is part of the world, and admins keep control. What it means for
code: approval writes through `EL_WhitelistManager`, so the file stays the
source of truth. Framework: approvers, open or closed applications, and the
rejection cooldown are config. After M1 (#128); M1 playtests use the file.

**2026-11-08 — Accelerated day and night by default.** The flagship runs a
faster day and night cycle. Server owners can switch to real time instead.
Why: players who join in the evening still see daylight, and night play comes
round often. What it means for code: prefer vanilla `TimeAndWeatherManager`
settings over a script loop. Framework: day length, night speed, and the
real-time mode are config. After M1 (#129).

**2026-11-08 — One marked lawless area; the town stays civil.** The flagship
has one marked remote place, such as the quarry or the islands, where the
server rules about starting a fight do not apply. Gangs and contraband are
drawn there; the town keeps its rules. Why: Altis servers that kept a war zone
moved PvP away from town and gave gangs a home. One clearly marked area keeps
that without turning Everon into Wasteland. What it means for code: zones are a
framework feature. A zone has a shape, a rule set (lawless, safe like the spawn
plaza, or normal), and in-world markings (signs, a gate, a fence line). Systems
ask the zone service, never a hard-coded position. The incident log records the
zone with every event, so a moderator sees at once whether a fight broke a
rule. Framework: zones and their rule sets are config. Flagship: one lawless
area, the spawn plaza safe, the rest normal.

**2026-11-08 — Hostile intent is a light mechanic.** A "hold up" action, or a
raised weapon aimed at someone, marks intent toward that player. The server
records it in the incident log. There is no forced dialogue, no countdown, and
no menu. Why: every lasting life community wrote initiation rules ("talk
first"). Recording the hold-up makes those rules cheap to enforce without
adding friction to play. What it means for code: the hold-up is an entry in the
player interaction menu (M1-12, #98) with an animation and a sound. The server
detects a raised and aimed weapon and logs it with throttling. Intent marks are
facts for moderators; on their own they change no damage rules. Framework: the
aim window, the range, and whether aim alone counts are config.

**2026-11-08 — Top-tier crime needs tools from a fence.** The armory, the cash
van, and the bank vault need tools (a drill, charges, a keycard) sold only by a
black-market fence. The fence deals with you after you have done smaller jobs.
There is no reputation bar and no menu: the fence turns you away until he has
seen your work. Why: the top tier stays rare and earned, and each tier feeds the
next, as the Altis Federal Reserve and GTA RP heist tools did. "Major crimes are
always allowed" still holds once you have the tools. What it means for code: a
hidden per-account crime record (count and kind of completed jobs) that the
fence NPC checks on the server. The tools are real items with their own
animation and sound in use. Framework: the fence's thresholds, tool list, and
prices are config.

**2026-11-08 — Death costs a hospital bill and the New Life Rule.** You wake in
hospital with the bill (decision "You wake up in hospital"). You also forget the
fight that killed you and must not return to that scene for about 15 minutes.
This is a server rule with a soft in-world cue: a hospital wristband or a note
on waking, and a quiet reminder if you walk back toward the scene. Why: death
needs a cost beyond money, or a fight restarts the moment the loser respawns.
Altis's New Life Rule is the tested answer. What it means for code: the server
stores the death position and time per player, and the incident log records a
return inside the radius and window. Nothing blocks the player physically.
Framework: the radius, the window, and the cue are config. Permanent character
death is not planned.

**2026-11-08 — Officers apply fixed penalties from config.** For M1 and M2 a
fine or sentence for a crime is the amount the server's penalty table sets. The
officer writes the ticket or the jail slip; the amount is not theirs to choose.
A player-staffed bail and appeal desk is a later option, not planned. Why:
fixed penalties are fair between officers and easy to explain on a sign; a
court needs players we do not have yet. What it means for code: the paper
ticket (M1-19, #104) and jailing (M1-21, #106) read the amount from a config
table keyed by crime. Framework: the table is config.

**2026-11-08 — Every on-duty officer can arrest and escort; rank gates the
extras.** Arrest and escort are the core police loop, and a new officer can do
both from the first minute on duty. Rank gates the fine cap, the armory tier,
and later supervisory actions (promotion, and what M2 adds), not the loop.
Why: on a fresh server nobody is senior, and senior took an hour on duty, so
the first hour of police play had no arrests (#165). What it means for code:
`LF_PoliceConfig` (`Configs/Police/LF_Police.conf`) holds the lowest rank that
may arrest and the lowest that may escort, and the patrol fine cap. Every
arrest and escort path re-checks it on the server (`LF_Jail.Arrest`,
`LF_JailEscort`); the offers read the same rule from the replicated rank.
Framework: per-rank permissions are config. Flagship: arrest and escort at
rank 0 (patrol), patrol fine cap 1000.

**2026-11-08 — Gamepad-friendly, PC-first.** Every interaction works on a
gamepad: amounts come from presets, lists stay short, and typing is rare.
Crossplay is on. Design and testing happen on PC first. Why: Workshop mods
reach Xbox and PS5 players, and a menu that needs a keyboard shuts them out.
What it means for code: every action binds keyboard plus `gamepad0:`
(AGENTS.md input rules). A UI that takes a number offers presets first and free
entry second. A feature PR names how a gamepad player completes it. Console
playtests come after the PC loop is proven.

**2026-11-08 — A new character can buy the cheapest car.** A newly registered
character starts with $1,000 in the bank, the price of the S105 on the lot, on
top of the $500 cash in the starter kit. Why: the maintainer's first playtest
stalled at the car lot with $650 in the bank; a first car should be a choice on
day one, not a grind. Framework: config (`EL_SpawnLogic.m_iStartingBank`, paid
once at registration). Flagship: the cheapest car on the lot.

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

**2026-11-08 — Private playtests first, then a public Workshop release with a
showcase.** The first public Workshop release comes after two or three private
M1 playtests with friends and the fixes they turn up. It ships with a showcase
video. Why: a stranger's first impression should be of a town that friends have
already broken and we have fixed. What it means for code: M1-24 (#109) runs the
private rounds first (an unlisted or private Workshop item, or a direct
install), then the public publish and the video.

**2026-11-08 — Animation and sound are part of done.** Where the player does
something physical, the interaction has a fitting animation and sound. A
flagship feature with a placeholder or missing animation or sound effect is not
done. Why: world feedback comes first (decision "In-world first"), and in a
life game the feel of an action is the content. What it means for code: every
feature PR names the animation and sound each physical interaction uses
(vanilla, a recorded dependency, or our own) and shows them in its clip. A
framework system exposes the animation and sound as config so a server can
swap them.

**2026-11-07 — 3D proof and filming are part of the dev loop.** The visual
harness and a filming harness run on any machine with the game; clips are PR
evidence and showcase material.

**2026-11-08 — Ore is mined by swinging a pickaxe at the rock.** The "F to
gather" action on ore rocks is gone. A pickaxe hit takes the same melee damage
path as woodcutting; every few hits a nugget breaks off and drops beside the
rock, and a spent rock crumbles and grows back on its ResourceChains.conf
cooldown. The maintainer has thebonbon's permission to take the pickaxe-hit
mining from `thebonbon/EveronLife` branch `enhancement/Main_World` (MIT); the
port keeps a `// Ported from` header and a THIRD_PARTY.md line. Why: playtest
feedback asked for physical mining instead of a prompt (decision "In-world
first"). What it means for code: `LF_OreNodeComponent` owns yield, depletion
and grow-back on the server; the hit zone only gates the tool. The spent state
is not saved, so a restart grows every rock back.

**2026-11-08 — Scheduled events are parked.** Organised events (a rally, a gang
war night, a strike at the factory) are interesting for a later roadmap phase
and are not planned for M2. Why: the sandbox has to prove its emergent loops
before a schedule sits on top of them. What it means for code: nothing now.
Game Master is the likely base when events come back.

**2026-11-08 — Milestone 2 leads with four themes.** M2 builds moderation (the
incident log and Game Master tools), medic (downed state, EMS, ambulance),
comms (payphones, pagers, a dispatch board), and investigation (witnesses and
evidence, which make the vague-description wanted system real). The maintainer
picked all four from the benchmark's top ten
(`docs/research/life-rp-benchmark.md`). The lawless zone, the hold-up, the
fence, and the New Life Rule ship as scopes under these themes. Why: moderation
decides whether a server lives, and the other three give police, medics, and
criminals something to do when the other side is absent. What it means for
code: `docs/roadmap/m2-outline.md` holds the outline; M2 is planned in full
after the M1 playtests. Placing the town in Everon from reusable site prefabs
stays in M2 as its closing step (`docs/direction.md`).

**2026-11-08 — The framework ships as two addons in one repo.** `LifeFramework`
is the era-agnostic core: every system, the persistence layer, the test suite,
and neutral defaults. `LF_Everon1989` is the flagship pack: the 1989 village and
town, period prefabs, flagship values, and period copy. Both live in this
monorepo and ship as two Workshop items. The pack depends on the core; the core
never references the pack. Why: an owner must be able to swap the flagship for
their own pack without forking the systems. What it means for code: M3 moves
pack resources with their `.meta` (the GUIDs stay the same), and a validator
guards the one-way boundary (`docs/roadmap/m3-outline.md`).

**2026-11-08 — The core decides what the game can do; the pack decides what
this server is.** The core (`addons/LifeFramework/`) gets script, components,
anything a server in another setting would use unchanged, anything a test
needs, base prefabs a system spawns with a neutral look, and one neutral
working default per system. The pack (`addons/LF_Everon1989/`) gets the
choices: placement and the village, names and flavour copy, prices and timers
as `.conf` values, the 1989 look, the vehicle catalogue, and the flagship
mission header. A resource belongs in the core when the pack can be deleted and
the core still boots and passes the suite, or when a modern-setting server
would use it as is. Script in the pack is a smell; the fix is an extension
point in the core. The maintainer set the line and its grey cases. Why: without
one written rule every agent classifies by feel, and the boundary leaks before
the move happens. What it means for code: every new resource is classified
with `docs/architecture/core-vs-pack.md`; `validate-addon-boundary` fails when
the core references the pack or the pack holds script.

**2026-11-08 — Owners configure through one JSON file over shipped defaults.**
Shipped `.conf` files are the defaults (core, then pack). The owner writes
`$profile:LifeFramework/config.json` with sections for jobs, shops, crime,
licences, vehicles, zones, and persistence. The owner file wins. An invalid
field logs, falls back to its default, and the server stays up. Unknown keys
are kept. Clients get what they need through a versioned stream. Why: Workshop
paks are read-only, and Overthrow's knobs-only JSON forces owners to fork for
real changes. What it means for code: every `LF_*Config.Get()` reads effective
values through one config service.

**2026-11-08 — One JSON Schema is the source for config.** A checked-in
`config.schema.json`, generated from the script config classes, feeds the game's
validation, `tools\cli config-check`, the web generator on life-framework.org,
and the owner docs. CI fails when it drifts. Why: four copies of the field list
would disagree within a release.

**2026-11-08 — The flagship opens after M3, from the maintainer's PC first.**
M4 starts once M3 has shipped, on the released core and pack. Sessions are ad
hoc, sized for about 40 players, and hosted from the maintainer's PC. The
server moves to a rented box for 24/7 play once sessions fill several times in
a row and donations cover three months of the box. M4 has no closing bar. Why:
the flagship proves the same path owners take, and rent is paid only when
demand and funding exist.

**2026-11-08 — Unplaced scopes go to the milestone they fit.** The Alpha,
Release, and After-release phases are retired; milestones are the one plan. Jailbreak,
gas-station robbery, the police application (#128), and jail work (#127) go to
M2. Day and night (#129) and police missions go to M4. Bus stops (#203), trash
cans (#205), housing, crafting, and tasks go to M5 with the flagship story
(#214). Revive folds into the M2 medic theme. Scheduled events stay parked. M3
carries no new gameplay.

**2026-11-08 — Features talk through a server event bus.** Core features
publish typed events (`OnMoneyMoved`, `OnCrimeCommitted`, `OnPlayerDowned`,
...) on `LF_EventBus`, and other features and packs subscribe instead of
calling each other. A failing subscriber logs and the rest still run. The
event list is part of the supported API. Why: M2's crime, dispatch, incident
log and investigation all react to the same moments; direct calls would tangle
them and leave packs nothing to hook. Scope #329, first in M2.

**2026-11-08 — Money is a ledger, not a balance.** Every cash and bank change
goes through `LF_Ledger` as a recorded transaction (who, why, amount, source,
balance after). Balances can be rebuilt from the log, and a validate rule bans
direct balance writes. Why: moderation must trace and reverse money, and a dupe
on a live server is otherwise unrecoverable. Scope #330.

**2026-11-08 — Characters per account come from config.** The framework keys
every record by a stable character id and allows `characters.maxPerAccount`
characters. The flagship ships one per account with an in-world "start a new
life" form. Bans and whitelists attach to the account; reputation and records
attach to the character. Why: the New Life Rule, whitelists and save keys all
depend on it, and changing it after live data is a migration. Scope #331.

**2026-11-08 — Sites are prefabs.** Shops, jobs, desks and other sites exist
as placed prefabs in a world. There is no config-driven spawn at coordinates.
An owner on another map places the core's site prefabs in their own world and
tunes them through config. Why: placement is world authoring, Workbench does it
well, and a second placement path would double the surface to test.

**2026-11-08 — Every `EL_` class becomes `LF_`.** One scripted rename in a short
freeze right after M1 sign-off, before M2 features fan out (#334). Any type name
written into a save gets a load-time alias. Afterwards a validator rejects new
`EL_` declarations. Why: `EL_` collides with the real EveronLife mod, mixed
prefixes confuse contributors, and the M3 public API must start with final names.

**2026-11-09 — The EL_ to LF_ rename resets saves (pre-release).** M1 is not on the Workshop,
so no player save exists. The engine matches a component key (`LF_PersistenceComponent:<id>`)
and a record `$type` literally and has no alias API, so a save written before the rename
loads none of our state. A boot that continues such a save logs one warning and starts fresh
(docs/persistence.md "Save reset"). This supersedes the load-time alias plan above; the
compatibility rule below starts at the first published save.

**2026-11-08 — Saves stay compatible within a major version.** Any 1.x save
loads in every later 1.x. A test loads saves recorded from older versions, and
`wt e2e` proves each persistence change. Before M4 runs 24/7, writes are atomic
with rolling backups, so a crash mid-save loses nothing. Why: owners and the
flagship keep player progress across updates; a broken save is a lost community.

**2026-11-08 — Build for 128 players.** Systems are designed and benchmarked for
128 players on a dedicated server: replication, server frame cost, save size and
time, UI fan-out. Budget and gate in #335. Why: the target sets every
replication and persistence choice, and raising it later means rework.

**2026-11-08 — Both addons are MIT; only the pack may change later.** The source
of the core and the flagship pack on GitHub is MIT. The project keeps the option to license
future versions of the pack differently; the core stays MIT for good.
CONTRIBUTING.md asks pack contributors to agree to that relicensing, since
without it the option would need every contributor's consent. Amended
2026-11-12: this covers the GitHub source only. The Workshop build is APL (entry
"The Workshop build is APL; the GitHub repo stays MIT", 2026-11-08), so
"published releases stay MIT" no longer applies to the Workshop. Assets pulled in under CC-BY or similar keep their own terms.

**2026-11-09 — The core's DebugWorld holds neutral fixtures only.** When the
pack move happens (M3 workstream 1), village and period content go to
`LF_Everon1989`. The core keeps generic test props, plain doors and boxes.
Flagship tests run on a pack test world, so both addons are exercised on every
build. Why: the core never holds period content (core-vs-pack red line), and
the core's Workshop page is clean and modern. What it means for code:
`validate-addon-boundary` stays strict; no "temporary exception" rows for the
village in `core-pack-inventory.md`.

**2026-11-09 — Packs may add their own config sections.** A pack declares a
schema fragment namespaced under its addon id (`packs.<AddonId>.<section>`),
merged into the one schema with per-field fallback like a core section. A pack
section never overrides a core section. Why: new systems in a pack (the M5
illicit chains, a modern pack's phone shop) need settings the seven core
sections do not cover, and the generator and `config-check` must keep one
source of truth. Issue #553.

**2026-11-09 — Compatibility with other mods is "do not collide", nothing
more.** The `LF_` prefix and the `validate-no-el-prefix` rule are the whole
compatibility story. No bridge, importer or shared-state API for Overthrow or
similar mods unless a real need appears. Why: the benefit is unproven and the
work is large (maintainer, 2026-11-09).

**2026-11-09 — Development pipelines are scheduled on demand, video first.**
The Development track (assets, animation, weapons, vehicles, voice, video,
release gates) does not run on a fixed capacity share. A pipeline starts when a
milestone issue depends on it. The video pipeline (#390) goes first because it
engages the community during development and feeds the M4 trailer (#317).
Development work runs with `--priority low` so it never delays milestone issues.

**2026-11-09 — The flagship opens by invitation, then to everyone.** M4 starts
with an invite-only alpha drawn from the Discord and playtesters. The server
opens to the public once several sessions reach the player target with no save
loss and no blocking regression, and trial moderators are in place. Why: the
first impression is permanent and staff are few. Issue #555.

**2026-11-09 — The flagship wipes after each milestone during M4, then keeps
progress from a declared date.** While milestone content (M5, M6) is still
landing, a wipe follows each one, announced a session ahead and preceded by a
backup. From a declared persistent launch date the save-compatibility rule for a
major version applies and a wipe needs a save-format break. Why: early saves are
test data, and a wipe is the honest way to start fresh around new systems. Issue
#556.

**2026-11-09 — First moderators are trial moderators from the playtesters.**
Trusted playtesters get the moderator guide and the logged Game Master tools on
probation, reviewed after a few sessions (#313). Why: the outline names staff
burnout as the main risk, so recruit before the first public session.

**2026-11-09 — PC-hosted sessions run ad hoc, announced on Discord.** No fixed
weekly slot during the PC phase. Open: the notice period before a session; the
24/7 trigger (sessions reaching the player target several times and donations
covering a rented box) is unchanged.

**2026-11-10 — M2.5 Showcase and Outreach runs right after M2.** A milestone for
the hero demo video, the landing page, our Discord onboarding, an outreach kit
and a trusted-tester roster. The video pipeline (#390) moves into it. The
maintainer posts in other communities as a human where they have permission;
agents prepare the content, run our own Discord (AI-labelled bot) and
coordinate playtesters, and never post in other servers or use a user account.
Why: our Discord is small and the release gates need testers. Epic #566.

**2026-11-10 — Feature verification moves from one person to a quorum.** A
feature is `verified` when its automated proofs pass and enough trusted testers
confirm it in a session; an agent then runs `promote`. The quorum size and the
tester list are set in #575, with the maintainer's approval. Why: a single
human signing off every feature is the bottleneck.

**2026-11-10 — M5 is two tracks, and 3D development lives in M5.** Every
roadmap item from M5 on is a Framework issue (the system, in the core, epic
#632) plus a Flagship issue (the content, in `LF_Everon1989`, epic #633). All 3D
asset work moves from Development to M5 (epic #339): Meshy generates, Blender
(MCP or CLI) refines, Reforger imports, and agents verify visually and
otherwise, wrapped as one nearly autonomous harness. Gated on Meshy's licence
terms and Bohemia's policy (#664). The 15 fitting ideas from the Genetic Life
review also land in M5. Why: the maintainer wants systems and content built
and planned separately, and wants custom art without hand modelling.

**2026-11-10 — Adopt Altis Life's structural patterns before M3.** Crime codes,
one base price with multipliers, a no-profit-loop check, per-field persistence
switches, a world cleanup service, disconnect rules, playtime tracking, and a
server-side request audit go into M3 (epic #684). Altis Life is CC BY-NC-ND, so
we take ideas only. Why: these shape saved data and the owner schema, which get
expensive to change once owners build on M3 and the flagship stops wiping.

**2026-11-12 — M3 maintainer decisions D1 to D12.** Jasper answered the open
questions in section 6 of `docs/roadmap/m3-plan.md`. Each answer is below.

- **D1 (a): M3 waves 0 and 1 start now, at low priority** so M2 keeps the pool.
- **D2 (a): the Workshop build is APL and the source is MIT.** The conflicting
  "Both addons are MIT" entry (2026-11-08) is amended to cover the GitHub source
  only. Unblocks #367 and the licence check in #368.
- **D3 (a): the pack Workshop item is published in the same release as move
  batch 1.** The host doc and `playtest-server.json` list both addon IDs, and
  Discord is told one session ahead.
- **D4 (b): the mission header lists config overlays.** `LF_MissionHeader` gets
  an array of `.conf` resources and the config service merges them field by
  field. No core path names a pack.
- **D5 (a): #746 is trimmed to what is expensive after release.** Kept in M3:
  #685a, #688, #749, #750a, #751, #752a, #754a, #755, #761. Moved to the M5
  milestone: #748, #753, #757, #758, #759, #760, #750b, #752b, #754b. #756 moves
  to "M6 Homes and Land".
- **D6 (a): one PR per save reshape, each with its own version bump,** and one
  announced reset at a release boundary. #761 becomes the close-out.
- **D7 (a): pre-move saves must load after every move batch.**
- **D8 (a): nine core config sections:** jobs, shops, crime, police,
  organizations, licences, vehicles, zones, persistence.
- **D9 (a): the crime catalog lives in config.** The saved
  `{uid, offence, count}` is unchanged. Licence seizure moves to M5 (#685b).
- **D10: as proposed.** #583 moves to M5, #716 to "M2 Living Town", #603 to
  after the move (M4 Flagship Live). #481 stays in M3 and gets the issue
  template and a link to the credits-source issue.
- **D11: not signed off.** The move freeze (#322) waits for all three open M1
  issues: #101, #109 and #199.
- **D12 (a): per-job XP replaces the single player level,** and licence gates
  read job levels.
- **Profit loops:** the gold furnace (+30) and cement plant (+24) stay
  baselined for now, the maintainer's choice.

**2026-11-12 — #749: a pre-job-levels save keeps its progress on CIVILIAN and
DELIVERY.** D12 replaced the single player level with per-job XP. A version 7-27
save has no per-job XP, so the load maps what it has: the single level's total
XP onto CIVILIAN (quests, fruit picking and debug grants all fed that one number,
so it cannot be split by job) and the delivery career's onto DELIVERY. The
shipped CIVILIAN and DELIVERY thresholds are the old curves, so nobody loses a
level; every other job starts at 0. Skill points are dropped: nothing spends
them any more. The issue's first draft started every job at 0; that would have
reset every courier's fuel and taxi access, so it was not taken. Detail:
`docs/persistence.md` item 20.

**2026-11-13 — #758: the dispatch boards keep the Read card; only the wanted board
draws on its face.** The visual-boards spike (issue 6) left open whether the
police and medic dispatch boards get the same face. Their content is service
calls that only on-duty staff of that service may read. A face is drawn by every
client that has the board streamed in, so the server would have to send the calls
to every client, and a filter on the client does not stop a modified client from
reading them. Sending a second list only to on-duty staff would draw a face that
other players see blank or with a call count, which still tells them how busy the
police are. Not worth it: the dispatch boards keep their painted DISPATCH face and
the per-read card. The wanted board's face carries only what its public Read card
already showed. Detail: `docs/architecture/visual-boards.md`.

**2026-11-13 — #664: Meshy-generated 3D assets may be used in the project and in
published builds.** The maintainer signed off the licence gate on 2026-11-13.
Models made with Meshy text-to-3D on the account's plan may go into the repository
and into the Workshop build under that plan's terms. Each one is credited
"generated with Meshy" in `docs/credits.md`, and its generation ids (preview and
refine task) are recorded per asset in `tools/assets/<slug>/meshy.json`, which the
`generate` stage writes. An asset made some other way (a CC0 model, a Blender
script, a human modeller) does not need the credit but keeps its source in the spec.

**2026-11-13 — #664: ElevenLabs-generated sound effects may be used in the project
and in published builds.** The asset pipeline's sound stage (`tools\cli asset sound`,
`docs/asset-pipeline/sounds.md`) generates an asset's sound effects with the
ElevenLabs sound-effects API. The maintainer moved the account to the Starter plan
on 2026-11-13, before the first generation that ships; the taser's 19 takes were all
made on it (the API reported tier `starter` at each run). What the terms say, read
on 2026-11-13 (Terms of Service and the ElevenCreative Terms with Exhibit 4, Sound
Effects, both last updated 2026-10-09):

- A paid plan may use the service for commercial purposes; the free plan is
  non-commercial only (Terms of Service 1(c)). **Nothing generated on the free plan
  ships.** If the plan lapses, generate nothing for the mod until it is paid again.
- Output may be used, reproduced, modified and distributed "including by
  incorporating it into films, trailers, advertising, video, games, applications,
  and other productions" (Exhibit 4, section 2), and the user keeps the rights in
  the output (Terms of Service 4(c)).
- Output may be used only "combined with other material" and may not be made
  "available as isolated audio files or samples". The samples ship inside the addon
  as part of the game; they are never published as a sound pack or a download.
- No attribution duty was found in the terms that were read. The project credits
  the tool anyway: "generated with ElevenLabs" in `docs/credits.md`.
- The service "may produce the same or similar Output" for someone else; a generated
  sound is not exclusive.

Not confirmed: the help-centre articles on publishing and attribution could not be
read (HTTP 403 to an automated client), and the EU terms were not read. Each
generation's prompt, parameters and date, and the takes that ship with the reason,
are recorded per asset in `tools/assets/<slug>/sounds.json`, which the stage writes.

**2026-11-13 — NPC voices are generated with ElevenLabs at build time, and may ship.**
The maintainer moved the ElevenLabs account to the Starter plan on 2026-11-13. Paid plans
allow commercial use and require no attribution, and the output is ours (terms of use read
the same day; `docs/architecture/npc-dialogue.md` "Licence"). Spoken lines made on that plan
may go into the repository and published builds. Each voiced NPC is credited "voices generated
with ElevenLabs" in `docs/credits.md`, and every line's request id, voice, model and text is
recorded in `tools/assets/dialogue/<npc>/elevenlabs.json`, which the dialogue stage writes.
Speech is never generated at run time: the game holds no key and calls no service. A
voice-library voice can be withdrawn by its owner; lines already made stay usable, so a
withdrawn voice means revoicing that NPC when its text next changes. Open: whether the
licence for existing audio survives a downgrade to the free plan; keep the plan or ask first.

**2026-11-13 — An NPC's line is heard by players nearby, the scene is the talker's alone.**
The camera, captions and replies of a conversation exist only on the talking player's
client. The voice and the moving lips are also played for players within the NPC's
`hear_range_m` (12 m by default; 0 makes an NPC private). Why: it is a town, and the world
does not stop for one player's conversation. A default from the spike, for the maintainer
to confirm per NPC.
