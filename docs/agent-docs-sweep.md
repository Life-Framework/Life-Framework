# Docs sweep

A daily background job keeps the player docs, the website and closed issues up
to date with what merged. It runs on the free DeepSeek model in opencode. The
builder agent takes the screenshots; the sweep only uses shots that are already
in the repo.

## Who does what

| Step | Who | When |
|---|---|---|
| Take 1–2 shots of the feature working | The Claude builder (`lf-builder`) | During its proof session, before shipping |
| Commit the shots to `website/public/docs-images/<issue>/` and add them to the PR body | The Claude builder | In the feature PR |
| Update the player docs and website pages, and comment on closed issues | DeepSeek docs sweep | Once a day, over every PR merged since the last sweep |

## Screenshot rule (builder)

- **Only when a player can see the change.** Skip pure logic.
- **Use the session you already have open** for proof. Never boot one just for a picture.
  - `node agent-box/harness.mjs shot world <Entity>` for a place or object.
  - `node agent-box/harness.mjs shot ui <name>` for a menu.
- **Look at the shot before keeping it.** It must show the feature clearly, with no debug overlay or error text.
- **Save at most 2 per issue,** at 1280 wide and under 500 KB each, as `website/public/docs-images/<issue>/<short-name>.png`.
- **Add them to the PR body** with a raw URL:
  `![what it shows](https://raw.githubusercontent.com/Life-Framework/Life-Framework/main/website/public/docs-images/<issue>/<short-name>.png)`
  The image goes live once the PR merges.

## Sweep rules (DeepSeek)

1. **Find what's new.** Read `docs/.last-sweep`, which holds the last merge commit swept. List the PRs merged since then:
   `gh pr list --state merged --search "merged:>=<date>" --json number,title,body,closingIssuesReferences,mergeCommit`
2. **Stick to the sources.** Facts come only from PR bodies, `docs/features.md` and the code. Never invent commands, places, prices or numbers. If something is unclear, leave it out.
3. **Update the player docs.**
   - Edit `docs/playtest.md` for what a player can now do.
   - Edit the website pages under `website/` that describe features. Use the shots from `website/public/docs-images/<issue>/` where they fit.
   - Don't touch `addons/`, `tools/`, tests or `AGENTS.md`.
4. **Comment on each closed issue** that has shots and no sweep comment yet: one short comment, a sentence on what shipped, the PR link and its shots. Skip issues without shots. Never reopen or relabel issues.
5. **Mark the sweep.** Set `docs/.last-sweep` to the newest merge commit you covered.
6. **Check the result:**
   - Run `tools\cli validate`.
   - In `website/`, run `npm run lint` and `npm run build`.
   - Both must pass.
7. **Ship one PR per sweep.** Work in `tools\cli wt new docs-sweep-<yyyymmdd>`, then ship with `tools\cli wt ship docs-sweep-<yyyymmdd> --skip-gate --body-file tmp/pr-body-docs-sweep.md`.
8. **Stay quiet when there's nothing to do.** If nothing merged, make no PR and reply with one line.
