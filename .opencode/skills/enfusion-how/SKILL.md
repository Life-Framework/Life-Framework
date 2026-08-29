---
name: enfusion-how
description: "Use when you want a walkthrough of how a subsystem works in this mod or the base game, before changing it or answering a question about it. Trigger on 'how does X work', 'walk me through', 'what happens when', 'trace the flow'. A read-only investigation. Use ONLY when the question is about Enfusion/Reforger behavior or this mod's code."
---

# How a subsystem works

A walkthrough of how a subsystem behaves. Read-only: the deliverable is understanding, backed by cited evidence.

1. Name the subsystem and the question. One-line claim you could defend.
2. Research the base game first via **enfusion-api-research**. The base game is the source of truth. `game_read` the vanilla class that owns the subsystem; `api_search` the methods; `wiki_read` engine concepts.
3. Read the mod side. The EL_ feature folder that touches it, the matching prefab, the config. Follow the data: prefab component wiring → scripted logic → config values → localization.
4. Trace the concrete flow end to end. For a networked feature, trace both sides: what the server does and what the client sees. For persistence, trace save and load paths separately, then the restart.
5. Note where behavior depends on runtime conditions the code does not make obvious, and say so rather than guessing. Runtime verification belongs to **enfusion-verify**.
6. Cite every claim with a path and line, an `api_search` result, or a wiki page. No citation, no claim.

The answer is a walkthrough: the one-line claim, then the flow in order, then what remains unverified. This is also the seed for the Bug fix playbook when a defect lives in this subsystem.