# Upgrade notes per release

Every release has one upgrade page. It answers one question for a server owner:
what do I change, and in what order, to move from the last release to this one.
The page lists the config changes, the save version changes, any migrations,
and the manual steps.

## The convention

- One file per release: `docs/upgrades/<x.y.z>.md`, from [TEMPLATE.md](TEMPLATE.md).
  The version is the one `tools\cli release` stamps.
- Write it in the release prep, in the same change as the release notes. A
  release without its upgrade page is not ready to promote.
- Add one line to `website/scripts/build-content.mjs` (an `addRepoDoc` call
  next to the others under "For server owners") so the page reaches the
  site under `/docs`. The website reads only the files it lists.
- Link the page from this index, newest first:

  | Release | Upgrade page |
  |---|---|
  | (none yet) | The first M3 release creates `0.1.0.md` |

## Where each fact comes from

Work from the diff between the previous `v*` tag and the new one. The
commands below work because tags sit on release commits.

| Section | Source |
|---|---|
| Summary and PR list | `tmp/release/notes-<x.y.z>.md` from `tools\cli release`. |
| Manual steps | The `## Manual testing` section of each PR, listed by `tools\cli promote`. |
| Config changes | `git diff <prev> <new> -- docs/config/config.schema.json`. A new or renamed field is a config change. Check the value a pack changed with `tools\cli pack explain <section.key> --config <config.json>`. |
| Config problems | `tools\cli config-check <config.json>` on the owner's file, after the change. |
| Save version changes | `git diff <prev> <new> -- docs/persistence.md`. Look for new "save version N" items and the "Save reset" section. The build stamp (save version 32) is the reference point. |
| Game build | `docs/ops/game-updates.md` changelog table: the game build the release was tested on. |
| Migrations | `docs/config/owner-config.md` and `docs/persistence.md`. Config migrations (#302 to #304) do not exist yet, so this section says "none" until they land. |

No tool writes the page for you yet. A generator that prints the config and
save-version diff for a tag is not built. Until then, fill the template by
hand from the sources above.

## Rules for the page

- Say what changed and what the owner does, in that order. Do not explain how
  the change was built.
- Every config change names the key in the owner file (`shops.defaultBuyPrice`),
  not the class field.
- A save version change says whether the old save loads, and whether a
  preflight or a reset is needed. A reset is announced here, once, at a release
  boundary (D6).
- A step that cannot be automated is marked **manual**, with the exact command
  or the exact click.
- Use plain words. No dates or promises that the release does not keep.
