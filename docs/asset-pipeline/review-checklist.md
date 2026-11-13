# Review checklist for a generated asset

For the agent before `wt ship`, and for the reviewer of the PR. Each line names its evidence.

## Source and cost

- [ ] The spec is committed (`tools/assets/<slug>/spec.json`) with the roadmap issue in `roadmap`.
- [ ] `tools\cli asset status <slug>` shows every stage `ok`.
- [ ] The PR states the credits spent (`credits.jsonl` sum) and the number of attempts, with each
      attempt's thumbnail and why the chosen one won ([review-generation.md](review-generation.md)).
- [ ] The Meshy key appears nowhere in the diff, the logs, or the PR.

## Files

- [ ] `tools\cli validate` passes: unique GUIDs, no orphan `.meta`, material chains, the addon
      boundary.
- [ ] The `.xob` bakes the asset's own `.emat` (the import stage checks it; a model that bakes
      the default material renders white).
- [ ] The asset is classified core or pack and has its rows in `docs/asset-inventory.md` and, for
      a pack resource that lands in the core, `docs/architecture/core-pack-inventory.md`.
- [ ] User-facing names are string table keys, and `tools\cli regen-localization` was run.

## In the game

- [ ] An `LF_Test` loads the prefab and checks what the feature depends on (a weapon: its
      component, muzzle, magazine well, the sockets the prefab names).
- [ ] `tools\cli look <x,z>` at the placed asset: it renders textured, the right way up, at a
      believable size next to vanilla props. The close-up is in the PR.
- [ ] `tools\cli audit placement --strict` passes for the layer it was placed in
      (`tools\cli place on|against|ground`, never typed coordinates).
- [ ] The inventory picture: harness `previews show <prefab>...` in a session, beside a vanilla
      item of the same kind. The picture is the engine's render of the prefab, so a wrong
      `PreviewRenderAttributes` shows here and nowhere else.
- [ ] For anything a player holds or uses on another player: one `--profile full` two-client
      session with its proof (`session.mjs <proof>`), then `session down`. Shoot the held item
      from the other client.
- [ ] The PR's `## Manual testing` lists what no agent can do. For a weapon that is always:
      aim down the sights, fire at a player, reload, drop and pick up.

## Quality verdict

Say one of these in the PR, with the picture that shows it:

- **Ships.** A player names the object, it fits 1989, the hand holds it properly.
- **Placeholder.** It works and is recognisable enough to play-test the feature; it gets a row
  in `docs/asset-inventory.md` with status `placeholder` and what the final asset needs.
- **Rejected.** Do not merge the asset; merge only what the attempt taught the pipeline.
