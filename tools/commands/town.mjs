// tools\cli town: the town pipeline's commands (docs/architecture/everon-world.md section 3).
//
// A thin dispatcher: `town <sub> ...` runs tools/town/<sub>.mjs, which exports `help` (one line) and
// `main(args, { root })` and returns an exit code. A later step (kit, fit, check, ready) is one new file in tools/town/
// and never touches this one (AGENTS.md golden rule 10).
import { existsSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

export const help = "town map|<step> ...   the town pipeline: `town map <place>` proposes the slot -> building mapping from a scout dossier (tools/town/<step>.mjs, one file each)";

const DIR = join(dirname(fileURLToPath(import.meta.url)), "..", "town");
const SUB_RX = /^[a-z][a-z0-9-]*$/;

async function subs() {
  const out = [];
  for (const f of readdirSync(DIR).filter((n) => n.endsWith(".mjs")).sort()) {
    try {
      const m = await import(pathToFileURL(join(DIR, f)).href);
      if (typeof m.main === "function") out.push({ name: f.slice(0, -4), help: m.help ?? "" });
    } catch (e) {
      out.push({ name: f.slice(0, -4), help: `(cannot load: ${e.message})` });
    }
  }
  return out;
}

export async function main(args, { root }) {
  const [sub, ...rest] = args;
  if (!sub || sub === "help" || sub === "--help") {
    console.log("usage: tools\\cli town <step> [args]\n");
    for (const s of await subs()) console.log(`  town ${s.help || s.name}`);
    return 0;
  }
  if (!SUB_RX.test(sub) || !existsSync(join(DIR, `${sub}.mjs`))) {
    console.log(`unknown town step '${sub}'. Steps:`);
    for (const s of await subs()) console.log(`  ${s.name}`);
    return 1;
  }
  const m = await import(pathToFileURL(join(DIR, `${sub}.mjs`)).href);
  if (typeof m.main !== "function") {
    console.log(`tools/town/${sub}.mjs exports no main()`);
    return 1;
  }
  return (await m.main(rest, { root })) ?? 0;
}
