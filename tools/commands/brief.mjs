// tools\cli brief <kind>: print the starter pack for one kind of work (docs/starter-packs/<kind>.md).
//
// One page of exact commands and known traps per kind, so an agent reads it and starts instead of reading the docs.
// `brief` with no kind lists them. The pack files are the source; this only prints one. (`pack` is the content-pack
// command and `slice brief` prints one issue's brief, so this has its own name.)
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export const help = "brief [<kind>]   print the starter pack for a kind of work: building | slice | map | town | grid (docs/starter-packs/)";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const PACKS_DIR = join(ROOT, "docs", "starter-packs");
export const ALIASES = { fitout: "building", "fit-out": "building", buildings: "building", slices: "slice", card: "slice", everon: "town", pipeline: "town", cell: "grid", cells: "grid" };

export function kinds(dir = PACKS_DIR) {
  if (!existsSync(dir)) return [];
  return readdirSync(dir).filter((n) => n.endsWith(".md") && n !== "README.md").map((n) => n.slice(0, -3)).sort();
}

export function resolveKind(name, dir = PACKS_DIR) {
  const k = String(name || "").toLowerCase().replace(/^--/, "");
  const kind = ALIASES[k] || k;
  return kinds(dir).includes(kind) ? kind : null;
}

export function pack(kind, dir = PACKS_DIR) {
  return readFileSync(join(dir, `${kind}.md`), "utf8");
}

export async function main(args) {
  const name = args.find((a) => !a.startsWith("-") || a.startsWith("--"));
  if (!name) {
    console.log("usage: tools\\cli brief <kind>   (the starter pack: exact commands and known traps)");
    for (const k of kinds()) console.log(`  ${k.padEnd(9)} ${pack(k).split("\n")[0].replace(/^#\s*Starter pack:\s*/, "")}`);
    return 0;
  }
  const kind = resolveKind(name);
  if (!kind) {
    console.log(`FAIL  no starter pack '${name}'. Kinds: ${kinds().join(", ")}`);
    return 2;
  }
  process.stdout.write(pack(kind));
  return 0;
}
