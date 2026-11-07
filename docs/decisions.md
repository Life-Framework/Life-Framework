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
configurable threshold per crime.

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

## Delivery

**2026-11-07 — Milestone 1 is a signposted DebugWorld playtest town.** Every
loop playable end to end with in-world signs explaining it, before any Everon
placement.

**2026-11-07 — Playtests are hosted from the maintainer's PC.** No rented
hosting yet.

**2026-11-07 — 3D proof and filming are part of the dev loop.** The visual
harness and a filming harness run on any machine with the game; clips are PR
evidence and showcase material.
