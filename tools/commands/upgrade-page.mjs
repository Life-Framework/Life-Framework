// tools\cli upgrade-page <fromRef> [<toRef>] [--version x.y.z] [--out <file>]: print the per-release upgrade
// page (docs/upgrades/TEMPLATE.md) filled in from the diff of two git refs (#294). Offline: git show only.
// Reads docs/config/config.schema.json, docs/config/config.migrations.json, the save version in
// tools/validation/record-shapes.json, and each pack's LF_PackManifest core range. toRef defaults to HEAD.
import { execFileSync } from "node:child_process";
import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { diffAll, parseManifest, renderPage } from "../upgrade/page.mjs";

export const help = "upgrade-page <fromRef> [<toRef>] [--version x.y.z] [--out F]   print the upgrade page filled from the config schema, migrations, save version and pack range diff between two refs";

const git = (root, args) => execFileSync("git", args, { cwd: root, encoding: "utf8", windowsHide: true, maxBuffer: 1 << 28, stdio: ["ignore", "pipe", "pipe"] });
const show = (root, ref, path) => { try { return git(root, ["show", `${ref}:${path}`]); } catch { return null; } };
const json = (text) => (text == null ? null : JSON.parse(text));

// Everything the diff reads, at one ref.
export function factsAt(root, ref) {
  const manifests = {};
  const files = git(root, ["ls-tree", "-r", "--name-only", ref, "addons"]).split("\n").filter((f) => /^addons\/LF_[^/]+\/Configs\/LF_PackManifest\.conf$/.test(f));
  for (const f of files) {
    const m = parseManifest(show(root, ref, f) ?? "");
    if (m) manifests[m.id] = { min: m.min, below: m.below };
  }
  return {
    schema: json(show(root, ref, "docs/config/config.schema.json")),
    migrations: json(show(root, ref, "docs/config/config.migrations.json")),
    shapes: json(show(root, ref, "tools/validation/record-shapes.json")),
    manifests,
  };
}

export async function main(args, { root } = {}) {
  root = root ?? process.cwd();
  const pos = [], opt = {};
  for (let i = 0; i < args.length; i++) {
    if (args[i] === "--version" || args[i] === "--out") opt[args[i].slice(2)] = args[++i];
    else pos.push(args[i]);
  }
  const [fromRef, toRef = "HEAD"] = pos;
  if (!fromRef) { console.error("usage: tools\\cli upgrade-page <fromRef> [<toRef>] [--version x.y.z] [--out <file>]"); return 2; }
  let was, now;
  try { was = factsAt(root, fromRef); now = factsAt(root, toRef); } catch (e) { console.error(`upgrade-page: cannot read a ref: ${String(e.message).split("\n")[0]}`); return 1; }
  if (!was.schema || !now.schema) { console.error("upgrade-page: docs/config/config.schema.json is missing at one of the refs"); return 1; }
  const template = readFileSync(join(root, "docs", "upgrades", "TEMPLATE.md"), "utf8");
  const page = renderPage(template, { fromRef, toRef, version: opt.version, d: diffAll(was, now) });
  if (opt.out) { writeFileSync(opt.out, page); console.log(`upgrade-page: wrote ${opt.out}`); } else process.stdout.write(page);
  return 0;
}
