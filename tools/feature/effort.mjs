// Effort per worktree, from the pool's run log (<main>/tmp/wt/pool/runs.jsonl): how many test
// runs and sessions a slug took, how long it waited and held the game, and the span from its
// first run to its last. The only per-feature cost record the repo keeps; used to compare a
// feature built with the scaffold against earlier ones (docs/architecture/agent-quality.md).
//
//   node tools/feature/effort.mjs <slug> [<slug>...]     one row per slug
//   node tools/feature/effort.mjs --since 2026-11-10     every slug with a run since then
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { mainRootOf } from "../wt.mjs";

export function summarise(rows) {
  const by = new Map();
  for (const r of rows) {
    if (!r.slug || !r.at) continue;
    const s = by.get(r.slug) ?? { slug: r.slug, tests: 0, testFail: 0, sessions: 0, waitS: 0, testS: 0, sessionS: 0, first: r.at, last: r.at };
    if (r.kind === "session") {
      s.sessions++;
      s.sessionS += r.heldS ?? 0;
    } else {
      s.tests++;
      s.testS += r.durS ?? 0;
      if (r.result && r.result !== "pass") s.testFail++;
    }
    s.waitS += r.waitS ?? 0;
    if (r.at < s.first) s.first = r.at;
    if (r.at > s.last) s.last = r.at;
    by.set(r.slug, s);
  }
  return [...by.values()].map((s) => ({ ...s, spanMin: Math.round((Date.parse(s.last) - Date.parse(s.first)) / 60000) }));
}

export function format(list) {
  const head = "slug                      tests fail sessions  test-min session-min wait-min span-min";
  const line = (s) => [s.slug.padEnd(25), String(s.tests).padStart(5), String(s.testFail).padStart(4), String(s.sessions).padStart(8), (s.testS / 60).toFixed(1).padStart(9), (s.sessionS / 60).toFixed(1).padStart(11), (s.waitS / 60).toFixed(1).padStart(8), String(s.spanMin).padStart(8)].join(" ");
  return [head, ...list.map(line)].join("\n");
}

const isMain = process.argv[1] && process.argv[1].replace(/\\/g, "/").endsWith("tools/feature/effort.mjs");
if (isMain) {
  const args = process.argv.slice(2);
  const here = join(import.meta.dirname, "..", "..");
  const file = join(mainRootOf(here), "tmp", "wt", "pool", "runs.jsonl");
  if (!existsSync(file)) {
    console.log(`effort: no pool log at ${file}`);
    process.exit(1);
  }
  const rows = readFileSync(file, "utf8").split("\n").filter(Boolean).map((l) => {
    try {
      return JSON.parse(l);
    } catch {
      return {};
    }
  });
  const i = args.indexOf("--since");
  const since = i === -1 ? null : args[i + 1];
  const slugs = args.filter((a, n) => !a.startsWith("--") && (i === -1 || n !== i + 1));
  let list = summarise(since ? rows.filter((r) => r.at >= since) : rows);
  if (slugs.length) list = slugs.map((s) => list.find((x) => x.slug === s) ?? { slug: s, tests: 0, testFail: 0, sessions: 0, waitS: 0, testS: 0, sessionS: 0, spanMin: 0 });
  else list.sort((a, b) => b.spanMin - a.spanMin);
  console.log(format(list));
}
