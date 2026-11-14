// Proves the per-kind triangle budgets (tools/assets/pipeline/budgets.mjs) offline: no Blender, no
// inputs: tools/**, agent-box/**, .githooks/**, addons/**, docs/**, website/**, video/**
// Workbench. The numbers are checked against the vanilla range the table cites, and the three
// committed pipeline specs must pass the budget they are measured by.
// red-proof: 2026-11-14, one line broken, restored after its FAIL was seen:
//   checkBudget without the floor test    -> FAIL budget: a Meshy gadget spec at 3,000 triangles is under the gadget floor
import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { acceptedTaskId, specProblems } from "../assets/pipeline/asset.mjs";
import * as meshy from "../assets/pipeline/meshy.mjs";
import { BUDGETS, budgetFor, checkBudget, defaultBudget } from "../assets/pipeline/budgets.mjs";
import { KINDS } from "../assets/pipeline/kinds.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
// Vanilla LOD0 triangles read from the compiled .xob files on 2026-11-14 (see budgets.mjs).
const VANILLA = { handgun: [4132, 17515], gadget: [3686, 8681] };

export async function main() {
  let failed = 0;
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    console.log(`FAIL  ${what}`);
  };
  const spec = (slug) => JSON.parse(readFileSync(join(ROOT, "tools", "assets", slug, "spec.json"), "utf8"));

  // A kind with no mesh (material: fit "none") has no triangle budget.
  for (const kind of Object.keys(KINDS).filter((k) => KINDS[k].fit !== "none")) {
    const b = budgetFor(kind);
    ok(b.min <= b.lod0 && b.lod0 <= b.max, `budget: ${kind} has floor <= default <= cap`);
    ok(KINDS[kind].defaultTris === b.lod0 && KINDS[kind].maxTris === b.max, `budget: kinds.mjs takes ${kind}'s default and cap from budgets.mjs`);
    ok(b.ratios.every((r, i) => r < (i ? b.ratios[i - 1] : 1)) && b.ratios.length >= 2, `budget: ${kind} LOD ratios shrink`);
    ok(b.source >= b.lod0 * (kind === "deployable" ? 1 : 1.5), `budget: the ${kind} Meshy target is not under what LOD0 keeps`);
    const d = defaultBudget(kind);
    ok(checkBudget({ kind, budget: d }).length === 0, `budget: a new ${kind} spec passes its own budget (${checkBudget({ kind, budget: d })[0]})`);
  }
  ok(BUDGETS.weapon.lod0 >= VANILLA.handgun[0] && BUDGETS.weapon.lod0 <= VANILLA.handgun[1], "budget: a weapon's LOD0 sits inside the vanilla handgun range");
  ok(BUDGETS.gadget.lod0 >= VANILLA.gadget[0] && BUDGETS.gadget.lod0 <= VANILLA.gadget[1], "budget: a held gadget's LOD0 sits inside the vanilla gadget range");

  const cuffs = spec("money-bag"); // a gadget Meshy generated (the handcuffs are modelled in Blender now)
  ok(checkBudget({ ...cuffs, budget: { ...cuffs.budget, lod0_tris: 3000 } }).some((p) => /floor/.test(p)), "budget: a Meshy gadget spec at 3,000 triangles is under the gadget floor");
  ok(checkBudget({ ...cuffs, budget: { ...cuffs.budget, lod0_tris: 14000 } }).some((p) => /cap/.test(p)), "budget: a gadget over the cap is refused");
  ok(checkBudget({ ...cuffs, budget: { ...cuffs.budget, lod_ratios: [0.5, 0.6] } }).some((p) => /shrink/.test(p)), "budget: LOD ratios that grow are refused");
  ok(checkBudget({ ...cuffs, budget: { ...cuffs.budget, lod_ratios: [0.9, 0.5] } }).some((p) => /almost all/.test(p)), "budget: a LOD1 that keeps 90 % of LOD0 is refused");
  ok(checkBudget({ ...cuffs, budget: { ...cuffs.budget, lod_ratios: [0.5, 0.25, 0.01] } }).some((p) => /under 0.03/.test(p)), "budget: a last LOD under 3 % is refused");

  ok(checkBudget({ ...cuffs, source: { blender: "model.py" }, budget: { ...cuffs.budget, lod0_tris: 1400 } }).length === 0, "budget: a model our Blender script builds is not held to the floor");

  for (const slug of ["taser", "money-bag", "handcuffs", "spike-strip"]) {
    const s = spec(slug);
    ok(specProblems(s).length === 0, `budget: the committed ${slug} spec passes (${specProblems(s)[0]})`);
    const b = budgetFor(s.kind);
    ok(s.budget.lod0_tris >= b.min, `budget: ${slug} keeps at least the ${s.kind} floor`);
  }

  const body = meshy.remeshBody(cuffs, "task-1", 8000);
  ok(body.input_task_id === "task-1" && body.topology === "triangle" && body.target_polycount === 8000 && body.target_formats[0] === "glb", "remesh: the body names the task and asks for a triangle mesh at the LOD0 count");
  ok((() => { try { meshy.remeshBody(cuffs, "t", 50); } catch { return true; } return false; })(), "remesh: a count outside Meshy's 100 to 300000 is refused before the call");
  ok(acceptedTaskId(ROOT, "handcuffs") === "01a1d331-c93f-74f9-a5e6-917ef7864e43" && acceptedTaskId(ROOT, "taser") === "01a1d627-d633-768e-b46a-fb21785de34c", "remesh: the accepted generation in meshy.json is the one remeshed (the taser's is the image-to-3D task)");

  if (failed) {
    console.log(`asset-budget-selftest: FAILED (${failed})`);
    return 1;
  }
  console.log("asset-budget-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
