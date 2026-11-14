// Stage "anim" of tools\cli asset: what moves. The spec's `anim` block declares, per asset,
//   bones      parts of the model that move by themselves (a spinning chuck, a hinged door)
//   sockets    named points on the model (where the sparks come out, where the hand grips)
//   effects    particle effects at a socket while the asset is active
//   sounds     sound events at a socket while the asset is active
//   character  the animation the character plays while using it: a workspace built from vanilla
//              clips ("{GUID}path.anm") and custom clips ("clip:<name>", posed in clips/<name>.json)
//   hold       how the character carries it (a vanilla carry profile, the grip socket)
//   actions    place and throw
// and this module turns that into: a skeleton for the clean stage, custom clips as .txa, the
// animation workspace text files, and the LF_AssetAnimComponent block for the prefab. The runtime
// half is Scripts/Game/Feature/AssetAnim/. Playbook: docs/asset-pipeline/animation.md.
import { existsSync, mkdirSync, readFileSync, writeFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { freshGuid, readGuid, usedGuids } from "./meta.mjs";
import * as pose from "./pose.mjs";
import { ptcMeta, sparksProblems, sparksPtc } from "./vfx.mjs";

export const MOTIONS = ["spin", "hinge", "slide"];
export const AXES = { x: [1, 0, 0], y: [0, 1, 0], z: [0, 0, 1] };
export const ROOT_BONE = "root";
const GUID_REF = /^\{[0-9A-F]{16}\}[\w/.\- ]+\.(\w+)$/;
const CLIP_REF = /^clip:([a-z][a-z0-9_]*)$/;

// Vanilla carry sets a held item can borrow whole (game 1.9, read from the paks 2026-11-14). `grip`
// is where the hand closes, in the vanilla item's own model space: an asset that borrows the set
// puts its grip socket there, so the hand lands on it.
export const HOLD_PROFILES = {
  // The fuel canister: one hand, arm down at the side, idle, walk, run and crouch, both stances.
  // Jerrycan_01.xob stands on its origin and is 0.47 m tall; the carry handle is at its top.
  jerrycan: {
    graph: "{7448534CC9BB5495}Assets/Items/Fuel/workspaces/Jerrycan.agr",
    instance: "{DB71459BEC12E8B1}Assets/Items/Fuel/workspaces/Jerrycan_player.asi",
    binding: "Gadget",
    hand: "left",
    grip: [0, 0.44, 0],
    modifiers: { StanceLimits: "0.025 1", SpeedLimit: 1.7, ADSSpeedLimit: 1.7, SpeedLimitHighready: 1.7, Supress1hShooting: 1, AllowGadgetUse: 1, CanBeEquippedInVehicle: 0, AllowJumping: 0 },
  },
};

// Vanilla one-hand clips for the timed actions (Anims/Anm/Player; GUIDs from the game's resource database).
export const ACTION_CLIPS = {
  place: "{A61183A1B5F89D85}anims/anm/player/actions/pick_place/p_1hd_erc_place_low.anm",
  throw: "{04FA200A046890EE}anims/anm/player/throwing/1handed/p_1hd_erc_throw_out.anm",
};

const isVec = (v, n = 3) => Array.isArray(v) && v.length === n && v.every((x) => typeof x === "number" && Number.isFinite(x));
const fmt = (n) => String(+Number(n).toFixed(4));
const vec = (v) => v.map(fmt).join(" ");

export const hasAnim = (spec) => !!spec.anim && Object.keys(spec.anim).length > 0;
export const clipName = (ref) => CLIP_REF.exec(ref || "")?.[1] || null;
export const animDir = (spec) => spec.anim?.dir || `Anims/LF/${spec.name}`;

// Every clip reference the anim block makes, as [where, ref].
export function clipRefs(anim) {
  const out = [];
  for (const [k, v] of Object.entries(anim.character?.clips || {})) out.push([`character.clips.${k}`, v]);
  for (const [k, a] of Object.entries(anim.actions || {})) if (a?.clip) out.push([`actions.${k}.clip`, a.clip]);
  return out;
}

// Everything wrong with an anim block, as sentences. `clips` is the set of clip names that have a
// clips/<name>.json; pass null to skip that check.
export function animProblems(spec, clips = null) {
  const a = spec.anim;
  const out = [];
  if (!a) return out;
  const names = new Set([ROOT_BONE]);
  const bones = new Set();
  for (const b of a.bones || []) {
    const at = `anim bone ${b.name || "?"}`;
    if (!/^[a-z][a-z0-9_]*$/.test(b.name || "")) out.push(`${at}: name must be lowercase letters, digits and underscores`);
    else if (names.has(b.name)) out.push(`${at}: the name is used twice (or is "${ROOT_BONE}")`);
    names.add(b.name);
    bones.add(b.name);
    if (!MOTIONS.includes(b.motion)) out.push(`${at}: motion must be one of ${MOTIONS.join(", ")}`);
    if (!AXES[b.axis]) out.push(`${at}: axis must be x, y or z (model space: x right, y up, z forward)`);
    if (!isVec(b.pivot)) out.push(`${at}: pivot must be [x, y, z] in metres, the point the part turns about`);
    if (!isVec(b.box, 6)) out.push(`${at}: box must be [x0, y0, z0, x1, y1, z1]; the faces inside it move with the bone`);
    else if (isVec(b.pivot) && b.motion !== "slide" && !inBox(b.pivot, b.box, 0.02)) out.push(`${at}: the pivot is outside the box, so the part would swing instead of turning in place`);
    if (b.motion === "spin" && !(Math.abs(b.speed) >= 30 && Math.abs(b.speed) <= 7200)) out.push(`${at}: a spin needs speed in degrees per second, 30 to 7200`);
    if (b.motion === "hinge" && !(Math.abs(b.angle) >= 1 && Math.abs(b.angle) <= 180)) out.push(`${at}: a hinge needs angle in degrees, 1 to 180`);
    if (b.motion === "slide" && !(Math.abs(b.distance) >= 0.001 && Math.abs(b.distance) <= 5)) out.push(`${at}: a slide needs distance in metres`);
    if (b.motion !== "spin" && !(b.seconds >= 0.05 && b.seconds <= 30)) out.push(`${at}: a ${b.motion} needs seconds, how long the move takes`);
  }
  const sockets = new Set();
  for (const s of a.sockets || []) {
    const at = `anim socket ${s.name || "?"}`;
    if (!/^[a-z][a-z0-9_]*$/.test(s.name || "")) out.push(`${at}: name must be lowercase letters, digits and underscores`);
    else if (names.has(s.name)) out.push(`${at}: the name is used twice`);
    names.add(s.name);
    sockets.add(s.name);
    if (!isVec(s.pos)) out.push(`${at}: pos must be [x, y, z] in metres, model space`);
    if (s.parent && !bones.has(s.parent)) out.push(`${at}: parent ${s.parent} is not an anim bone`);
  }
  for (const e of a.effects || []) {
    if (!sockets.has(e.socket)) out.push(`anim effect: socket ${e.socket || "?"} is not an anim socket`);
    if (e.sparks) out.push(...sparksProblems(e.sparks).map((p) => `anim effect at ${e.socket}: ${p}`));
    else if (!GUID_REF.test(e.particle || "") || !e.particle.endsWith(".ptc")) out.push(`anim effect at ${e.socket}: give sparks { ... } (an effect the vfx hook writes) or particle, a {GUID}path to a .ptc`);
  }
  for (const s of a.sounds || []) {
    if (!/^SOUND_[A-Z0-9_]+$/.test(s.event || "")) out.push(`anim sound: event ${s.event || "?"} must be a SOUND_ name`);
    if (!GUID_REF.test(s.project || "") || !s.project.endsWith(".acp")) out.push(`anim sound ${s.event}: project must be a {GUID}path to an .acp (tools\\cli asset sound writes it)`);
  }
  const c = a.character;
  if (c) {
    if (!/^[A-Z][A-Za-z0-9_]*$/.test(c.workspace || "")) out.push("anim character: workspace must be a PascalCase file name, such as LF_DrillPush");
    if (!(Number.isInteger(c.control) && c.control >= 500 && c.control <= 999)) out.push("anim character: control must be an integer 500 to 999, the Gestures value that starts the clip (unique per workspace)");
    for (const k of ["in", "loop", "out"]) if (!c.clips?.[k]) out.push(`anim character: clips.${k} is missing`);
  }
  for (const [where, ref] of clipRefs(a)) {
    const name = clipName(ref);
    if (name) { if (clips && !clips.has(name)) out.push(`anim ${where}: no clips/${name}.json for ${ref}`); }
    else if (!GUID_REF.test(ref) || !ref.endsWith(".anm")) out.push(`anim ${where}: "${ref}" is neither clip:<name> nor a {GUID}path to an .anm`);
  }
  const h = a.hold;
  if (h) {
    const profile = HOLD_PROFILES[h.profile];
    if (!profile) out.push(`anim hold: profile must be one of ${Object.keys(HOLD_PROFILES).join(", ")}`);
    if (!sockets.has(h.grip)) out.push(`anim hold: grip ${h.grip || "?"} is not an anim socket (the point the hand closes on)`);
    const grip = (a.sockets || []).find((s) => s.name === h.grip);
    if (profile && grip && isVec(grip.pos)) {
      const off = grip.pos.map((v, i) => v - profile.grip[i]);
      if (Math.hypot(...off) > 0.02) {
        out.push(`anim hold: the grip socket is at ${vec(grip.pos)} but the ${h.profile} carry closes the hand at ${vec(profile.grip)}; move the model with fit.offset by ${vec(off.map((v) => -v))} (added to what it has) and the socket to ${vec(profile.grip)}`);
      }
    }
  }
  for (const [k, act] of Object.entries(a.actions || {})) {
    if (!["place", "throw"].includes(k)) out.push(`anim actions: ${k} is not an action (place, throw)`);
    if (!(act.seconds >= 0.2 && act.seconds <= 10)) out.push(`anim actions.${k}: seconds must be 0.2 to 10, when the item leaves the hand`);
    if (k === "throw" && !(act.speed >= 1 && act.speed <= 25)) out.push("anim actions.throw: speed must be 1 to 25 m/s");
  }
  return out;
}

function inBox(p, box, slack = 0) {
  return [0, 1, 2].every((k) => p[k] >= Math.min(box[k], box[k + 3]) - slack && p[k] <= Math.max(box[k], box[k + 3]) + slack);
}

// The skeleton the clean stage builds for an asset with moving parts or sockets: one root bone at the
// origin, a bone per moving part at its pivot, and the sockets as empties under their bone. Returned
// in the kinds.mjs node format (positions relative to the parent).
export function animNodes(spec) {
  const a = spec.anim || {};
  if (!(a.bones?.length || a.sockets?.length)) return [];
  const nodes = [{ name: ROOT_BONE, parent: null, pos: [0, 0, 0], bone: true }];
  const pivot = { [ROOT_BONE]: [0, 0, 0] };
  for (const b of a.bones || []) {
    nodes.push({ name: b.name, parent: ROOT_BONE, pos: b.pivot, bone: true });
    pivot[b.name] = b.pivot;
  }
  for (const s of a.sockets || []) {
    const parent = s.parent || ROOT_BONE;
    nodes.push({ name: s.name, parent, pos: s.pos.map((v, i) => +(v - pivot[parent][i]).toFixed(5)) });
  }
  return nodes;
}

// A kind profile and a spec's parts with the anim block folded in: what the clean and import stages
// read. A weapon keeps its vanilla skeleton; its anim bones are not supported yet.
export function withAnim(kind, spec) {
  const nodes = animNodes(spec);
  if (!nodes.length) return { kind, parts: spec.parts || [] };
  if (kind.skeleton?.length) throw new Error(`${spec.kind} has a vanilla skeleton; anim bones and sockets on it are not supported yet`);
  const a = spec.anim;
  return {
    kind: {
      ...kind,
      skeleton: nodes,
      metaFlags: { ...(kind.metaFlags || {}), ExportSceneHierarchy: true, ExportSkinning: true },
      requiredBones: [...(kind.requiredBones || []), ROOT_BONE, ...(a.bones || []).map((b) => b.name)],
      requiredSockets: [...(kind.requiredSockets || []), ...(a.sockets || []).map((s) => s.name)],
      animReview: (a.bones || []).map((b) => ({ name: b.name, motion: b.motion, axis: AXES[b.axis], angle: b.angle || 0, distance: b.distance || 0 })),
    },
    parts: [...(spec.parts || []), ...(a.bones || []).map((b) => ({ name: b.name, bone: b.name, box: b.box }))],
  };
}

// ---------------------------------------------------------------- the prefab component
// SetBone takes its angles as pitch (about x), yaw (about y), roll (about z), in degrees.
export function boneAngles(axis, degrees) {
  return [axis === "x" ? degrees : 0, axis === "y" ? degrees : 0, axis === "z" ? degrees : 0];
}

// Where the vfx hook writes an effect it generates.
export const effectRes = (spec, e) => `Particles/LF/${spec.name}_${e.socket}.ptc`;

// The particle reference of every effect: its own, or the generated file's with the GUID its .meta
// holds (null until the stage has written it).
export function effectRefs(spec, addon) {
  return (spec.anim?.effects || []).map((e) => {
    if (!e.sparks) return e.particle;
    const guid = readGuid(join(addon, effectRes(spec, e) + ".meta"));
    return guid ? `{${guid}}${effectRes(spec, e)}` : null;
  });
}

// The LF_AssetAnimComponent block for the asset's prefab, from the anim block (and, for generated
// effects, their references).
export function componentBlock(spec, guid = "0000000000000000", indent = "  ", effects = (spec.anim?.effects || []).map((e) => e.particle)) {
  const a = spec.anim || {};
  const I = indent;
  const lines = [`${I}LF_AssetAnimComponent "{${guid}}" {`];
  const list = (name, cls, rows) => {
    if (!rows.length) return;
    lines.push(`${I} ${name} {`);
    for (const r of rows) {
      lines.push(`${I}  ${cls} "{${r.id}}" {`);
      // Strings (m_s...) are quoted; enums and vectors are written bare, as Workbench writes them.
      for (const [k, v] of r.props) lines.push(`${I}   ${k} ${k.startsWith("m_s") ? JSON.stringify(v) : v}`);
      lines.push(`${I}  }`);
    }
    lines.push(`${I} }`);
  };
  // Ids inside the component are derived from its own id, so a regenerated block is byte-identical.
  let n = 0;
  const id = () => (BigInt("0x" + guid) + BigInt(++n)).toString(16).toUpperCase().padStart(16, "0").slice(-16);
  list("m_aBones", "LF_AssetAnimBone", (a.bones || []).map((b) => ({
    id: id(),
    props: [
      ["m_sBone", b.name],
      ["m_eMotion", b.motion.toUpperCase()],
      ...(b.motion === "spin" ? [["m_vAngles", vec(boneAngles(b.axis, b.speed))]] : []),
      ...(b.motion === "hinge" ? [["m_vAngles", vec(boneAngles(b.axis, b.angle))], ["m_fSeconds", fmt(b.seconds)]] : []),
      ...(b.motion === "slide" ? [["m_vOffset", vec(AXES[b.axis].map((v) => v * b.distance))], ["m_fSeconds", fmt(b.seconds)]] : []),
    ],
  })));
  list("m_aEffects", "LF_AssetAnimEffect", (a.effects || []).map((e, i) => ({ id: id(), props: [["m_sSocket", e.socket], ["m_sParticle", effects[i] || ""]] })));
  list("m_aSounds", "LF_AssetAnimSound", (a.sounds || []).map((s) => ({ id: id(), props: [["m_sProject", s.project], ["m_sEvent", s.event]] })));
  lines.push(`${I}}`);
  return lines.join("\n") + "\n";
}

// The component block a prefab holds, with its id, or null.
export function prefabComponent(text) {
  const m = /^( *)LF_AssetAnimComponent "\{([0-9A-F]{16})\}" \{\n/m.exec(text);
  if (!m) return null;
  const close = text.indexOf(`\n${m[1]}}\n`, m.index);
  return { guid: m[2], indent: m[1], block: text.slice(m.index, close + m[1].length + 3) };
}

// What is wrong with a prefab's component against the spec. Empty means the prefab plays what the spec says.
export function prefabProblems(spec, prefabText, rel, effects) {
  const have = prefabComponent(prefabText);
  if (!have) return [`${rel} has no LF_AssetAnimComponent; add the block tools\\cli asset anim ${spec.slug} printed (tmp/assets/${spec.slug}/anim/component.txt)`];
  const want = componentBlock(spec, have.guid, have.indent, effects);
  return have.block === want ? [] : [`${rel}: LF_AssetAnimComponent differs from the spec's anim block; replace it with tmp/assets/${spec.slug}/anim/component.txt (tools\\cli asset anim ${spec.slug} --fix writes it)`];
}

// ---------------------------------------------------------------- the character workspace
// Five text files: template (.ast), instance (.asi), graph (.agr, .agf) and workspace (.aw). The
// graph is the one the CPR and the work poses use (Anims/Workspaces/Medic/LF_ReviveCPR.agf, copied
// from the vanilla rally emotes): an in, loop, out state machine entered when the Gestures variable
// equals `control`, attached to the player graph as a custom loiter.
export function workspaceFiles({ name, dir, control, clips, guids }) {
  const p = (ext) => `${dir}/${name}.${ext}`;
  const ref = (ext) => `{${guids[ext]}}${p(ext)}`;
  const ast = `AnimSetTemplateSource {
 Groups {
  AnimSetTemplateSource_AnimationGroup "{${guids.group}}" {
   Name "Use"
   Animations {
    "In"
    "Loop"
    "Out"
   }
   Columns {
    "Default"
   }
  }
 }
}`;
  const line = (anim, res) => ` AnimSetInstanceSource_Line "Use.Default.${anim}" {\n   Resource "${res}"\n  }`;
  const asi = `AnimSetInstanceSource {
 Template "${ref("ast")}"
 Lines {
 ${line("In", clips.in)}
 ${line("Loop", clips.loop)}
 ${line("Out", clips.out)}
 }
}`;
  const agr = `AnimSrcGraph {
 AnimSetTemplate "${ref("ast")}"
 ControlTemplate AnimSrcGCT "{${guids.control}}" {
  Variables {
   AnimSrcGCTVarInt Gestures {
    DefaultValue -1
    MinValue -2
    MaxValue 1000
   }
  }
  Commands {
   AnimSrcGCTCmd CMD_Gestures {
   }
  }
 }
 Debug AnimSrcGD "{${guids.debug}}" {
 }
 GraphFilesResourceNames {
  "${ref("agf")}"
 }
}`;
  const agf = `AnimSrcGraphFile {
 Sheets {
  AnimSrcGraphSheet ${name} {
   Nodes {
    AnimSrcNodeStateMachine MasterControl {
     EditorPos 0 -1
     states {
      AnimSrcNodeState Use {
       EditorPos 0 1
       Child "UseG"
       StartCondition "Gestures == ${control}"
       IsExit 1
      }
     }
    }
    AnimSrcNodeGroupSelect UseG {
     EditorPos 0 3
     Child "UseSequenceSTM"
     Group "Use"
     Column "Default"
    }
    AnimSrcNodeStateMachine UseSequenceSTM {
     EditorPos 0 5
     states {
      AnimSrcNodeState In {
       EditorPos -3 7
       Child "UseInSrc"
       StartCondition "true"
       TimeStorage "Real Time"
      }
      AnimSrcNodeState Loop {
       EditorPos 0 8
       Child "UseLoopSwitch"
       StartCondition "false"
       TimeStorage "Real Time"
      }
      AnimSrcNodeState Out {
       EditorPos 3 7
       Child "UseOutSrc"
       TimeStorage "Real Time"
       IsExit 1
      }
     }
     transitions {
      AnimSrcNodeTransition "{${guids.t1}}" {
       FromState "In"
       ToState "Loop"
       Duration "0.2"
       Condition "RemainingTimeLess(0.1)"
       PostEval 1
      }
      AnimSrcNodeTransition "{${guids.t2}}" {
       FromState "Loop"
       ToState "Out"
       Duration "0.5"
       Condition "Gestures == -1"
       Reentering "Blendout Only"
      }
      AnimSrcNodeTransition "{${guids.t3}}" {
       FromState "In"
       ToState "Out"
       Duration "0.5"
       StartTime "1-GetLowerTime()"
       Condition "Gestures == -1"
       PostEval 1
       Reentering "Blendout Only"
      }
     }
    }
    AnimSrcNodeSource UseInSrc {
     EditorPos -3 10
     Source "Use.In"
    }
    AnimSrcNodeSwitch UseLoopSwitch {
     EditorPos 0 10
     BlendTime 0.2
     FirstProbabilities "1"
     SwitchItems {
      AnimSrcNodeSwitchItem "{${guids.sw}}" {
       Child "UseLoopSrc"
       NextProbabilities "100"
      }
     }
    }
    AnimSrcNodeSource UseLoopSrc {
     EditorPos 0 12
     Source "Use.Loop"
    }
    AnimSrcNodeSource UseOutSrc {
     EditorPos 3 10
     Source "Use.Out"
    }
   }
  }
 }
}`;
  const aw = `BaseSource "${ref("aw")}" {
 AnimSetTemplate "${ref("ast")}"
 AnimSetInstances {
  "${ref("asi")}"
 }
 AnimGraph "${ref("agr")}"
 PreviewModels {
  AnimSrcWorkspacePreviewModel "{${guids.preview}}" {
   Model "{790CB9C809DE64B8}Assets/Characters/Animation/AnimTestChar_US_01.xob"
  }
 }
 AudioTesting AnimSrcWorkspaceAudioTesting "{${guids.audio}}" {
 }
 IkTesting AnimSrcWorkspaceIkTesting "{${guids.ik}}" {
 }
}`;
  return { [p("ast")]: ast, [p("asi")]: asi, [p("agr")]: agr, [p("agf")]: agf, [p("aw")]: aw };
}

const WS_CLASS = { ast: "AnimSetTemplateResourceClass", asi: "AnimSetInstanceResourceClass", agr: "AnimGraphResourceClass", agf: "AnimGraphFileResourceClass", aw: "AnimWorkspaceResourceClass" };
const PLATFORMS = ["PC", "XBOX_ONE", "XBOX_SERIES", "PS4", "HEADLESS"];
export function plainMeta(resPath, guid, cls, ps5 = false) {
  const rows = [...PLATFORMS, ...(ps5 ? ["PS5"] : [])].map((p, i) => `  ${cls} ${p}${i ? " : PC" : ""} {\n  }\n`).join("");
  return `MetaFileClass {\n Name "{${guid}}${resPath}"\n Configurations {\n${rows} }\n}\n`;
}

// The ids inside a workspace that already exists, so a rerun rewrites the same bytes.
const INNER = { group: /AnimationGroup "\{([0-9A-F]{16})\}"/, control: /AnimSrcGCT "\{([0-9A-F]{16})\}"/, debug: /AnimSrcGD "\{([0-9A-F]{16})\}"/, preview: /PreviewModel "\{([0-9A-F]{16})\}"/, audio: /AudioTesting "\{([0-9A-F]{16})\}"/, ik: /IkTesting "\{([0-9A-F]{16})\}"/, sw: /SwitchItem "\{([0-9A-F]{16})\}"/ };
function workspaceGuids(addon, dir, name, used) {
  const guids = {};
  const text = {};
  for (const ext of Object.keys(WS_CLASS)) {
    const rel = `${dir}/${name}.${ext}`;
    guids[ext] = readGuid(join(addon, rel + ".meta")) || freshGuid(used);
    text[ext] = existsSync(join(addon, rel)) ? readFileSync(join(addon, rel), "utf8") : "";
  }
  const all = Object.values(text).join("\n");
  for (const [k, re] of Object.entries(INNER)) guids[k] = re.exec(all)?.[1] || freshGuid(used);
  const tr = [...text.agf.matchAll(/AnimSrcNodeTransition "\{([0-9A-F]{16})\}"/g)].map((m) => m[1]);
  for (const [i, k] of ["t1", "t2", "t3"].entries()) guids[k] = tr[i] || freshGuid(used);
  return guids;
}

// ---------------------------------------------------------------- custom clips
export const clipsDir = (root, slug) => join(root, "tools", "assets", slug, "clips");
export function loadClip(root, slug, name) {
  const file = join(clipsDir(root, slug), `${name}.json`);
  if (!existsSync(file)) throw new Error(`no ${file}`);
  const clip = JSON.parse(readFileSync(file, "utf8"));
  if (!(clip.frames >= 1 && clip.frames <= 600)) throw new Error(`${file}: frames must be 1 to 600`);
  return clip;
}

// What is wrong with a solved clip, as sentences an agent can act on.
export function clipProblems(sk, clip, built) {
  const out = [];
  built.miss.forEach((m, i) => {
    for (const [limb, d] of Object.entries(m)) if (d > 0.03) out.push(`key ${i}: the ${limb} target is ${(d * 100).toFixed(0)} cm out of reach; move it closer to the body`);
  });
  if (clip.bones === "body" || !clip.bones) {
    built.frames.forEach((p, f) => {
      const w = pose.fk(sk, p);
      for (const toe of ["LeftToe", "RightToe"]) {
        if (w[toe].pos[1] < -0.03) out.push(`frame ${f}: ${toe} is ${(-w[toe].pos[1] * 100).toFixed(0)} cm under the ground`);
        if (w[toe].pos[1] > 0.25 && w[toe.replace("Toe", "Foot")].pos[1] > 0.3) out.push(`frame ${f}: ${toe.replace("Toe", "")} foot is ${(w[toe].pos[1] * 100).toFixed(0)} cm in the air`);
      }
    });
  }
  if (clip.loop) {
    const a = built.frames[0], b = built.frames[clip.frames];
    for (const bone of Object.keys(a.q)) {
      const d = Math.abs(a.q[bone].reduce((s, v, i) => s + v * (b.q[bone]?.[i] ?? 0), 0));
      if (d < 0.9999) { out.push(`the loop does not close: ${bone} differs between the first and the last frame`); break; }
    }
  }
  return [...new Set(out)].slice(0, 12);
}

export const clipRes = (spec, name) => `${animDir(spec)}/${spec.name}_${name}.anm`;

// ---------------------------------------------------------------- the stage
// Writes the custom clips (.txa and .meta, compiled to .anm by Workbench), the character workspace,
// and the component block; checks the prefab against it. ctx.workbench is absent in the selftest.
export async function stageAnim(ctx, { fix = false, preview = true } = {}) {
  const { root, spec, dir } = ctx;
  const a = spec.anim;
  if (!a) throw new Error(`${spec.slug} has no anim block in its spec (docs/asset-pipeline/animation.md)`);
  const names = [...new Set(clipRefs(a).map(([, r]) => clipName(r)).filter(Boolean))];
  const have = new Set(names.filter((n) => existsSync(join(clipsDir(root, spec.slug), `${n}.json`))));
  const problems = animProblems(spec, have);
  if (problems.length) throw new Error("anim problems:\n  " + problems.join("\n  "));
  const addon = join(root, "addons", "LifeFramework");
  const used = usedGuids(addon);
  const out = join(dir, "anim");
  mkdirSync(out, { recursive: true });
  const result = { clips: {}, workspace: null };

  // Custom clips.
  const sk = names.length ? pose.loadSkeleton() : null;
  const rebuilt = [];
  for (const name of names) {
    const clip = loadClip(root, spec.slug, name);
    const built = pose.buildClip(sk, clip);
    const bad = clipProblems(sk, clip, built);
    if (bad.length) throw new Error(`clip ${name}:\n  ` + bad.join("\n  "));
    const rel = clipRes(spec, name);
    const txaRel = rel.replace(/\.anm$/, ".txa");
    mkdirSync(dirname(join(addon, rel)), { recursive: true });
    const txa = pose.writeTxa(sk, clip, built);
    const before = existsSync(join(addon, txaRel)) ? readFileSync(join(addon, txaRel), "utf8") : null;
    if (before !== txa) writeFileSync(join(addon, txaRel), txa);
    const guid = readGuid(join(addon, rel + ".meta")) || freshGuid(used);
    if (!existsSync(join(addon, rel + ".meta"))) writeFileSync(join(addon, rel + ".meta"), plainMeta(rel, guid, "TXAResourceClass", true));
    if (before !== txa || !existsSync(join(addon, rel))) rebuilt.push(join(addon, txaRel));
    result.clips[name] = `{${guid}}${rel}`;
    writeFileSync(join(out, `${name}.frames.json`), JSON.stringify(previewJob(sk, clip, built, ctx, join(out, `preview-${name}`))));
    if (preview && ctx.blender) {
      ctx.blender(join(dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, "$1")), "blender", "pose_preview.py"), [join(out, `${name}.frames.json`)], { log: join(out, `preview-${name}.log`) });
      console.log(`anim: clip ${name}: ${clip.frames + 1} frames, pictures in ${join(out, `preview-${name}`)}`);
    }
  }
  if (rebuilt.length) {
    if (!ctx.workbench) throw new Error(`${rebuilt.length} clip(s) need Workbench to compile; run through tools\\cli asset`);
    for (const txa of rebuilt) compileClip(ctx, txa);
  }

  const resolve = (ref) => (clipName(ref) ? result.clips[clipName(ref)] : ref);
  if (a.character) {
    const c = a.character;
    const wsDir = c.dir || animDir(spec);
    const guids = workspaceGuids(addon, wsDir, c.workspace, used);
    const files = workspaceFiles({ name: c.workspace, dir: wsDir, control: c.control, guids, clips: { in: resolve(c.clips.in), loop: resolve(c.clips.loop), out: resolve(c.clips.out) } });
    mkdirSync(join(addon, wsDir), { recursive: true });
    for (const [rel, text] of Object.entries(files)) {
      writeFileSync(join(addon, rel), text);
      const ext = rel.split(".").pop();
      if (!existsSync(join(addon, rel + ".meta"))) writeFileSync(join(addon, rel + ".meta"), plainMeta(rel, guids[ext], WS_CLASS[ext]));
    }
    result.workspace = { graph: `{${guids.agr}}${wsDir}/${c.workspace}.agr`, instance: `{${guids.asi}}${wsDir}/${c.workspace}.asi`, control: c.control };
    console.log(`anim: workspace ${result.workspace.graph}\n      instance  ${result.workspace.instance}\n      Gestures == ${c.control} starts it (a custom loiter on this graph, as LF_WorkAnimPlayer does)`);
  }

  // Generated effects (the vfx hook): text the engine reads as it is.
  for (const e of a.effects || []) {
    if (!e.sparks) continue;
    const rel = effectRes(spec, e);
    mkdirSync(dirname(join(addon, rel)), { recursive: true });
    writeFileSync(join(addon, rel), sparksPtc(e.sparks));
    if (!existsSync(join(addon, rel + ".meta"))) writeFileSync(join(addon, rel + ".meta"), ptcMeta(rel, freshGuid(used)));
    console.log(`anim: effect ${rel}`);
  }
  const effects = effectRefs(spec, addon);

  // The prefab.
  const prefabRel = a.prefab || `${spec.prefab_dir}/${spec.name}.et`;
  const prefabAbs = join(addon, prefabRel);
  const needsComponent = (a.bones?.length || a.effects?.length || a.sounds?.length) > 0;
  if (needsComponent) {
    const existing = existsSync(prefabAbs) ? readFileSync(prefabAbs, "utf8") : "";
    const have = prefabComponent(existing);
    const block = componentBlock(spec, have?.guid || freshGuid(used), have?.indent || "  ", effects);
    writeFileSync(join(out, "component.txt"), block);
    if (!existing) console.log(`anim: no prefab at ${prefabRel} yet; its LF_AssetAnimComponent block is in ${join(out, "component.txt")}`);
    else {
      const bad = prefabProblems(spec, existing, prefabRel, effects);
      if (bad.length && fix && have) {
        writeFileSync(prefabAbs, existing.replace(have.block, block));
        console.log(`anim: rewrote LF_AssetAnimComponent in ${prefabRel}`);
      } else if (bad.length) throw new Error(bad.join("\n"));
      else console.log(`anim: ${prefabRel} matches the spec`);
    }
  }
  if (a.hold) {
    const profile = HOLD_PROFILES[a.hold.profile];
    writeFileSync(join(out, "hold.txt"), holdBlock(profile));
    console.log(`anim: hold profile ${a.hold.profile} (${profile.hand} hand); the ItemAnimationAttributes block is in ${join(out, "hold.txt")}`);
  }
  return result;
}

// The ItemAnimationAttributes and CharacterModifierAttributes a prefab needs to be carried like a profile's item.
export function holdBlock(profile, ids = { anim: "0000000000000001", attach: "0000000000000002", mods: "0000000000000003" }) {
  const mods = Object.entries(profile.modifiers).map(([k, v]) => `      ${k} ${v}\n`).join("");
  return `    ItemAnimationAttributes ItemAnimationAttributes "{${ids.anim}}" {
     AnimationAttachments {
      AnimationAttachmentInfo "{${ids.attach}}" {
       AnimGraph "${profile.graph}"
       AnimInstance "${profile.instance}"
       BindingName "${profile.binding}"
      }
     }
     AllowInspection 0
    }
    CustomAttributes {
     CharacterModifierAttributes "{${ids.mods}}" {
${mods}     }
    }
`;
}

function previewJob(sk, clip, built, ctx, out) {
  const fwd = (p) => p.split("\\").join("/");
  const pv = clip.preview || {};
  const frames = [...new Set(pv.frames || [0, Math.round(clip.frames / 2)])];
  const parents = Object.fromEntries(sk.nodes.map((n) => [n.name, n.parent]));
  return {
    out: fwd(out), parents, wall_z: pv.wall_z ?? null, views: pv.views || ["side", "front", "back"],
    frames: frames.map((f) => {
      const w = pose.fk(sk, built.frames[f]);
      return {
        label: `f${String(f).padStart(3, "0")}`,
        joints: Object.fromEntries(Object.entries(w).map(([k, v]) => [k, v.pos.map((x) => +x.toFixed(4))])),
        props: (pv.props || []).map((p) => ({ ...p, fbx: fwd(join(ctx.root, "tmp", "assets", p.asset, "clean", `${p.name}.fbx`)) })).filter((p) => existsSync(p.fbx)),
      };
    }),
  };
}

function compileClip(ctx, txa) {
  const anm = txa.replace(/\.txa$/, ".anm");
  const before = existsSync(anm) ? statSync(anm).mtimeMs : 0;
  const pluginDir = join(ctx.root, "addons", "LifeFramework", "Scripts", "WorkbenchGame", "ResourceManager");
  mkdirSync(pluginDir, { recursive: true });
  const plugin = "LF_AssetImportPlugin";
  writeFileSync(join(pluginDir, `${plugin}.c`), readFileSync(new URL(`./wb/${plugin}.c`, import.meta.url), "utf8"));
  const timeoutMs = Number(process.env.LF_ASSET_IMPORT_TIMEOUT_S || 90) * 1000;
  ctx.workbench(ctx.root, plugin, [`-lfAsset0=${txa}`], { timeoutMs, logFile: join(ctx.dir, "anim", "workbench.log") });
  if (!existsSync(anm) || statSync(anm).mtimeMs <= before) throw new Error(`Workbench did not compile ${anm} (log: ${join(ctx.dir, "anim", "workbench.log")})`);
  console.log(`anim: compiled ${anm} (${statSync(anm).size} bytes)`);
}
