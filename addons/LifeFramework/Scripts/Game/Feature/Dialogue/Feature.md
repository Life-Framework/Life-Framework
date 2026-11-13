# Dialogue: conversation scenes with NPCs

Talking to an NPC that has an `LF_DialogueComponent` opens a scene on the talker's client: the
camera frames the NPC, its line is a caption with a recorded voice, its lips move, and the player
answers from a numbered list. Findings, limits and licence: `docs/architecture/npc-dialogue.md`.
Adding an NPC: `docs/asset-pipeline/dialogue.md`.

| File | Side | Does |
|---|---|---|
| `LF_DialogueConfig.c` | both | The data classes and `LF_DialogueFlow`, the pure rules (which replies are offered, what is said, the mouth envelope, config problems) |
| `LF_DialogueComponent.c` | both | On the NPC prefab; loads and checks the config once |
| `LF_DialogueHook.c` | server | Base of a reply's condition or action; `Find("Name")` resolves class `LF_DialogueHook_Name` |
| `LF_Dialogue.c` | server | Sessions: start, choose, end; tells bystanders what to hear |
| `LF_DialoguePlayerController.c` | both | The RPCs, on the player's controller |
| `LF_DialogueScene.c` | client | Captions, replies, input, freezing the character, the harness hook |
| `LF_DialogueCamera.c` | client | The scripted camera: two shots, easing, collision, hand-back |
| `LF_DialogueVoice.c` | client | Plays a line from the NPC's head and drives the lip bones |

Rules:

- The config is generated (`tools\cli asset dialogue <npc>`). Edit
  `tools/assets/dialogue/<npc>/dialogue.json`, never the `.conf`.
- The server sends ids, never text. The client sends an NPC and a reply index, nothing else.
- A hook lives in the feature that owns the work and reuses that feature's server entry point.
  Hooks hold no state.
- Every way out of the scene restores the camera, the controls and the HUD (`LF_DialogueScene.Close`).
- An NPC whose config is missing or broken talks the way it did before; the reason is logged once.

Converted so far: the town clerk (`Prefabs/Characters/Quests/QuestGiver_Clerk.et`, hooks in
`Feature/Quests/LF_QuestDialogue.c`).

Not done: the NPC does not turn or gesture; a late joiner does not hear a line in progress; the
lip pose was tuned on one head (`LF_DialogueVoice.DEFAULT_RIG`).
