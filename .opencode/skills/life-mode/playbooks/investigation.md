# Investigation

A read-only question. How does X work, why was Y built this way, are we sure about Z. The deliverable is a cited answer, not a change.

1. Read the question and scope it. What exactly is being asked, and what is out of scope. Write the answer target as a one-line claim you could defend.
2. Research the base game first. The base game is the source of truth. Use the **enfusion-api-research** skill: `api_search` for classes and methods, `game_read` on the vanilla `.c` source, `wiki_read` for engine concepts, `game_browse` for structure. Do not rely on memory for how an Enfusion system behaves. Verify the class exists and what it inherits before reasoning about it.
3. Research the mod side. Read the EL_ source under `addons/LifeFramework/Scripts/Game/` that touches the subsystem. Note the component hierarchy (`EL_Component<Class T>`), the feature folders, and any `EL_*Manager` singletons involved.
4. Trace the actual data flow. Prefab wiring in `.et` files, config entries in `.conf`, component properties, RPC paths. Walk the chain from the place the question starts to the place the answer lives.
5. When the answer depends on runtime behavior the code does not make obvious, say so explicitly and mark it as "requires in-game check" rather than guessing. The Bug fix playbook or `enfusion-verify` can confirm it later.
6. Cite every claim. Each citation is a file path plus line number, an `api_search` result, or a wiki page. If you cannot cite it, it is not an answer, it is a guess. Never fabricate a citation.
7. Write the reply: the direct answer up front, then the evidence trail, then what remains uncertain or unverified.

**Reply:** the answer, the evidence trail (paths + lines), and anything left unverified.