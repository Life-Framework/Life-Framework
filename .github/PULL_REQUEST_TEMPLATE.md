## Summary

<!-- What changed and why? One or two sentences is enough. -->

## Related

<!-- Link issues, Discord threads, or feature requests when you have them. -->
<!-- Example: Fixes #123 -->

## Verification

<!-- Check what applies. Run commands from the repo root. -->

- [ ] `node tools/cli.mjs validate` passes (also runs on pre-commit)
- [ ] Logic-only changes: `node tools/cli.mjs test --tier fast`
- [ ] World/prefab/spawn changes: `node tools/cli.mjs test` (full tier)
- [ ] New or updated `EL_Test_*` files include a `// red-proof:` comment
- [ ] No generated artifacts committed (`resourceDatabase.rdb`, `*.gproj.user`, `node_modules/`, etc.)

## Manual testing

<!-- Required for UI, multiplayer, JIP, or save/restart paths — the automated suite does not cover these. -->
<!-- Delete this section if not applicable. -->

**Steps:**

1.

**Expected:**

## Notes for reviewers

<!-- Optional: prefabs/GUIDs touched, breaking changes, screenshots, follow-ups. -->
