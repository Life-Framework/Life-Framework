// Proves the deterministic parts of the asset pipeline's anim stage offline (tools/assets/pipeline/
// anim.mjs, pose.mjs, vfx.mjs): the anim block's rules, the skeleton it gives the clean stage, the
// pose solver, the .txa writer, the workspace and effect text, and that what is committed for every
// asset with an anim block (clips, workspace, effect, the prefab's component) is what its spec
// generates. No Blender, no Workbench, no network.
// red-proof: 2026-11-14, one line broken per block, each restored after its FAIL was seen:
//   animProblems without the pivot-in-box test    -> FAIL anim: a pivot outside its box is refused
//   animNodes putting sockets at model positions  -> FAIL anim: a socket under a bone is stored relative to it
//   limbIk bending away from the pole (-h)        -> FAIL pose: the elbow bends towards its pole
//   limbIk without the reach clamp                -> FAIL committed: Anims/LF/Drill/Drill_push_in.txa is what clips/push_in.json solves to
//   writeTxa without the still-bone range         -> FAIL txa: a bone that does not move is one ranged key
//   clipProblems without the loop test            -> FAIL clip: a loop whose ends differ is refused
//   componentBlock quoting the enum               -> FAIL component: enums and vectors are written bare
//   sparksPtc with Repeat 0                       -> FAIL vfx: the sparks loop
//   LF_Drill.et with speed 1440 edited to 1000    -> FAIL committed: Prefabs/Items/Fence/LF_Drill.et: LF_AssetAnimComponent differs ...
//   Drill_push_loop.txa with one key edited       -> FAIL committed: Anims/LF/Drill/Drill_push_loop.txa is what clips/push_loop.json solves to
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import * as anim from "../assets/pipeline/anim.mjs";
import { KINDS } from "../assets/pipeline/kinds.mjs";
import * as pose from "../assets/pipeline/pose.mjs";
import { readGuid } from "../assets/pipeline/meta.mjs";
import { ptcMeta, sparksProblems, sparksPtc } from "../assets/pipeline/vfx.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");

export async function main() {
  let failed = 0;
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    console.log(`FAIL ${what}`);
  };
  const near = (a, b, tol = 1e-4) => a.every((v, i) => Math.abs(v - b[i]) <= tol);

  // ---- the anim block's rules ---------------------------------------------------------------------
  const base = () => ({
    slug: "grinder", name: "Grinder", kind: "gadget", asset_dir: "Assets/Items/Roleplay/Grinder", prefab_dir: "Prefabs/Items/Roleplay",
    anim: {
      bones: [{ name: "disc", motion: "spin", axis: "x", speed: 900, pivot: [0, 0.02, 0.1], box: [-0.02, -0.04, 0.04, 0.02, 0.08, 0.16] }],
      sockets: [{ name: "fx_disc", pos: [0, 0.02, 0.16], parent: "disc" }, { name: "grip", pos: [0, 0, 0] }],
      effects: [{ socket: "fx_disc", sparks: { rate: 40 } }],
      sounds: [{ event: "SOUND_LF_GRIND_LP", project: "{00000000000000AA}Sounds/Items/Grinder/Items_Grinder_Use.acp" }],
      character: { workspace: "LF_GrinderUse", control: 570, clips: { in: "clip:use_in", loop: "{4B74C849B2079E41}anims/anm/player/actions/supplies/build/p_erc_supplies_demolitionUS_low_loop.anm", out: "clip:use_in" } },
    },
  });
  ok(anim.animProblems(base(), new Set(["use_in"])).length === 0, `anim: a complete block passes (${anim.animProblems(base(), new Set(["use_in"])).join("; ")})`);
  const bad = (edit, pattern, what) => {
    const s = base();
    edit(s.anim);
    ok(anim.animProblems(s, new Set(["use_in"])).some((p) => pattern.test(p)), what);
  };
  bad((a) => { a.bones[0].pivot = [0, 0.5, 0.1]; }, /pivot is outside the box/, "anim: a pivot outside its box is refused");
  bad((a) => { a.bones[0].speed = 5; }, /speed in degrees per second/, "anim: a spin with no real speed is refused");
  bad((a) => { a.bones[0].motion = "wobble"; }, /motion must be one of/, "anim: an unknown motion is refused");
  bad((a) => { a.bones.push({ ...a.bones[0] }); }, /used twice/, "anim: two bones of one name are refused");
  bad((a) => { a.bones[0] = { name: "lid", motion: "hinge", axis: "y", pivot: [0, 0.02, 0.1], box: a.bones[0].box, angle: 95 }; a.sockets[0].parent = "lid"; }, /needs seconds/, "anim: a hinge without its time is refused");
  bad((a) => { a.sockets[0].parent = "blade"; }, /parent blade is not an anim bone/, "anim: a socket under a bone that does not exist is refused");
  bad((a) => { a.effects[0].socket = "nowhere"; }, /socket nowhere is not an anim socket/, "anim: an effect at a socket that does not exist is refused");
  bad((a) => { a.effects[0] = { socket: "fx_disc", particle: "Particles/x.ptc" }; }, /sparks \{ \.\.\. \}.*or particle/, "anim: an effect needs sparks or a {GUID} particle");
  bad((a) => { a.sounds[0].event = "grind"; }, /must be a SOUND_ name/, "anim: a sound event must be a SOUND_ name");
  bad((a) => { a.character.control = 12; }, /control must be an integer 500 to 999/, "anim: the control value must be in the custom range");
  bad((a) => { a.character.clips.loop = "push.anm"; }, /neither clip:<name> nor a \{GUID\}path/, "anim: a clip is clip:<name> or a {GUID} reference");
  bad((a) => { a.character.clips.in = "clip:missing"; }, /no clips\/missing\.json/, "anim: a custom clip needs its clips/<name>.json");
  bad((a) => { a.hold = { profile: "jerrycan", grip: "grip" }; }, /move the model with fit\.offset by 0 0\.44 0/, "anim: a grip away from the carry profile's hand says how far to move the model");
  bad((a) => { a.actions = { throw: { seconds: 0.5, speed: 90 } }; }, /speed must be 1 to 25/, "anim: a throw faster than an arm is refused");

  // ---- the skeleton for the clean stage -----------------------------------------------------------
  {
    const nodes = anim.animNodes(base());
    const by = Object.fromEntries(nodes.map((n) => [n.name, n]));
    ok(nodes[0].name === anim.ROOT_BONE && nodes[0].bone && nodes[0].parent === null, "anim: the skeleton starts with a root bone at the origin");
    ok(by.disc?.bone && by.disc.parent === anim.ROOT_BONE && near(by.disc.pos, [0, 0.02, 0.1]), "anim: a moving part is a bone at its pivot");
    ok(by.fx_disc && !by.fx_disc.bone && by.fx_disc.parent === "disc" && near(by.fx_disc.pos, [0, 0, 0.06]), "anim: a socket under a bone is stored relative to it");
    ok(by.grip?.parent === anim.ROOT_BONE && near(by.grip.pos, [0, 0, 0]), "anim: a socket with no parent hangs under the root");
    const w = anim.withAnim(KINDS.gadget, base());
    ok(w.kind.metaFlags.ExportSkinning && w.kind.metaFlags.ExportSceneHierarchy, "anim: a model with bones is imported with skinning and hierarchy");
    ok(w.kind.requiredBones.includes("disc") && w.kind.requiredSockets.includes("fx_disc"), "anim: the import check asks for the bone and the socket");
    ok(w.parts.some((p) => p.bone === "disc" && p.box.length === 6), "anim: the bone's box becomes a part the clean stage splits off");
    ok(anim.withAnim(KINDS.gadget, { ...base(), anim: undefined }).kind === KINDS.gadget, "anim: a spec with no anim block leaves the kind alone");
    let threw = false;
    try { anim.withAnim(KINDS.weapon, base()); } catch { threw = true; }
    ok(threw, "anim: bones on a weapon's vanilla skeleton are refused, not silently dropped");
  }

  // ---- the pose solver ----------------------------------------------------------------------------
  const sk = pose.loadSkeleton();
  {
    const rest = pose.fk(sk, pose.restPose());
    ok(Math.abs(rest.Head.pos[1] - 1.618) < 0.01 && Math.abs(rest.Head.pos[0]) < 1e-3, "pose: the rest pose stands 1.62 m to the head bone, on the centre line");
    ok(rest.RightHand.pos[0] > 0.3 && rest.LeftHand.pos[0] < -0.3, "pose: the right hand is on +x");
    ok(Math.abs(rest.LeftFoot.pos[1] - 0.1) < 0.01, "pose: the ankles rest 0.10 m up");
    const p = pose.restPose();
    const target = [0.1, 1.2, 0.42];
    const miss = pose.armIk(sk, p, "right", { hand: target, elbow: [0.5, 1.0, 0], fingers: [0, 0, 1], palm: [-1, 0, 0] });
    const w = pose.fk(sk, p);
    ok(miss < 1e-3 && near(w.RightHand.pos, target, 1e-3), "pose: the wrist reaches a target in range");
    const mid = pose.mul(pose.add(w.RightArm.pos, w.RightHand.pos), 0.5);
    ok(pose.dot(pose.sub(w.RightForeArm.pos, mid), pose.sub([0.5, 1.0, 0], mid)) > 0.01, "pose: the elbow bends towards its pole");
    const f = pose.handFrame(sk, p, "right");
    ok(pose.dot(f.fingers, [0, 0, 1]) > 0.999, "pose: the fingers point where they were told");
    ok(pose.dot(f.palm, [-1, 0, 0]) > 0.99, "pose: the palm faces where it was told");
    const far = pose.restPose();
    const out = pose.armIk(sk, far, "right", { hand: [0.1, 1.2, 1.5] });
    ok(out > 0.5, "pose: a target out of reach is reported as a miss, not solved through");
    const leg = pose.restPose();
    pose.legIk(sk, leg, "left", { foot: [-0.17, 0.2, 0.2] });
    const wl = pose.fk(sk, leg);
    ok(near(wl.LeftFoot.pos, [-0.17, 0.2, 0.2], 1e-3) && Math.abs((wl.LeftFoot.pos[1] - wl.LeftToe.pos[1]) - (rest.LeftFoot.pos[1] - rest.LeftToe.pos[1])) < 0.005, "pose: a stepped foot lands on its target with the sole flat");
    const before = pose.fk(sk, pose.restPose()), curled = pose.restPose();
    pose.curlFingers(sk, curled, "left", 60);
    const after = pose.fk(sk, curled);
    const palm = pose.handFrame(sk, pose.restPose(), "left").palm;
    ok(pose.dot(pose.sub(after.LeftHandIndex4.pos, before.LeftHandIndex4.pos), palm) > 0.01 && pose.dot(pose.sub(after.LeftHandThumb4.pos, before.LeftHandThumb4.pos), palm) > 0, "pose: curled fingers and the thumb close towards the palm");
    const prop = pose.restPose();
    pose.setProp(sk, prop, "left", [0.2, 1.0, 0.3]);
    ok(near(pose.fk(sk, prop).LeftHandProp.pos, [0.2, 1.0, 0.3], 1e-4), "pose: a held item's origin goes where it was put");
  }

  // ---- clips and the .txa -------------------------------------------------------------------------
  {
    const clip = {
      name: "wave", fps: 30, frames: 10, loop: true, bones: ["RightShoulder"],
      keys: [
        { frame: 0, arms: { right: { hand: [0.3, 1.5, 0.2] } } },
        { frame: 5, arms: { right: { hand: [0.35, 1.6, 0.2] } } },
      ],
    };
    const built = pose.buildClip(sk, clip);
    ok(built.frames.length === 11, "clip: frames 0 to numFrames are all solved");
    ok(near(built.frames[0].q.RightArm, built.frames[10].q.RightArm, 1e-9), "clip: a loop ends on its first key");
    ok(anim.clipProblems(sk, clip, built).length === 0, "clip: a clean clip has no problems");
    const txa = pose.writeTxa(sk, clip, built);
    ok(/#numFrames 10\n/.test(txa) && /#fps 30\n/.test(txa), "txa: the header carries the rate and the last frame");
    ok(!/\$node "Hips"|\$node "LeftArm"/.test(txa) && /\$node "RightShoulder"/.test(txa) && /\$node "RightHandIndex3"/.test(txa), "txa: an arm clip writes that arm and nothing else");
    ok(/\$node "RightShoulder" \{\n\s+\$keys q s \{\n\s+\$frame 0 10 \{/.test(txa), "txa: a bone that does not move is one ranged key");
    const back = pose.parseTxa(txa);
    ok(back.frames === 10 && back.keys.RightArm.length === 11 && near(back.keys.RightArm[5].q, built.frames[5].q.RightArm, 1e-6), "txa: what is written reads back as the solved rotations");
    ok(near(back.keys.RightShoulder[0].q, sk.by.RightShoulder.quat, 1e-6), "txa: an unposed bone is written at its rest rotation");
    const body = pose.clipBones(sk, "body");
    ok(body.includes("Hips") && body.includes("LeftToe") && !body.includes("Root") && !body.includes("EntityPosition"), "txa: a body clip writes the hips and everything under them, never the root");
    const open = { ...clip, keys: [clip.keys[0], { frame: 10, arms: { right: { hand: [0.35, 1.6, 0.2] } } }] };
    ok(anim.clipProblems(sk, open, pose.buildClip(sk, open)).some((p) => /does not close/.test(p)), "clip: a loop whose ends differ is refused");
    const reach = { ...clip, keys: [{ frame: 0, arms: { right: { hand: [0.3, 1.5, 1.4] } } }] };
    ok(anim.clipProblems(sk, reach, pose.buildClip(sk, reach)).some((p) => /out of reach/.test(p)), "clip: a hand target out of reach is refused");
    const sunk = { name: "sink", frames: 4, loop: true, bones: "body", keys: [{ frame: 0, hips: [0, -0.3, 0] }] };
    ok(anim.clipProblems(sk, sunk, pose.buildClip(sk, sunk)).some((p) => /under the ground/.test(p)), "clip: feet through the floor are refused");
  }

  // ---- the prefab component, the workspace and the effect text -----------------------------------
  {
    const s = base();
    const block = anim.componentBlock(s, "00000000000000F0", "  ", ["{00000000000000BB}Particles/LF/Grinder_fx_disc.ptc"]);
    ok(/m_eMotion SPIN\n/.test(block) && /m_vAngles 900 0 0\n/.test(block), "component: enums and vectors are written bare");
    ok(/m_sBone "disc"/.test(block) && /m_sParticle "\{00000000000000BB\}Particles\/LF\/Grinder_fx_disc\.ptc"/.test(block) && /m_sEvent "SOUND_LF_GRIND_LP"/.test(block), "component: strings are quoted");
    ok(block === anim.componentBlock(s, "00000000000000F0", "  ", ["{00000000000000BB}Particles/LF/Grinder_fx_disc.ptc"]), "component: the same spec writes the same bytes");
    ok(near(anim.boneAngles("z", 30), [0, 0, 30]) && near(anim.boneAngles("y", 30), [0, 30, 0]), "component: an axis becomes pitch, yaw or roll");
    const prefab = `GenericEntity {\n components {\n${block} }\n}\n`;
    ok(anim.prefabProblems(s, prefab, "x.et", ["{00000000000000BB}Particles/LF/Grinder_fx_disc.ptc"]).length === 0, "component: a prefab holding the generated block passes");
    ok(anim.prefabProblems(s, prefab.replace("900 0 0", "800 0 0"), "x.et", ["{00000000000000BB}Particles/LF/Grinder_fx_disc.ptc"]).length === 1, "component: a prefab that drifted from the spec is flagged");
    ok(anim.prefabProblems(s, "GenericEntity {\n}\n", "x.et", []).some((p) => /has no LF_AssetAnimComponent/.test(p)), "component: a prefab without the component is flagged");

    const g = (n) => n.toString(16).toUpperCase().padStart(16, "0");
    const guids = { ast: g(1), asi: g(2), agr: g(3), agf: g(4), aw: g(5), group: g(6), control: g(7), debug: g(8), preview: g(9), audio: g(10), ik: g(11), sw: g(12), t1: g(13), t2: g(14), t3: g(15) };
    const files = anim.workspaceFiles({ name: "LF_GrinderUse", dir: "Anims/LF/Grinder", control: 570, guids, clips: { in: "{AA}a.anm", loop: "{BB}b.anm", out: "{CC}c.anm" } });
    const agf = files["Anims/LF/Grinder/LF_GrinderUse.agf"], asi = files["Anims/LF/Grinder/LF_GrinderUse.asi"];
    ok(Object.keys(files).length === 5, "workspace: five files (template, instance, graph, graph file, workspace)");
    ok(/StartCondition "Gestures == 570"/.test(agf) && /Condition "Gestures == -1"/.test(agf), "workspace: the control value enters the state and -1 leaves it");
    ok(/"Use\.Default\.In" \{\n\s+Resource "\{AA\}a\.anm"/.test(asi) && /"Use\.Default\.Loop" \{\n\s+Resource "\{BB\}b\.anm"/.test(asi), "workspace: the instance maps in, loop and out to the clips");
    ok(files["Anims/LF/Grinder/LF_GrinderUse.agr"].includes(`"{${g(4)}}Anims/LF/Grinder/LF_GrinderUse.agf"`), "workspace: the graph names its graph file by GUID");

    const ptc = sparksPtc({ rate: 40, life: 0.5 });
    ok(!/Repeat 0/.test(ptc) && /EmitterDef streaks/.test(ptc), "vfx: the sparks loop");
    ok(/MaxNum 32\n/.test(ptc), "vfx: the particle cap follows the rate and the lifetime");
    ok(!/EmitterDef flash/.test(sparksPtc({ flash: false })), "vfx: the light flicker can be left out");
    ok(sparksProblems({ rate: 4000 }).length === 1 && sparksProblems({}).length === 0, "vfx: a silly rate is refused");
    ok(/PTCResourceClass PC : "\{4B4500E061CCD624\}/.test(ptcMeta("Particles/LF/x.ptc", g(1))), "vfx: the .meta names the particle config");
    ok(/BindingName "Gadget"/.test(anim.holdBlock(anim.HOLD_PROFILES.jerrycan)) && /Jerrycan_player\.asi/.test(anim.holdBlock(anim.HOLD_PROFILES.jerrycan)), "hold: the jerrycan profile writes the vanilla carry graph on the Gadget binding");
  }

  // ---- what is committed is what the specs generate ----------------------------------------------
  const addon = join(ROOT, "addons", "LifeFramework");
  const assets = join(ROOT, "tools", "assets");
  for (const slug of readdirSync(assets)) {
    const specFile = join(assets, slug, "spec.json");
    if (!existsSync(specFile)) continue;
    const spec = JSON.parse(readFileSync(specFile, "utf8"));
    if (!anim.hasAnim(spec)) continue;
    const a = spec.anim;
    const names = [...new Set(anim.clipRefs(a).map(([, r]) => anim.clipName(r)).filter(Boolean))];
    const have = new Set(names.filter((n) => existsSync(join(anim.clipsDir(ROOT, slug), `${n}.json`))));
    const problems = anim.animProblems(spec, have);
    ok(problems.length === 0, `committed: ${slug}'s anim block passes (${problems.join("; ")})`);
    if (problems.length) continue;
    const refs = {};
    for (const name of names) {
      const clip = anim.loadClip(ROOT, slug, name);
      const built = pose.buildClip(sk, clip);
      const bad2 = anim.clipProblems(sk, clip, built);
      ok(bad2.length === 0, `committed: ${slug} clip ${name} solves cleanly (${bad2.join("; ")})`);
      const rel = anim.clipRes(spec, name);
      const txa = join(addon, rel.replace(/\.anm$/, ".txa"));
      ok(existsSync(txa) && readFileSync(txa, "utf8") === pose.writeTxa(sk, clip, built), `committed: ${rel.replace(/\.anm$/, ".txa")} is what clips/${name}.json solves to (tools\\cli asset anim ${slug})`);
      ok(existsSync(join(addon, rel)), `committed: ${rel} is compiled`);
      refs[name] = `{${readGuid(join(addon, rel + ".meta"))}}${rel}`;
    }
    if (a.character) {
      const c = a.character, dir = c.dir || anim.animDir(spec);
      const asi = join(addon, dir, `${c.workspace}.asi`), agf = join(addon, dir, `${c.workspace}.agf`);
      ok(existsSync(asi) && existsSync(agf), `committed: ${slug}'s workspace ${dir}/${c.workspace} is in the addon`);
      if (existsSync(asi) && existsSync(agf)) {
        const text = readFileSync(asi, "utf8");
        for (const k of ["in", "loop", "out"]) {
          const want = anim.clipName(c.clips[k]) ? refs[anim.clipName(c.clips[k])] : c.clips[k];
          ok(text.includes(`Resource "${want}"`), `committed: ${c.workspace}.asi plays ${want} for ${k}`);
        }
        ok(readFileSync(agf, "utf8").includes(`"Gestures == ${c.control}"`), `committed: ${c.workspace}.agf starts on Gestures == ${c.control}`);
      }
    }
    for (const e of a.effects || []) {
      if (!e.sparks) continue;
      const rel = anim.effectRes(spec, e);
      ok(existsSync(join(addon, rel)) && readFileSync(join(addon, rel), "utf8") === sparksPtc(e.sparks), `committed: ${rel} is what the spec's sparks generate (tools\\cli asset anim ${slug})`);
    }
    if ((a.bones?.length || a.effects?.length || a.sounds?.length) > 0) {
      const rel = a.prefab || `${spec.prefab_dir}/${spec.name}.et`;
      ok(existsSync(join(addon, rel)), `committed: ${slug}'s prefab ${rel} exists`);
      if (existsSync(join(addon, rel))) for (const p of anim.prefabProblems(spec, readFileSync(join(addon, rel), "utf8"), rel, anim.effectRefs(spec, addon))) ok(false, `committed: ${p}`);
    }
  }

  if (failed) {
    console.log(`asset-anim-selftest: FAILED (${failed})`);
    return 1;
  }
  console.log("asset-anim-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
