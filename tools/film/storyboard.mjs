// The storyboard format of the film pipeline (#395): one JSON file names the output, the scenes, and the shots (camera,
// duration, captions, voice). `tools\cli film-make video/<name>.json` validates it, shoots each shot through the camera
// rig (#392) and scene presets (#394) on a running session, and hands the clips to post-production (post.mjs).
//   format "lf-storyboard/1"; the schema is video/storyboard.schema.json, the checks that name a field are here.
//   film-make flags:  --shot S01,S02   re-shoot only those shots (the rest come from earlier takes)
//                     --group <name>   shoot only the shots of one location group (shot.group)
//                     --preview        low resolution, 10 fps, no voice-over or music
//                     --client <n>     film every shot on that session client (a one-client session for a storyboard that names several)
//                     --no-shoot       post-produce from the takes already on disk
//                     --shoot-only     shoot, skip post-production
// A shot whose camera says `pending` (or mode "still") is listed and skipped, never guessed.

import { existsSync, mkdirSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const FORMAT = "lf-storyboard/1";
export const MODES = ["firstPerson", "follow", "orbit", "static", "drone", "still"];
export const ASPECTS = { "16:9": 16 / 9, "9:16": 9 / 16, "1:1": 1, "4:5": 4 / 5 };
const LEAD_IN = 0.5; // seconds of take before the camera starts moving; post trims it

const isNum = (n) => typeof n === "number" && Number.isFinite(n);
const isVec = (v) => Array.isArray(v) && v.length === 3 && v.every(isNum);

// Validates a storyboard and throws an Error that names the field that is wrong.
export function validateStoryboard(b) {
  const bad = (f, why) => {
    throw new Error(`storyboard ${f}: ${why}`);
  };
  if (!b || typeof b !== "object" || Array.isArray(b)) bad("(root)", "not an object");
  if (b.format !== FORMAT) bad("format", `must be "${FORMAT}"`);
  if (typeof b.name !== "string" || !/^[A-Za-z0-9_-]+$/.test(b.name)) bad("name", "needs letters, digits, - or _ only");
  const out = b.output;
  if (!out || typeof out !== "object") bad("output", "missing");
  checkOutput("output", out, bad);
  (b.variants || []).forEach((v, i) => {
    if (typeof v.name !== "string") bad(`variants[${i}].name`, "missing");
    checkOutput(`variants[${i}]`, v, bad);
  });
  const sceneNames = new Set();
  (b.scenes || []).forEach((s, i) => {
    if (typeof s.name !== "string" || !s.name) bad(`scenes[${i}].name`, "missing");
    if (sceneNames.has(s.name)) bad(`scenes[${i}].name`, `"${s.name}" is listed twice`);
    sceneNames.add(s.name);
    if (s.hour !== undefined && !(isNum(s.hour) && s.hour >= 0 && s.hour < 24)) bad(`scenes[${i}].hour`, "must be a number from 0 to 24");
    if (s.weather !== undefined && typeof s.weather !== "string") bad(`scenes[${i}].weather`, "must be a weather state name");
  });
  if (!Array.isArray(b.shots) || !b.shots.length) bad("shots", "needs at least one shot");
  const ids = new Set();
  b.shots.forEach((s, i) => {
    const at = `shots[${i}]`;
    if (typeof s.id !== "string" || !s.id) bad(`${at}.id`, "missing");
    if (ids.has(s.id)) bad(`${at}.id`, `"${s.id}" is used twice`);
    ids.add(s.id);
    if (!(isNum(s.duration) && s.duration >= 1)) bad(`${at}.duration`, "must be a number of seconds, at least 1");
    if (s.group !== undefined && typeof s.group !== "string") bad(`${at}.group`, "must be a text name");
    if (s.kind === "title") {
      if (!Array.isArray(s.lines) || !s.lines.length || !s.lines.every((l) => typeof l === "string")) bad(`${at}.lines`, "a title shot needs its lines of text");
      return;
    }
    if (s.kind !== undefined && s.kind !== "shot") bad(`${at}.kind`, 'must be "shot" or "title"');
    if (s.scene !== undefined) {
      if (typeof s.scene !== "string") bad(`${at}.scene`, "must be a scene name");
      if (sceneNames.size && !sceneNames.has(s.scene) && s.scene !== "street") bad(`${at}.scene`, `"${s.scene}" is not in scenes[]`);
    }
    if (s.setup !== undefined && !(Array.isArray(s.setup) && s.setup.every((x) => x && typeof x.verb === "string"))) bad(`${at}.setup`, "must be a list of harness commands, each with a verb");
    if (s.hud !== undefined && s.hud !== "on" && s.hud !== "off") bad(`${at}.hud`, 'must be "on" or "off"');
    checkCamera(`${at}.camera`, s.camera, bad);
  });
  if (b.transition !== undefined && !(isNum(b.transition) && b.transition >= 0)) bad("transition", "must be a number of seconds, 0 or more");
  return true;
}

function checkOutput(at, o, bad) {
  if (!(Array.isArray(o.size) && o.size.length === 2 && o.size.every((n) => Number.isInteger(n) && n > 0 && n % 2 === 0))) bad(`${at}.size`, "must be [width, height], both even whole numbers");
  if (!(o.fps === undefined || (isNum(o.fps) && o.fps >= 1 && o.fps <= 60))) bad(`${at}.fps`, "must be 1 to 60");
  if (o.aspect !== undefined) {
    if (!(o.aspect in ASPECTS)) bad(`${at}.aspect`, `must be one of ${Object.keys(ASPECTS).join(", ")}`);
    if (Math.abs(o.size[0] / o.size[1] - ASPECTS[o.aspect]) > 0.01) bad(`${at}.aspect`, `"${o.aspect}" does not match size ${o.size[0]}x${o.size[1]}`);
  }
}

function checkCamera(at, c, bad) {
  if (!c || typeof c !== "object") bad(at, "missing");
  if (!MODES.includes(c.mode)) bad(`${at}.mode`, `must be one of ${MODES.join(", ")}`);
  if (c.pending !== undefined) {
    if (typeof c.pending !== "string" || !c.pending) bad(`${at}.pending`, "must say why the shot cannot be shot yet");
    return;
  }
  const num = (k, min) => {
    if (!(isNum(c[k]) && (min === undefined || c[k] >= min))) bad(`${at}.${k}`, min === undefined ? "must be a number" : `must be a number, ${min} or more`);
  };
  const vec = (v, k) => {
    if (!isVec(v)) bad(`${at}.${k}`, "must be [x, y, z] in world metres");
  };
  if (c.fov !== undefined && !(isNum(c.fov) && c.fov >= 10 && c.fov <= 120)) bad(`${at}.fov`, "must be 10 to 120 degrees");
  if (c.mode === "firstPerson") {
    if (!(Number.isInteger(c.client) && c.client >= 1)) bad(`${at}.client`, "a first-person shot needs the client number, 1 or more");
  } else if (c.mode === "follow") {
    vec(c.point, "point");
    num("distance", 1);
  } else if (c.mode === "orbit") {
    vec(c.point, "point");
    num("radius", 1);
    if (c.azimuth !== undefined) num("azimuth");
    if (c.arc !== undefined) num("arc");
    if (c.height !== undefined) num("height");
  } else if (c.mode === "static") {
    vec(c.position, "position");
    vec(c.lookAt, "lookAt");
  } else if (c.mode === "drone") {
    if (!(Array.isArray(c.keys) && c.keys.length === 2)) bad(`${at}.keys`, "a drone move needs exactly two keys, [from, to]");
    c.keys.forEach((k, i) => {
      vec(k.pos, `keys[${i}].pos`);
      vec(k.lookAt, `keys[${i}].lookAt`);
    });
  } else if (c.mode === "still") {
    if (typeof c.source !== "string" || !c.source) bad(`${at}.source`, "name the still (tools\\cli shots ...) this shot uses");
  }
}

const pos = (v) => v.map((n) => +n.toFixed(3)).join(" ");

// Harness `camera ...` argument strings that play one shot's camera for its duration. A first-person shot has none:
// the player's own view is the picture.
export function cameraCommands(shot) {
  const c = shot.camera;
  const d = shot.duration;
  if (!c || c.pending) return [];
  if (c.mode === "orbit") return [`orbit ${pos(c.point)} ${c.radius} ${d} ${c.azimuth ?? 0} ${c.arc ?? 140} ${c.height ?? 5}`];
  if (c.mode === "follow") return [`follow ${pos(c.point)} ${c.distance} ${d}`];
  if (c.mode === "static") return [`static ${pos(c.position)} ${pos(c.lookAt)} ${d} ${c.fov ?? 50}`];
  if (c.mode === "drone") return [`drone ${pos(c.keys[0].pos)} ${pos(c.keys[0].lookAt)} ${pos(c.keys[1].pos)} ${pos(c.keys[1].lookAt)} ${d} ${c.fov ?? 60}`];
  return [];
}

// Why a shot cannot be shot now, or "" when it can.
export function skipReason(shot) {
  if (shot.kind === "title") return "title card (post-production)";
  if (shot.camera.pending) return `pending: ${shot.camera.pending}`;
  if (shot.camera.mode === "still") return `still: ${shot.camera.source}`;
  return "";
}

// Which shots a run touches: --shot ids and --group, in storyboard order.
export function selectShots(b, { shots, group } = {}) {
  const want = shots ? new Set(shots) : null;
  if (want) for (const id of want) if (!b.shots.some((s) => s.id === id)) throw new Error(`--shot ${id}: no such shot (have ${b.shots.map((s) => s.id).join(", ")})`);
  if (group && !b.shots.some((s) => s.group === group)) throw new Error(`--group ${group}: no such group (have ${[...new Set(b.shots.map((s) => s.group).filter(Boolean))].join(", ")})`);
  return b.shots.filter((s) => (!want || want.has(s.id)) && (!group || s.group === group));
}

export const takePath = (b, shot, dir = join(ROOT, "agent-box", "reports", "film", b.name)) => join(dir, `${shot.id}.mp4`);

// The post-production storyboard (post.mjs `clips`) for the shots that have a take. A leading title shot becomes the
// title card, a trailing one the end card; a title in the middle is not supported by post yet and is left out.
export function toPostBoard(b, { preview = false, have = existsSync, dir, only } = {}) {
  const warnings = [];
  const clips = [];
  let titleCard;
  let endCard;
  b.shots.forEach((s, i) => {
    if (only && !only.has(s.id)) return; // --shot / --group post just those shots
    if (s.kind === "title") {
      if (i === 0) titleCard = { seconds: s.duration, lines: s.lines };
      else if (i === b.shots.length - 1) endCard = { seconds: s.duration, lines: s.lines };
      else warnings.push(`${s.id}: a title card between shots is not supported yet, left out`);
      return;
    }
    const src = takePath(b, s, dir);
    if (skipReason(s) || !have(src)) {
      warnings.push(`${s.id}: no take (${skipReason(s) || "not shot yet"}), left out`);
      return;
    }
    clips.push({ src, start: LEAD_IN, seconds: s.duration, captions: s.captions, vo: s.vo, audio: s.audio });
  });
  const size = preview ? b.output.size.map((n) => Math.round(n / 2 / 2) * 2) : b.output.size;
  const post = { name: b.name + (preview ? "-preview" : ""), title: b.title, size, fps: b.output.fps || 30, transition: b.transition, titleCard, endCard, clips, voice: b.voice };
  if (!preview) post.music = b.music;
  return { post, warnings };
}

// ---------------------------------------------------------------- shooting

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// Shoots the selected shots on the running session. Scenes are started and stopped as the run moves between them,
// each shot records through the engine recorder (LF_FilmRecorder), and the frames are deleted after the encode.
export async function shoot(b, { selected, preview = false, log = console.log, io, onClient }) {
  const eng = io.eng;
  const fps = preview ? 10 : Math.min(b.output.fps || 30, 30);
  const dir = join(ROOT, "agent-box", "reports", "film", b.name);
  mkdirSync(dir, { recursive: true });
  const scenes = new Map((b.scenes || []).map((s) => [s.name, s]));
  const results = [];
  let live = null;
  const cmd = (client, verb, args, ms = 8000) => {
    const r = io.send(client, { verb, args }, ms);
    log(`  ${verb} ${args} -> ${r.status} ${r.detail}`);
    return r;
  };
  for (const s of selected) {
    const why = skipReason(s);
    if (why) {
      log(`${s.id}: skipped, ${why}`);
      results.push({ id: s.id, skipped: why });
      continue;
    }
    const client = onClient || s.camera.client || s.client || 1;
    log(`${s.id}: ${s.camera.mode}, ${s.duration}s${s.scene ? `, scene ${s.scene}` : ""}`);
    if (s.scene && s.scene !== live) {
      cmd(client, "scene", "stop"); // also clears a scene left running by an earlier take
      const def = scenes.get(s.scene) || {};
      const r = cmd(client, "scene", `start ${s.scene} ${def.hour ?? "-"} ${def.weather ?? "-"}`);
      if (r.status !== "ok" || String(r.detail).startsWith("error")) throw new Error(`${s.id}: scene ${s.scene} did not start: ${r.status} ${r.detail}`);
      live = s.scene;
      await sleep(2500); // the clock and any actors need a moment to settle before a take
    }
    for (const spec of s.setup || []) {
      const r = io.send(client, spec, 15000);
      log(`  setup ${spec.verb} -> ${r.status} ${r.detail}`);
    }
    if (s.hud !== "on") cmd(client, "camera", "hud off");
    const name = `${b.name}_${s.id}`.replace(/[^A-Za-z0-9_]/g, "_"); // the recorder takes letters, digits and _
    // The client can still be flushing the last take's frames; a start that gets no answer is retried.
    let start = { ok: false };
    for (let attempt = 0; attempt < 3 && !start.ok; attempt++) {
      if (attempt) await sleep(4000);
      start = eng.engineStart((spec, t) => io.send(client, spec, t), name, { fps, seconds: Math.ceil(s.duration + LEAD_IN + 2) });
      if (!start.ok) log(`  film start: ${start.error}${attempt < 2 ? ", retrying" : ""}`);
    }
    if (!start.ok) throw new Error(`${s.id}: film start failed: ${start.error}`);
    await sleep(LEAD_IN * 1000);
    for (const args of cameraCommands(s)) {
      const r = cmd(client, "camera", args);
      if (r.status !== "ok" || String(r.detail).startsWith("error")) {
        eng.engineStop((spec, t) => io.send(client, spec, t), io.resolve, { label: name });
        throw new Error(`${s.id}: camera ${args.split(" ")[0]} failed: ${r.detail}`);
      }
    }
    await sleep((s.duration + 0.3) * 1000);
    if (s.camera.mode !== "firstPerson") cmd(client, "camera", "stop");
    const clip = eng.engineStop((spec, t) => io.send(client, spec, t), io.resolve, { out: takePath(b, s, dir), label: name, fps });
    log(`  ${eng.describe(clip)}`);
    if (!clip.ok) throw new Error(`${s.id}: ${clip.error}`);
    results.push({ id: s.id, out: clip.out });
  }
  const last = selected.find((s) => !skipReason(s));
  if (last) {
    const client = onClient || last.camera.client || last.client || 1;
    cmd(client, "camera", "hud on");
    if (live) cmd(client, "scene", "stop");
  }
  return results;
}

// The harness and engine, loaded only when a shoot needs them. Boxes are the session's client dirs.
export async function sessionIo() {
  const h = await import("../../agent-box/harness.mjs");
  const eng = await import("./engine.mjs");
  const sess = await import("../../agent-box/session.mjs");
  const dir = sess.sessionDir();
  const use = (n) => h.setBoxes([sess.clientBox(dir, n)]);
  return {
    eng,
    send: (n, spec, t) => {
      use(n);
      return h.sendCommand(spec, t);
    },
    resolve: (alias) => h.resolveCapture(alias),
    use,
  };
}

const flag = (argv, name) => {
  const i = argv.indexOf(name);
  return i === -1 ? null : argv[i + 1];
};

// film-make's front door for a storyboard with shots: validate, shoot what was asked for, return the post board.
export async function prepare(bp, argv, { log = console.log } = {}) {
  const board = JSON.parse(readFileSync(bp, "utf8"));
  validateStoryboard(board);
  const preview = argv.includes("--preview");
  const selected = selectShots(board, { shots: flag(argv, "--shot")?.split(","), group: flag(argv, "--group") });
  if (!argv.includes("--no-shoot")) {
    log(`film-make: shooting ${selected.length} shot(s) of ${board.name}${preview ? " (preview)" : ""}`);
    const results = await shoot(board, { selected, preview, log, io: await sessionIo(), onClient: flag(argv, "--client") ? Number(flag(argv, "--client")) : 0 });
    log(`film-make: shot ${results.filter((r) => r.out).length}, skipped ${results.filter((r) => r.skipped).length}`);
  }
  if (argv.includes("--shoot-only")) return null;
  const { post, warnings } = toPostBoard(board, { preview, only: argv.includes("--shot") || argv.includes("--group") ? new Set(selected.map((s) => s.id)) : undefined });
  warnings.forEach((w) => log(`film-make: ${w}`));
  if (!post.clips.length) throw new Error("no takes to post-produce; shoot first (a session is needed) or name other shots");
  return post;
}

// ---------------------------------------------------------------- selftest

export function selftestStoryboard() {
  const fails = [];
  const ok = (c, m) => c || fails.push(m);
  const good = () => ({
    format: FORMAT,
    name: "t",
    output: { size: [1920, 1080], fps: 30, aspect: "16:9" },
    scenes: [{ name: "trailer-square", hour: 17.9 }],
    shots: [
      { id: "A", scene: "trailer-square", duration: 4, camera: { mode: "orbit", point: [1, 0, 2], radius: 8, azimuth: 270, height: 7 } },
      { id: "B", scene: "trailer-square", duration: 3, camera: { mode: "drone", keys: [{ pos: [0, 3, 0], lookAt: [0, 1, 5] }, { pos: [0, 12, 0], lookAt: [20, 0, 5] }] } },
      { id: "C", scene: "trailer-square", duration: 3, camera: { mode: "static", position: [1, 2, 3], lookAt: [4, 5, 6], fov: 55 } },
      { id: "D", duration: 2, camera: { mode: "firstPerson", client: 2 } },
      { id: "E", duration: 2, camera: { mode: "static", pending: "position is prose" } },
      { id: "T", kind: "title", duration: 2, lines: ["LIFE FRAMEWORK"] },
    ],
  });
  const blame = (mutate, field) => {
    const b = good();
    mutate(b);
    try {
      validateStoryboard(b);
      fails.push(`accepted a bad storyboard (${field})`);
    } catch (e) {
      if (!e.message.includes(field)) fails.push(`error did not name ${field}: ${e.message}`);
    }
  };
  ok(validateStoryboard(good()), "good board validates");
  blame((b) => (b.format = "x"), "format");
  blame((b) => (b.output.size = [1921, 1080]), "output.size");
  blame((b) => (b.output.aspect = "9:16"), "output.aspect");
  blame((b) => (b.shots[1].id = "A"), "shots[1].id");
  blame((b) => (b.shots[0].duration = 0), "shots[0].duration");
  blame((b) => (b.shots[0].scene = "nowhere"), "shots[0].scene");
  blame((b) => (b.shots[0].camera.mode = "zoom"), "shots[0].camera.mode");
  blame((b) => delete b.shots[0].camera.radius, "shots[0].camera.radius");
  blame((b) => (b.shots[1].camera.keys = [b.shots[1].camera.keys[0]]), "shots[1].camera.keys");
  blame((b) => (b.shots[2].camera.lookAt = [1, 2]), "shots[2].camera.lookAt");
  blame((b) => delete b.shots[3].camera.client, "shots[3].camera.client");
  blame((b) => delete b.shots[5].lines, "shots[5].lines");
  const b = good();
  ok(cameraCommands(b.shots[0])[0] === "orbit 1 0 2 8 4 270 140 7", `orbit command: ${cameraCommands(b.shots[0])}`);
  ok(cameraCommands(b.shots[1])[0] === "drone 0 3 0 0 1 5 0 12 0 20 0 5 3 60", `drone command: ${cameraCommands(b.shots[1])}`);
  ok(cameraCommands(b.shots[2])[0] === "static 1 2 3 4 5 6 3 55", `static command: ${cameraCommands(b.shots[2])}`);
  ok(cameraCommands(b.shots[3]).length === 0, "first person has no camera command");
  ok(skipReason(b.shots[4]).startsWith("pending") && skipReason(b.shots[5]).startsWith("title") && skipReason(b.shots[0]) === "", "skip reasons");
  ok(selectShots(b, { shots: ["B", "C"] }).length === 2 && selectShots(b, { group: undefined }).length === 6, "select by id");
  const g = good();
  g.shots[0].group = "sq";
  g.shots[1].group = "sq";
  ok(selectShots(g, { group: "sq" }).map((s) => s.id).join() === "A,B", "select by group");
  try {
    selectShots(g, { shots: ["Z"] });
    fails.push("an unknown --shot was accepted");
  } catch (e) {
    ok(e.message.includes("Z"), "unknown shot named");
  }
  const { post, warnings } = toPostBoard(g, { have: (p) => /[AB]\.mp4$/.test(p), dir: "d" });
  ok(post.clips.length === 2 && post.clips[0].seconds === 4 && post.endCard && post.endCard.lines[0] === "LIFE FRAMEWORK" && !post.titleCard, "post board from takes");
  ok(warnings.length === 3, `warnings for the shots without a take: ${warnings}`);
  ok(toPostBoard(g, { preview: true, have: () => true }).post.size.join("x") === "960x540", "preview halves the size");
  return fails;
}
