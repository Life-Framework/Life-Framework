// Proves the asset states stage offline (tools/assets/pipeline/states.mjs, docs/asset-pipeline/states.md)
// inputs: tools/**, agent-box/**, .githooks/**, addons/**, docs/**, website/**, video/**
// and the concept rubric's implementability half (concepts.mjs): the hand-pose maths against what the
// engine was measured to do, the spec checks, the prefab writer, that every committed spec with states
// matches its prefab, and that a concept set with states is picked on looks and build together.
// No Meshy, no Blender, no Workbench, no session.
// red-proof: 2026-11-14, one line broken per block, each restored after its FAIL was seen:
//   additiveMatrix as Rz*Ry*Rx instead of Rz*Rx*Ry -> FAIL states: HandAdditiveRotation 90 -52 0 lands as measured (GA)
//   handValues with t = the pose's grip alone        -> FAIL states: the cuffs' lock boxes land in the fist (measured GA translation, got 0.0000 0.0450 0.0000)
//   statesProblems without the handheld rule         -> FAIL states: a gadget that declares states must declare the hand
//   Handcuffs.et HandAdditiveTranslation set to 0 0 0 -> FAIL states: handcuffs: the prefab matches spec.states (HandAdditiveTranslation: the prefab has 0 0 0, the spec says 0.0197 0.045 0.0252)
//   recommended() ranking on RUBRIC alone            -> FAIL concepts: with states the recommended pick is the best combined score, not the prettiest
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import * as st from "../assets/pipeline/states.mjs";
import * as cc from "../assets/pipeline/concepts.mjs";
import { gadgetPrefab } from "../assets/pipeline/prefab-gadget.mjs";
import { STAGES, specProblems } from "../assets/pipeline/asset.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");

// Held-item readings from the harness verb heldxf (2026-11-14, MX991 hand pose, client 1 of a
// --profile full session): the item's axes and the LeftHandProp bone's axes, in the character frame.
const MEASURED = {
  MRx: [[90, 0, 0], [[0.9959, -0.0887, 0.0148], [0.0083, -0.073, -0.9973], [0.0896, 0.9934, -0.072]], [[0.9959, -0.0887, 0.0148], [0.0896, 0.9934, -0.072], [-0.0083, 0.073, 0.9973]]],
  MRy: [[0, 90, 0], [[0.0084, -0.0684, -0.9976], [0.0914, 0.9935, -0.0673], [0.9958, -0.0906, 0.0146]], [[0.9958, -0.0906, 0.0146], [0.0914, 0.9935, -0.0673], [-0.0084, 0.0684, 0.9976]]],
  MRz: [[0, 0, 90], [[-0.0908, -0.9938, 0.0648], [0.9958, -0.0901, 0.0142], [-0.0083, 0.0659, 0.9978]], [[0.9958, -0.0901, 0.0142], [0.0908, 0.9938, -0.0648], [-0.0083, 0.0659, 0.9978]]],
  GA: [[90, -52, 0], [[0.6064, -0.0025, 0.7951], [0.7898, -0.1133, -0.6028], [0.0916, 0.9936, -0.0667]], [[0.9958, -0.0909, 0.0145], [0.0916, 0.9936, -0.0667], [-0.0084, 0.0678, 0.9977]]],
  GB: [[90, 0, -52], [[0.6196, -0.1074, -0.7775], [-0.7796, 0.0304, -0.6255], [0.0908, 0.9938, -0.0649]], [[0.9958, -0.0901, 0.0142], [0.0908, 0.9938, -0.0649], [-0.0083, 0.0659, 0.9978]]],
  GD: [[90, 52, 0], [[0.6197, -0.1092, -0.7772], [-0.7795, 0.0299, -0.6257], [0.0916, 0.9936, -0.0666]], [[0.9958, -0.0908, 0.0146], [0.0916, 0.9936, -0.0666], [-0.0084, 0.0677, 0.9977]]],
};
const T = (A) => [0, 1, 2].map((i) => A.map((r) => r[i]));
const mul = (A, B) => A.map((r) => [0, 1, 2].map((j) => r[0] * B[0][j] + r[1] * B[1][j] + r[2] * B[2][j]));
const maxDiff = (A, B) => Math.max(...A.flat().map((v, i) => Math.abs(v - B.flat()[i])));

export async function main() {
  let failed = 0;
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    console.log(`FAIL ${what}`);
  };
  const cuffsHand = { pose: "fist", grip: [-0.032, 0, 0], up: "+z", out: "+x" };
  const spec = (states) => ({
    slug: "cuffs", name: "Cuffs", kind: "gadget", prompt: "A pair of cuffs, no text, no logos.", size_m: { length: 0.18, height: 0.032, width: 0.1 },
    budget: { lod0_tris: 8000, lod_ratios: [0.5, 0.25, 0.125], texture_px: 1024, credits: 45 },
    asset_dir: "Assets/Items/Roleplay/Cuffs", prefab_dir: "Prefabs/Items/Roleplay", states,
  });

  // ---- the engine's rotation and translation, against the readings ----------------------------
  for (const [name, [angles, item, bone]] of Object.entries(MEASURED)) {
    const seen = mul(item, T(bone)); // the item's axes in the bone frame
    ok(maxDiff(st.additiveMatrix(angles), seen) < 0.01, `states: HandAdditiveRotation ${angles.join(" ")} lands as measured (${name})`);
  }
  for (const a of [[90, -52, 0], [30, 40, -20], [-65, -60, -60], [0, 66, 0], [-90, 10, 0]]) {
    const back = st.additiveMatrix(st.additiveAngles(st.additiveMatrix(a)));
    ok(maxDiff(back, st.additiveMatrix(a)) < 1e-9, `states: additiveAngles inverts additiveMatrix for ${a.join(" ")}`);
  }
  const h = st.handValues(cuffsHand);
  ok(maxDiff([h.rotation], [[90, -51.98, 0]]) < 0.05, `states: the cuffs turn upright with the rings out of the fist (measured GA 90 -52 0, got ${h.rotation.join(" ")})`);
  ok(maxDiff([h.translation], [[0.0197, 0.045, 0.0252]]) < 0.0005, `states: the cuffs' lock boxes land in the fist (measured GA translation, got ${h.translation.map((v) => v.toFixed(4)).join(" ")})`);
  ok(h.graph.includes("Flashlight_MX991.agr") && h.instance.includes("MX991_player.asi") && h.variable === "MX991", "states: the fist pose borrows the MX991 graph, anim set and gadget variable");
  const turned = st.handValues({ ...cuffsHand, turn: 90 });
  const outAxis = turned.matrix[0];
  ok(Math.abs(outAxis[0] - -0.788) < 0.01 && Math.abs(outAxis[2] - 0.616) < 0.01, "states: hand.turn swings the out axis round the fist's column");

  // ---- spec checks ------------------------------------------------------------------------------
  ok(st.statesProblems(spec(undefined)).length === 0, "states: a spec without states has nothing to check (it behaves as before)");
  ok(st.statesProblems(spec({ ground: { up: "Up" } })).some((p) => /states\.hand is missing/.test(p)), "states: a gadget that declares states must declare the hand");
  ok(st.statesProblems(spec({ hand: cuffsHand, ground: { up: "Up" }, inventory: { angles: [-55, -30, 0], distance: 0.6 } })).length === 0, "states: the cuffs' states pass");
  ok(st.statesProblems(spec({ hand: { ...cuffsHand, out: "-z" } })).some((p) => /different axes/.test(p)), "states: up and out on one axis are refused");
  ok(st.statesProblems(spec({ hand: { ...cuffsHand, pose: "pinch" } })).some((p) => /pose must be one of/.test(p)), "states: an unmeasured pose is refused");
  ok(st.statesProblems(spec({ hand: { ...cuffsHand, grip: [0.5, 0, 0] } })).some((p) => /outside the model/.test(p)), "states: a grip outside the model is refused");
  ok(st.statesProblems(spec({ hand: cuffsHand, ground: { up: "Top" } })).some((p) => /ground\.up/.test(p)), "states: an unknown RestingUP is refused");
  ok(st.statesProblems(spec({ hand: cuffsHand, use: [{ name: "open", needs: "bone", bone: "strand" }] })).some((p) => /not in anim\.bones/.test(p)), "states: a built bone use state needs its bone");
  ok(st.statesProblems(spec({ hand: cuffsHand, use: [{ name: "open", needs: "bone", bone: "strand", planned: true }] })).length === 0, "states: a planned use state records its need without the bone");
  ok(specProblems(spec({ ground: { up: "Up" } })).some((p) => /states\.hand/.test(p)), "states: the spec stage reports states problems");
  ok(STAGES[STAGES.length - 1] === "states", "states: the stage runs last, after the prefab and anim stages");

  // ---- the prefab writer on a scaffolded gadget ---------------------------------------------------
  {
    const g16 = (n) => String(n).repeat(16);
    const guids = { anim: g16(1), attach: g16(2), preview: g16(3), mesh: g16(4), body: g16(5), gadget: g16(6), components: [] };
    const text = gadgetPrefab({ id: "A", xob: "{1}x.xob", g: { name_key: "#N", description_key: "#D" }, report: { size: [0.18, 0.032, 0.1] }, guids });
    const s = spec({ hand: cuffsHand, ground: { up: "Up", offset: [0, 0.01, 0] }, inventory: { angles: [-55, -30, 0], distance: 0.6, offset: [0, 0.01, 0] } });
    ok(st.statesDrift(text, s).length > 0, "states: a fresh scaffold differs from the cuffs' states");
    const once = st.applyStates(text, s);
    ok(st.statesDrift(once, s).length === 0, `states: applyStates writes every state value (${st.statesDrift(once, s).join("; ")})`);
    ok(st.applyStates(once, s) === once, "states: applying twice changes nothing");
    ok(once.includes("      CameraOffset 0 0.01 0\n") && once.includes("     RestingAdditiveOffsetLS 0 0.01 0\n"), "states: a missing value goes in its block at the block's depth");
    ok(once.split("HandAdditiveRotation").length === 2 && once.split("m_eAnimVariable").length === 2, "states: values are replaced, never doubled");
    ok(/AnimationAttachmentInfo "\{2{16}\}" \{\n {7}AnimGraph "\{82906ACD75DF8523\}/.test(once), "states: the hand graph is written inside the attachment, keeping its id");
  }

  // ---- every committed spec with states matches its prefab (the check that keeps them in step) -----
  for (const slug of readdirSync(join(ROOT, "tools", "assets"))) {
    const f = join(ROOT, "tools", "assets", slug, "spec.json");
    if (!existsSync(f)) continue;
    const s = JSON.parse(readFileSync(f, "utf8"));
    if (!st.hasStates(s)) continue;
    ok(st.statesProblems(s).length === 0, `states: ${slug}: spec.states passes (${st.statesProblems(s).join("; ")})`);
    const prefab = st.prefabPath(ROOT, s);
    if (!existsSync(prefab)) { ok(false, `states: ${slug}: no prefab at ${prefab}`); continue; }
    const drift = st.statesDrift(readFileSync(prefab, "utf8"), s);
    ok(drift.length === 0, `states: ${slug}: the prefab matches spec.states (${drift.join("; ")}); tools\\cli asset states ${slug} --fix`);
  }

  // ---- concepts: implementability for an asset with states --------------------------------------
  {
    const withStates = { states: { hand: cuffsHand, ground: { up: "Up" }, use: [] } };
    const rubric = cc.rubricFor(withStates);
    ok(cc.rubricFor({}).length === cc.RUBRIC.length && rubric.length === cc.RUBRIC.length + cc.IMPLEMENT_RUBRIC.length, "concepts: the build criteria join the rubric only when the asset declares states");
    const variants = Array.from({ length: 10 }, (_, i) => ({ label: `v${i + 1}`, detail: `variant ${i + 1}` }));
    const plain = cc.buildPrompts("A pair of cuffs.", variants);
    const built = cc.buildPrompts("A pair of cuffs.", variants, cc.implementPhrase(withStates));
    ok(plain.every((p) => !/grip area/.test(p.prompt)) && built.every((p) => /separate distinct parts.*clear grip area.*flat stable side.*not fused\. Single object/.test(p.prompt)), "concepts: with states every prompt asks for separate parts, a grip, a flat side and unfused moving parts");
    ok(cc.conceptProblems({ subject: "A pair of cuffs.", variants }, cc.implementPhrase(withStates)).length === 0, "concepts: the longer prompts stay under Meshy's 800 characters");
    const rec = { subject: "A pair of cuffs.", variants: variants.map((v) => ({ ...v })) };
    const looks = (n) => [n, n, n, n, n, n, n];
    for (let n = 1; n <= 10; n++) cc.applyScore(rec, n, [...looks(1), 1, 1, 1, 1, 1, 1], "", rubric);
    cc.applyScore(rec, 3, [...looks(2), 0, 0, 1, 1, 1, 0], "the prettiest: fused rings, floating chain", rubric);
    cc.applyScore(rec, 6, [...looks(1), 2, 2, 2, 2, 2, 2], "plain but separate rings, a lock box to grip", rubric);
    cc.applyScore(rec, 8, [...looks(2), 2, 2, 0, 2, 2, 2], "pretty and buildable, but nothing to hold", rubric);
    ok(cc.ranking(rec)[0].n === 3, "concepts: on looks alone the prettiest leads");
    ok(cc.recommended(rec, rubric).n === 6, "concepts: with states the recommended pick is the best combined score, not the prettiest");
    ok(cc.ranking(rec, rubric)[0].n === 8 && cc.recommended(rec, rubric).n !== 8, "concepts: a concept the hand cannot hold (hold_point 0) is never recommended");
    cc.pickByRubric(rec, new Date("2026-11-14T10:00:00Z"), rubric);
    ok(rec.pick.n === 6 && /combined looks and build/.test(rec.pick.why), "concepts: --pick takes the recommended concept and says why");
    const labels = cc.sheetLabels(rec, rubric);
    ok(/^6 v6 L7 B12 BEST$/.test(labels[5]) && /^3 v3 L14 B3$/.test(labels[2]), `concepts: the sheet shows looks and build per candidate and marks the recommended one (${labels[5]} | ${labels[2]})`);
    let threw = false;
    try { cc.applyScore(rec, 1, looks(1), "", rubric); } catch (e) { threw = /13 integers/.test(e.message); }
    ok(threw, "concepts: with states a score needs all 13 criteria");
  }

  if (failed) {
    console.log(`asset-states-selftest: FAILED (${failed})`);
    return 1;
  }
  console.log("asset-states-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
