# Bohemia Interactive forums post (draft, not posted)

Status: drafted. Not posted. Find the right section first (Arma Reforger, mods, or the community
recruitment area), read its posting rules, and check whether a server or mod announcement needs a
particular title format. Post from the maintainer's own account. Forum readers tend to be more
technical and read more, so this draft is plainer and longer than the Reddit one, and says what is
built and what is not.

**Attach:** `town-1-vista-dusk.jpg`, `scene-robber-1.jpg`, `police-2-uaz.jpg` (see
[the picture table](README.md#pictures)). Put the credit line from that table under them.

**Link:** `https://life-framework.org/join`. To count visits, add a `bohemia-forums` source first
([README](README.md#link-tags)) and use `https://life-framework.org/join/bohemia-forums`.

## Title

```
Life Framework (alpha): open-source life/RP framework and a 1989 Everon server, looking for playtesters and coders
```

## Body

```
Hello,

Life Framework is an open-source life and roleplay framework for Arma Reforger (MIT source; the Workshop build is under the APL). Alongside it we are building a 1989 Everon server, to prove the framework works outside a test world. It is an alpha.

WHAT EXISTS
A small playable town with a bank, a store, a car lot, a police post, medics, payphones, a fence and a few crimes. Interactions happen at things in the world, such as a clerk or a payphone, rather than in menus. Money, jobs, police and medic duties, and persistence are in. An in-game test suite runs against a dedicated server.

WHAT DOES NOT
Most systems have had few players. Expect bugs, rough UI and balance that is guesswork. There is no release date. Saves on the alpha server can be wiped with 24 hours' notice.

STRUCTURE
Two Workshop items. The core holds the systems and works for any setting; the pack holds this server's places, names and prices. The core never depends on the pack, so another setting can use the same core. A server needs both.
  Core: https://reforger.armaplatform.com/workshop/4550A2B656E1B569
  1989 Everon pack: https://reforger.armaplatform.com/workshop/B1E84D5687E11724

PLAYING
The server "Life Framework | Everon 1989 (Alpha)" is invite-only. Join our Discord, ask for an invite, and a staff member will send the password. We hold a playtest session each week, announced there.

RUNNING YOUR OWN
A server owner lists both Workshop items in the server config. Owner guides, a config generator and example server files are on the website.

HELPING
Playtest reports are the most useful thing. If you write code, the repository has a CONTRIBUTING guide, a short setup, and small "good first issue" tasks, several of them documentation or scripts that need no game.

HOW IT IS MADE
Mostly by AI coding agents working under the maintainer's direction. Every change is a public pull request that must pass checks, and a person plays the result before it counts as done. The framework began from the EveronLife mod's code (MIT, credited) and uses some Overthrow code with its author's permission, also credited.

Site and Discord invite: https://life-framework.org/join
Source: https://github.com/Life-Framework/Life-Framework

Questions and criticism are welcome.
```
