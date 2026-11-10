// Run by .github/workflows/issue-state.yml when a PR merges into main:
//   node tools/issue-state.mjs <pr-number>
// Moves the issues the PR refs to in-dev / needs-playtest and reopens any an old
// `wt ship --closes` closed. Safe to re-run.
import { onPrMerged, realGh } from "./lib/release-gates.mjs";

const n = Number(process.argv[2]);
if (!Number.isInteger(n) || n <= 0) {
  console.log("usage: node tools/issue-state.mjs <pr-number>");
  process.exit(1);
}
try {
  const done = await onPrMerged(realGh(process.cwd()), n, (s) => console.log(s));
  console.log(`issue-state: PR #${n} -> ${done.length} issue(s) updated`);
} catch (err) {
  console.log(`FAIL  ${err.message}`);
  process.exit(1);
}
