// tools\cli wait ship: retired with the ship queue (maintainer decision 2026-11-15). `wt ship` turns GitHub
// auto-merge on and returns; GitHub's validate check is the pre-merge gate and the main guard checks main after
// the merge (docs/worktrees.md "Ship", "Main guard").
export const help = "wait ship <slug>   retired: wt ship turns auto-merge on and returns; watch the PR with gh pr view";

export async function main() {
  console.log("wait ship: retired with the ship queue. 'wt ship' turns auto-merge on; check the PR with 'gh pr view <N> --json state,mergeStateStatus,statusCheckRollup'.");
  return 0;
}
