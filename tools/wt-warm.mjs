// tools/wt-warm.mjs — background filler for the warm worktree pool (tools/wt.mjs).
//
// `wt new` and `wt prune` start it detached and hidden: `node tools/wt-warm.mjs
// fill <main-checkout>`. It runs at below-normal priority (git inherits it),
// makes at most one tree at a time, and exits when the pool holds K trees, the
// disk is under the warm floor, or another filler already runs.
import { constants, setPriority } from "node:os";
import { wtWarmFill } from "./wt.mjs";

const [cmd, root] = process.argv.slice(2);
if (cmd !== "fill" || !root) {
  console.log("usage: node tools/wt-warm.mjs fill <main-checkout>");
  process.exit(2);
}
try {
  setPriority(0, constants.priority.PRIORITY_BELOW_NORMAL);
} catch { /* not allowed: run at normal priority */ }
try {
  wtWarmFill(root, { log: (m) => console.log(m) });
} catch (e) {
  console.log(`wt-warm: ${e.message}`);
  process.exit(1);
}
