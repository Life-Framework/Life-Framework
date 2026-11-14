// Proves a worktree delete never follows a game-addons junction into Steam.
// inputs: tools/**, agent-box/**, .githooks/**
// red-proof: with the unlink removed from removeWorktree, git 2.47.1.windows
// emptied the fixture install and this printed "removing the worktree followed
// the junction and deleted the install sentinel".
// red-proof (cleanRunFolder, 2026-11-14): with keep: [] in the lane fixture it printed
// "cleanRunFolder deleted resourceDatabase.rdb" and "reported 4 paths, expected 3".
// red-proof (warm-pool recycle, 2026-11-14): with recycleWorktree calling
// `git worktree remove --force` and rmSync directly, this printed "recycling a
// warm worktree followed the junction ..." and "recycling an unknown tree
// followed the junction ...".
import { selftestLinkSafety } from "../link-safety.mjs";

const failures = selftestLinkSafety();
if (failures.length) {
  console.log("link-safety-selftest: FAIL");
  for (const failure of failures) console.log(`  ${failure}`);
  process.exit(1);
}
console.log("link-safety-selftest: OK (worktree deleted, junction target kept)");
