// tools\cli wait ship <slug> [--budget s]: block until the slug's ship gate ends, in chunks an agent's shell
// allows (default 540 s, under the 10 min tool cap), and print one SUMMARY line.
// Exit 0 passed, 1 failed / cancelled / not queued, 2 the budget ran out with the gate still queued or running.
// docs/worktrees.md "Async ship".
export const help = "wait ship <slug> [--budget s]   block (default 540 s) until the ship gate passes or fails; one SUMMARY line, exit 0|1, 2 = still going, re-run";

export const DEFAULT_BUDGET_S = 540;

export function parseArgs(args) {
  const [what, slug, ...rest] = args;
  if (what !== "ship" || !slug || slug.startsWith("-")) return { error: "usage: tools\\cli wait ship <slug> [--budget s]" };
  const i = rest.indexOf("--budget");
  let budgetS = DEFAULT_BUDGET_S;
  if (i >= 0) {
    budgetS = Number(rest[i + 1]);
    if (!Number.isFinite(budgetS) || budgetS < 0) return { error: `--budget needs seconds, got '${rest[i + 1] ?? ""}'` };
  }
  return { slug, budgetS };
}

export async function main(args, { root }) {
  const a = parseArgs(args);
  if (a.error) {
    console.log(a.error);
    return 1;
  }
  const q = await import("../ship-queue.mjs");
  return q.waitFor(root, a.slug, { budgetMs: a.budgetS * 1000, quiet: true, resume: `tools\\cli wait ship ${a.slug}` });
}
