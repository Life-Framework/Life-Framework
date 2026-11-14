// tools\cli asset-search: search many 3D asset sites through 3d-asset-server (docs/asset-pipeline/sources.md).
//   asset-search install                      clone the pinned commit into tools/mcp/3d-asset-server/ (gitignored) and build it
//   asset-search status                       installed or not, and the pinned commit
//   asset-search "<words>" [--slug <spec>] [--limit N] [--free] [--scrape] [--json]
// With --slug the spec's name, size and budget score the results and the shortlist is written to
// tmp/assets/<slug>/shortlist.md. It never buys, logs in or enters payment details.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { PIN, installed, install, rank, search, serverDir, shortlistMarkdown, writeShortlist } from "../assets/pipeline/assetserver.mjs";

export const help = 'asset-search install | status | "<words>" [--slug <spec>] [--limit N] [--free] [--scrape] [--json]   search 3D asset sites (3d-asset-server), shortlist with licence gate';

export async function main(args, { root }) {
  const [first] = args;
  if (first === "install") { install(root); return 0; }
  if (first === "status") {
    console.log(installed(root) ? `3d-asset-server ${PIN.slice(0, 7)} at ${serverDir(root)}` : "3d-asset-server not installed: tools\cli asset-search install");
    return installed(root) ? 0 : 1;
  }
  const opt = (n) => { const i = args.indexOf(n); return i >= 0 ? args[i + 1] : undefined; };
  const flag = (n) => args.includes(n);
  const free = args.filter((a, i) => !a.startsWith("--") && !["--slug", "--limit"].includes(args[i - 1]));
  if (!free.length) { console.log(help); return 2; }
  const slug = opt("--slug");
  const specFile = slug ? join(root, "tools", "assets", slug, "spec.json") : null;
  const spec = specFile && existsSync(specFile) ? JSON.parse(readFileSync(specFile, "utf8")) : { name: free.join(" "), search: free.join(" ") };
  const query = free.join(" ");
  const found = await search(root, { query, limit: Number(opt("--limit") || 12), scrape: flag("--scrape"), freeOnly: flag("--free") });
  const ranked = rank(found.candidates, { ...spec, search: `${spec.search || ""} ${query}` });
  if (flag("--json")) { console.log(JSON.stringify({ ranked, links: found.links, reports: found.reports }, null, 1)); return 0; }
  const text = shortlistMarkdown(slug || "search", spec, ranked, found.links, { query });
  console.log(text);
  for (const r of found.reports.filter((x) => x.status === "error" || x.status === "timeout")) console.log(`  ${r.provider}: ${r.status} ${r.error || ""}`);
  if (slug) console.log(`wrote ${writeShortlist(root, slug, text)}`);
  return 0;
}
