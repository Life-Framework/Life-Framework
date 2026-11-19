// A sign prefab's material never resolves to a placeholder or black.
// inputs: addons/**
//
// A vanilla sign body (SignGeneric_01, SignWall_01_*, SignCommercial_01, SignLarge_01) is one mesh with TWO
// material slots: the face and a far-LOD slot (`<face>_MLOD`, or just `MLOD` on the commercial fascia). The
// base prefab assigns neither, so each bakes the generic placeholder. A sign prefab that retargets only the
// face slot to `LF_SignFace.emat` ($rendertarget) draws its printed face up close, then switches to the
// placeholder LOD a few metres out: the plate went black in play (`SignGeneric_01_Base_161241`, the pole
// plate, and every legacy body like it). GeneralStore and GunStore already carried both slots.
//
// Fails, with file:line:
//   - a prefab that assigns a known body's face slot but not its LOD slot;
//   - an AssignedMaterial whose .emat is missing from every addon, so the slot resolves to a placeholder;
//   - an assigned sign .emat (under `Assets/Signs/`) that has no `$rendertarget` or texture map and a black
//     or missing `Color`.
//
// LF_SIGNMAT_ROOT points the scan at another tree (the selftest).
//
// red-proof: 2026-11-19, tools/validation/sign-materials-selftest.mjs plants a pole plate with only the
// SignGeneric_01 slot: `FAIL ...:5 assigns SignGeneric_01 but not SignGeneric_01_MLOD`, exit 1; the real tree
// held 6 such prefabs (Plate, PlateSmall, Fascia, Posts, PolePlate, SiteFlag) before the LOD slots were added.
import { execFileSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { signMaterialProblems } from "../lib/sign-materials.mjs";

const root = process.env.LF_SIGNMAT_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");

function walk(dir, rel, out) {
  for (const name of readdirSync(dir)) {
    if (name === ".git" || name === "node_modules") continue;
    const p = join(dir, name);
    const r = rel ? `${rel}/${name}` : name;
    if (statSync(p).isDirectory()) walk(p, r, out);
    else out.push(r);
  }
}

function listFiles() {
  if (!existsSync(join(root, ".git"))) {
    const out = [];
    if (existsSync(join(root, "addons"))) walk(join(root, "addons"), "addons", out);
    return out;
  }
  const raw = execFileSync("git", ["ls-files", "-co", "--exclude-standard", "--", "addons"], { cwd: root, encoding: "utf8", maxBuffer: 1 << 28 });
  return raw.split("\n").filter(Boolean);
}

const files = listFiles();
const emats = new Map();
for (const f of files) if (f.endsWith(".emat")) emats.set(f.split("/").slice(2).join("/").toLowerCase(), f);

const bad = [];
for (const f of files) {
  if (!f.endsWith(".et")) continue;
  const text = readFileSync(join(root, f), "utf8");
  if (!text.includes("MaterialAssignClass")) continue;
  for (const p of signMaterialProblems(text, (path) => {
    const rel = emats.get(path.toLowerCase());
    return rel ? readFileSync(join(root, rel), "utf8") : null;
  })) bad.push(`${f}:${p.line} ${p.message}`);
}

if (bad.length) {
  for (const b of bad) console.log(`FAIL  ${b}`);
  console.log(`SUMMARY validate-sign-materials fail ${bad.length}`);
  process.exit(1);
}
console.log("PASS  validate-sign-materials: every sign body assigns its face and LOD slots to a real material");
