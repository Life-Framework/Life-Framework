# Npc: roles, outfits and talk

## Intent

A town NPC wears what its job wears (bank teller in a suit, quarry worker in a hard hat, police in a
peaked cap) and answers when spoken to: a greeting, what it does, directions, a rumour, goodbye.

## Interaction pattern

Walk up to any role NPC: the F key offers **Talk** (`LF_RolesAction`). It opens the existing
conversation scene (camera, captions, numbered replies; `Feature/Dialogue`). The role's lines are
captions only: no recordings. An NPC with its own authored dialogue (the town clerk) keeps it.

## Roles

- **Player sees:** NPCs dressed per job, with 2 to 3 outfits per busy role so a crowd is not cloned.
- **Owner and side:** `LF_Roles` builds the conversation from string keys on the server and every
  client alike; `LF_Dialogue` (server) still validates each reply. Nothing is replicated or saved.
- **Data:** `tools/assets/outfits/roles.json` is the only source. `tools\cli outfits generate` writes
  the outfit prefabs (`Prefabs/Characters/Npcs/Outfits/`, vanilla clothing, each carrying
  `LF_NpcRoleComponent`), `LF_RolesTable.generated.c`, the strings (`LF-NpcRole_<id>_*`,
  `LF-NpcTalk_*`) and each placed NPC's prefab reference in the layers. A placed NPC's outfit is
  `hash(entity name) % outfits`: stable per NPC. `LF_Roles.PickOutfit(role, seed)` is the same rule
  for an NPC spawned by code.
- **Not wired:** replies do not yet open the bank or a shop; those stay on the NPC's own F-key
  actions. Doctors, mechanics and store clerks have outfits and talk but no NPC placed yet.
- **Proof:** `npc/roles` (LOGIC), `npc/roles-world` (WORLD), `node agent-box/session.mjs roles-proof`.
