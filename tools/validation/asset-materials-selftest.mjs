// Proves the material-class step of the clean stage (tools/assets/pipeline/materials.mjs and
// inputs: tools/**, agent-box/**, .githooks/**
// blender/materials.py): the default plan, the spec checks, the shipped specs' classes, and, when
// Blender is installed, the remapped numbers. No Meshy, no Workbench.
// red-proof: 2026-11-13, one line broken per block, each restored after its FAIL was seen:
//   CLASSES.plastic.rough widened to [0.1, 0.9]           -> FAIL materials: plastic roughness stays 0.75..0.85
//   materials.py apply leaving roughness as Meshy gave it -> FAIL materials: plastic roughness stays 0.75..0.85
//   DEFAULT_MATERIALS without its metallic_above region   -> FAIL materials: a spec with no block is plastic with Meshy's metal kept
import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { blenderExe } from "../assets/pipeline/asset.mjs";
import { CLASSES, materialProblems, resolveMaterials } from "../assets/pipeline/materials.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");

export async function main() {
  let failed = 0;
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    console.log(`FAIL ${what}`);
  };
  const spec = (slug) => JSON.parse(readFileSync(join(ROOT, "tools", "assets", slug, "spec.json"), "utf8"));

  const plain = resolveMaterials({});
  ok(plain.default === "plastic" && plain.regions.some((r) => r.class === "metal" && r.metallic_above > 0), "materials: a spec with no block is plastic with Meshy's metal kept");
  ok(CLASSES.plastic.rough[0] >= 0.75 && CLASSES.plastic.rough[1] <= 0.85 && CLASSES.plastic.metal[1] <= 0.05, "materials: plastic roughness stays 0.75..0.85");
  ok(materialProblems({}).length === 0, "materials: no block is a valid spec");
  ok(materialProblems({ materials: { default: "nope" } }).some((p) => /nope/.test(p)), "materials: an undefined default class is refused");
  ok(materialProblems({ materials: { regions: [{ class: "metal" }] } }).some((p) => /exactly one/.test(p)), "materials: a region needs one selector");
  ok(materialProblems({ materials: { classes: { glass: { rough: [0.9, 0.1], metal: [0, 0] } } } }).some((p) => /lo above hi/.test(p)), "materials: an inverted range is refused");
  for (const slug of ["taser", "handcuffs", "spike-strip"]) {
    const s = spec(slug);
    ok(materialProblems(s).length === 0, `materials: ${slug} declares valid materials`);
    ok(!existsSync(join(ROOT, "tools", "assets", slug, "tone-maps.py")), `materials: ${slug} has no hand-written tone-map beside its spec`);
  }
  ok(resolveMaterials(spec("taser")).default === "plastic", "materials: the taser is plastic");

  const exe = blenderExe();
  if (existsSync(exe)) {
    const dir = mkdtempSync(join(tmpdir(), "lf-materials-selftest-"));
    const job = join(dir, "plan.json");
    const height = resolveMaterials(spec("spike-strip"));
    height.regions = [{ class: "spike_steel", above_height_m: 0.05 }];
    writeFileSync(job, JSON.stringify({ default: resolveMaterials({}), height }));
    const res = spawnSync(exe, ["--background", "--factory-startup", "--python-exit-code", "3", "--python", join(ROOT, "tools", "assets", "pipeline", "blender", "materials_selftest.py"), "--", job],
      { encoding: "utf8", timeout: 120000, windowsHide: true });
    const out = (res.stdout || "") + (res.stderr || "");
    for (const l of out.split("\n").filter((x) => x.startsWith("MATFAIL"))) ok(false, `materials: ${l.slice(8).trim()}`);
    ok(/MATDONE/.test(out), "materials: the pixel selftest ran to the end under Blender");
  } else {
    console.log("asset-materials-selftest: Blender not installed, pixel numbers skipped");
  }

  if (failed) {
    console.log(`asset-materials-selftest: FAILED (${failed})`);
    return 1;
  }
  console.log("asset-materials-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
