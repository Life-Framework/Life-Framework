# Dialogue: conversation scenes with NPCs

Talking to an NPC that has an `LF_DialogueComponent` opens a scene on the talker's client: the
camera frames the NPC, its line is a caption with a recorded voice, its lips move, and the player
answers from a numbered list. Findings, limits and licence: `docs/architecture/npc-dialogue.md`.
Adding an NPC: `docs/asset-pipeline/dialogue.md`.

| File | Side | Does |
|---|---|---|
| `LF_DialogueOverrides.c` | both | Dev caption overrides (#970): `$profile:LifeFramework/dev/dialogue.json` read by `reload dialogue`, streamed to clients; the scene shows the override instead of the translated key |
| `LF_DialogueConfig.c` | both | The data classes and `LF_DialogueFlow`, the pure rules (which replies are offered, what is said, the mouth envelope, config problems). Nodes and lines are `LF_ConfigRecord`s: a pack patch of a dialogue adds, replaces, patches or removes them by id (`LF_DialogueConfig.Layered`, `docs/config/override-model.md` "Dialogue patches", #1109); a patch that leaves the conversation broken is dropped and the core plays |
| `LF_DialogueComponent.c` | both | On the NPC prefab; names its dialogue by id (`m_sDialogueId`, or a path in `m_sDialogue`), loads and checks the config once |
| `LF_DialogueRegistry.c` | both | Dialogue by id (#957): the core index `Configs/Dialogue/LF_DialogueIndex.conf` plus pack overlays (add, replace, remove an id). `LF_DialogueRegistry.Resolve(id)` |
| `LF_DialogueHook.c` | server | Base of a reply's condition or action; `Find("Name")` resolves class `LF_DialogueHook_Name` |
| `LF_Dialogue.c` | server | Sessions: start, choose, end; tells bystanders what to hear |
| `LF_DialoguePlayerController.c` | both | The RPCs, on the player's controller |
| `LF_DialogueScene.c` | client | Captions, replies, input, freezing the character, the harness hook |
| `LF_DialogueCamera.c` | client | The scripted camera: one over-the-shoulder shot, ease in and out, collision, hand-back |
| `LF_DialogueVoice.c` | client | Plays a line from the NPC's head and poses the face bones (lips, jaw, nod, blink) |
| `LF_DialogueMouth.c` | client | Pure face maths: envelope smoothing, opening cap, syllable size, nod and blink timing |

Rules:

- The config is generated (`tools\cli asset dialogue <npc>`). Edit
  `tools/assets/dialogue/<npc>/dialogue.json`, never the `.conf`.
- The server sends ids, never text. The client sends an NPC and a reply index, nothing else.
- A hook lives in the feature that owns the work and reuses that feature's server entry point.
  Hooks hold no state.
- Every way out of the scene restores the camera, the controls and the HUD (`LF_DialogueScene.Close`).
- An NPC whose config is missing or broken talks the way it did before; the reason is logged once.
- A pack patches a dialogue by node and line id (`m_sOp`), never by copying the `.conf`. Replies have no id: a node patch replaces its reply list. Tests `dialogue/override-verbs`, `config/pack-patch-overlays`.

Converted so far: the town clerk (`Prefabs/Characters/Quests/QuestGiver_Clerk.et`, hooks in
`Feature/Quests/LF_QuestDialogue.c`).

Not done: the NPC does not turn or gesture; a late joiner does not hear a line in progress; the
lip pose was tuned on one head (`LF_DialogueVoice.DEFAULT_RIG`).
