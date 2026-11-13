## Summary

<!-- What changed and why (1–2 sentences). What a player notices. -->

**In-world first:** <!-- the object the player walks up to, or why this needs a sheet (docs/decisions.md) -->

## Related

<!-- Refs #123 · Discord thread · feature request. Use "Refs", not "Closes": merging does not close an issue, a human verifies it first (issue goes in-dev, or needs-playtest when Manual testing has steps) -->

## Screenshots

<!-- Any change a player can see: the before | after table from `tools\cli evidence <issue> table`, embedded (not a path).
     `wt ship` refuses a visible change with no picture. Nothing to see? Write "No screenshots: <reason>". -->

## Verification

- [ ] `node tools/cli.mjs validate`
- [ ] `node tools/cli.mjs test --tier fast` — logic-only changes
- [ ] `node tools/cli.mjs test` — world, prefab, or spawn changes
- [ ] New `LF_Test_*` files include `// red-proof:` with the observed failure line
- [ ] Player-visible change: the polish checklist holds (`.claude/skills/polish/SKILL.md`), exceptions named
- [ ] No generated artifacts committed

## Review

<!-- Findings from `tmp/review-<slug>.md`, each fixed or dismissed with the reason (`wt ship` appends the file), or "Not reviewed: <why this diff is low risk>". -->

## Manual testing

<!-- Real steps here make the issue needs-playtest and appear in the promotion checklist. Nothing manual? Write "None" and why. -->

**Steps**

1.

**Expected**

## Notes

<!-- Prefabs, GUIDs, breaking changes, follow-ups. -->
