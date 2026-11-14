// tools\cli ship-queue: the ship queue `wt ship` feeds (tools/ship-queue.mjs, docs/worktrees.md "Ship queue").
export const help = "ship-queue [status|wait <slug>|cancel <slug>]   the merge-train queue wt ship feeds: gates on pool machines, FIFO, parallel when files do not overlap";

export async function main(args, { root }) {
  return (await import("../ship-queue.mjs")).main(args, { root });
}
