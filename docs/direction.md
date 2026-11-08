# Direction

The north star for Life Framework. Read this before choosing what to build.
`docs/roadmap/` holds the scoped plan; `docs/features.md` holds what exists;
`docs/decisions.md` holds the design calls that set the taste.

## What we are building

**A life/roleplay framework for Arma Reforger that other communities build on,
proven by a flagship server we run ourselves.**

- **The framework** is the product: modular systems (economy, jobs, crime,
  police, ownership, persistence) that a server owner can host, configure, and
  extend without forking.
- **The flagship** is the proof: 1989 Everon, played by real people. It is the
  showcase that makes server owners want the framework and players want the
  servers.
- **Core is era-agnostic.** 1989 Everon is our content pack, not an assumption
  baked into systems. A modern or alternate setting must be buildable on the
  same core.
- **Open source, MIT.** We grow by adoption and reputation. The source on
  GitHub is MIT; the Workshop build is APL, as Bohemia's Workshop terms require.
  The framework is never monetized. The flagship may take cosmetic-only
  donations within Bohemia's policy, never gameplay advantages; other servers
  follow Bohemia's rules on their own.

The ambition is the most played and most built-upon mod in Arma's history. We
get there by being the most *playable*, the most *provable*, and the easiest to
contribute to — in that order.

## What "playable" means

A build is playable when a stranger can join, understand what to do without
asking anyone, and complete a loop:

1. Arrive: register at the town clerk, get an ID card, read the town board. Everyone starts civilian; whitelisted players clock in as police at the station.
2. Earn: take a job, gather or process, get paid in cash.
3. Spend and save: shop, bank at the ATM, buy a vehicle.
4. Crime: rob a till, a bank, a hospital; carry contraband.
5. Consequence: police respond, arrest, fine, jail.
6. Ownership: an organization, a hideout, a garage.
7. Restart the server: everything a player earned is still there.

Every step works on a **dedicated server with several clients**, gives
**visible feedback** for success and refusal, and is **verified in the 3D world**
— not only by a test that calls the function.

## Milestones

Each milestone ends with a public build and a filmed showcase. M0 is completed
history. GitHub's M1+ milestones, descriptions, and issue assignments are the
live roadmap data that the website and Discord board render.

| # | Milestone | Done when |
|---|---|---|
| M0 | **Foundation** | `release/next` merged; server-authority, restart, and dedicated-client fixes landed; CI + structural lint on every PR; one honest AGENTS.md. |
| M1 | **Playtest Town** | The DebugWorld street is a signposted test town: a sign at spawn explains the loop, every site has a sign saying what it is and how to use it, every loop above works on a dedicated server, and the visual harness proves each site from the player's view. Published on the Workshop; sessions hosted from the maintainer's PC; feedback collected through GitHub issues. |
| M2 | **Living town** | Four themes work on a dedicated server in the playtest village: moderation (an incident log and Game Master tools), medic (downed state, EMS, ambulance), comms (payphones, pagers, a dispatch board), and investigation (witnesses and evidence behind the vague-description wanted system). M2 closes by placing the loops in one Everon town from reusable site prefabs, on MainWorld, with a filmed tour. Outline: [m2-outline](roadmap/m2-outline.md). |
| M3 | **Framework release** | A server owner installs the `LifeFramework` core and the `LF_Everon1989` pack from the Workshop, sets every price, job, crime, licence, vehicle, zone and save rule in one `config.json` (by hand or with the web generator) without forking, and hosts on their own box; the config harness proves it. Outline: [m3-outline](roadmap/m3-outline.md). |
| M4 | **Flagship live** | Opens once M3 has shipped: a recurring public 1989 Everon server for about 40 players, first in ad hoc sessions from the maintainer's PC, then 24/7 on a rented box once sessions fill and donations cover it. No closing bar; it keeps iterating. Outline: [m4-outline](roadmap/m4-outline.md). |
| M5 | **Flagship story** | The flagship's own story and content: relocated from Chernobyl, starting life again on Everon in 1989 (housing, the dacha, food, the state factory, bus stops). Ideas epic #214. |

Until 2026-11-08 M2 was "Flagship town": Everon placement, reusable site
prefabs, and MainWorld. That work is now the closing step of M2, after the four
themes are proven in the playtest village.

We do not start a milestone's features before the previous milestone's
"done when" is true. New systems wait; playability does not.

## How we build: AI-first

This repository assumes the contributor is an AI agent, working in parallel
with other agents, and a human can still read and reason about everything.

- **The contract is short and true.** `AGENTS.md` is the only rulebook. A rule
  that is false or session-specific gets deleted, not worked around.
- **Every claim has proof.** Tests that can go red, a CI gate on every PR, a
  structural lint that ratchets, and a visual harness for the 3D world.
- **Work is scoped for one agent.** Roadmap items are written as scopes with
  files, acceptance proof, and no hidden context — so a human or an agent can
  pick one up cold.
- **Agents improve their own tools.** When a session is slowed by missing
  tooling — the MCP, the harness, the filming rig, a validator — fixing that
  tool is part of the work. Each PR names the friction it hit and either fixes
  it or files it as a scope. Over time the loop gets faster and more accurate.
- **Subtract before you add.** Deleting a wrong doc, a dead script, or a test
  that cannot fail is progress.

## Proof in the 3D world

Most mod repos stop at "it compiles". Ours proves the world plays:

- **Dedicated-server suite** (`tools\cli test`) for logic, world seams, and
  restarts.
- **Visual harness** (`agent-box/`) drives a rendering session as a player:
  walks to every site, presses the interaction, takes screenshots, and fails on
  white meshes, missing actions, blank menus, or misplaced sites.
- **Filming harness** records the same runs as video: short clips attached to
  PRs as visual proof, and scripted camera passes for community showcases.
  Both run on any machine with the game installed — the maintainer's PC today,
  remote playtest machines later.

## Community

- The Discord and GitHub are the front door. Playtest builds come with a
  guide (`docs/playtest.md`) and a feedback template.
- Every milestone ships with a short video made by the filming harness.
- Contributors find work in GitHub issues written as scopes. Good first scopes
  are small, self-contained, and carry their acceptance proof.

## Not now

- New feature systems beyond the release scopes, until M1 is done.
- Rented hosting before M4 goes 24/7; the maintainer's PC hosts playtests and the first M4 sessions.
- Monetization of the framework, and anything on the flagship beyond cosmetic
  donations.
- Scheduled and organised events (parked for a later phase).
