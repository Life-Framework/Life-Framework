// tools\cli blender: drive the user's live Blender (the LF Bridge extension) or a headless one.
//
//   blender status                                     which Blenders, the bridge, the extension
//   blender install [--blender <exe>]                  build tools/blender/lf_bridge and install it (Blender 5.3 by default)
//   blender run <file.py> [--live|--headless] [--needs archbuilder] [--blender <exe>] [--no-factory] [--discard] [-- args]
//   blender exec "<python>" [--live|--headless] ...     the same for one snippet; `bpy` is imported
//   blender show <model.fbx|.glb|.gltf|.obj|.blend | asset slug | addons/...fbx> [--clear] [--colliders] [--shot out.png]   put it in the open Blender, framed
//   blender shot <out.png> [--view front|iso|top|...] [--camera <name>] [--size WxH] [--target a,b] [--file x.blend] [--live|--headless]
//
// run/exec/shot use the live bridge when it answers and a headless Blender otherwise. Setup, the
// security model and the protocol: docs/blender-bridge.md. The code is in tools/lib/blender.mjs.
import { existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { addonDir, resolveAddon, CORE } from "../lib/addons.mjs";
import { KNOWN_INSTALLS, bridge, bridgePort, blenderCommand, blenderExe, needMetBy, runHeadless, runLive, tokenFile, userExtensionsDir, versionOf } from "../lib/blender.mjs";

export const help = "blender status|install|run <file.py>|exec \"<code>\"|shot <out.png>   drive the live Blender (LF Bridge extension) or a headless one";

const HERE = dirname(fileURLToPath(import.meta.url));
const BRIDGE_DIR = join(HERE, "..", "blender", "lf_bridge");
const SHOW_SCRIPT = join(HERE, "..", "blender", "lf_show.py");
const SHOT_SCRIPT = join(HERE, "..", "blender", "lf_shot.py");
const USAGE = `usage: tools\\cli blender status
       tools\\cli blender install [--blender <exe>]
       tools\\cli blender run <file.py> [--live|--headless] [--needs archbuilder] [--blender <exe>] [--no-factory] [--discard] [-- args]
       tools\\cli blender exec "<python>" [--live|--headless]
       tools\\cli blender show <model file | asset slug | addons/...fbx> [--clear] [--colliders] [--shot out.png] [--view iso]
       tools\\cli blender shot <out.png> [--view front|back|left|right|top|bottom|iso] [--camera <name>] [--size WxH] [--target a,b] [--file x.blend] [--live|--headless]`;

const VALUED = ["--needs", "--blender", "--view", "--camera", "--size", "--target", "--file", "--shot"];

// { pos: [...], flags: Set, val: {--x: v}, rest: [after "--"] }
export function parseArgs(args) {
  const cut = args.indexOf("--");
  const head = cut >= 0 ? args.slice(0, cut) : args;
  const rest = cut >= 0 ? args.slice(cut + 1) : [];
  const out = { pos: [], flags: new Set(), val: {}, rest };
  for (let i = 0; i < head.length; i++) {
    const a = head[i];
    if (VALUED.includes(a)) out.val[a] = head[++i];
    else if (a.startsWith("--")) out.flags.add(a);
    else out.pos.push(a);
  }
  return out;
}

// The last lines of Blender's output that name a failure (a user's other add-ons print pages of noise first).
const failureLines = (text) => {
  const lines = text.trim().split("\n");
  const hits = lines.filter((l) => /FATAL|Error:|error:|failed|invalid|unknown/i.test(l) && !/EnfusionBlenderTools|CRITICAL/.test(l));
  return (hits.length ? hits : lines).slice(-8).join("\n");
};
const kb = (n) => `${Math.max(1, Math.round(n / 1024))} KB`;
const secs = (t0) => `${((Date.now() - t0) / 1000).toFixed(1)}s`;

// "live" | "headless", and the bridge's ping when live. A wrong token is an error, never a silent fallback.
async function chooseMode(p) {
  if (p.flags.has("--live") && p.flags.has("--headless")) throw new Error("--live and --headless exclude each other");
  if (p.flags.has("--headless")) return { mode: "headless" };
  const st = await bridge.status();
  if (st.state === "listening") return { mode: "live", info: st.info };
  if (p.flags.has("--live")) throw new Error(`no live Blender: ${st.detail || st.state}. Enable the LF Bridge extension (tools\\cli blender install) and check its sidebar panel says "listening".`);
  if (st.state === "auth") throw new Error(st.detail);
  return { mode: "headless", why: st.state };
}

function headlessOpts(p) {
  return { needs: p.val["--needs"], exe: p.val["--blender"], factory: !p.flags.has("--no-factory") };
}

async function status(root) {
  const lines = [];
  for (const exe of KNOWN_INSTALLS) {
    const dir = userExtensionsDir(exe);
    const by = needMetBy("archbuilder", exe);
    const arch = by ? `, ArchBuilder (${by})` : "";
    const ext = dir && existsSync(join(dir, "lf_bridge")) ? ", lf_bridge installed" : "";
    lines.push(`  Blender ${versionOf(exe)}: ${existsSync(exe) ? "present" : "MISSING"} at ${exe}${arch}${ext}`);
  }
  const st = await bridge.status();
  const detail = st.state === "listening" ? `listening on 127.0.0.1:${bridgePort()} (Blender ${st.info.blender}, file ${st.info.file || "untitled"}${st.info.dirty ? ", unsaved changes" : ""})` : `${st.state}${st.detail ? `: ${st.detail}` : ""}`;
  console.log(`blender status: default ${blenderExe()}; bridge ${detail}`);
  for (const l of lines) console.log(l);
  console.log(`  token file ${tokenFile()} ${existsSync(tokenFile()) ? "present" : "not created yet (enable the extension once)"}`);
  return 0;
}

async function install(root, p) {
  const exe = p.val["--blender"] || blenderExe({ version: "5.3" });
  if (!existsSync(exe)) throw new Error(`Blender not found at ${exe}; pass --blender <exe> or set LF_BLENDER`);
  const manifest = readFileSync(join(BRIDGE_DIR, "blender_manifest.toml"), "utf8");
  const version = /^version\s*=\s*"([^"]+)"/m.exec(manifest)?.[1] || "0.0.0";
  const tmp = join(root, "tmp", "blender-bridge");
  mkdirSync(tmp, { recursive: true });
  const zip = join(tmp, `lf_bridge-${version}.zip`);
  const build = blenderCommand(exe, ["extension", "build", "--source-dir", BRIDGE_DIR, "--output-filepath", zip]);
  if (build.status !== 0 || !existsSync(zip)) throw new Error(`extension build failed (exit ${build.status})\n${failureLines(build.stdout + build.stderr)}`);
  const inst = blenderCommand(exe, ["extension", "install-file", "-r", "user_default", "-e", zip]);
  const text = (inst.stdout + inst.stderr).trim();
  // Blender prints other add-ons' load errors on the way; the install is judged by its exit code and by the files landing.
  const landed = existsSync(join(userExtensionsDir(exe), "lf_bridge", "blender_manifest.toml"));
  if (inst.status !== 0 || !landed) throw new Error(`extension install-file failed (exit ${inst.status}, files ${landed ? "present" : "missing"})\n${failureLines(text)}`);
  console.log(`blender install: lf_bridge ${version} installed and enabled in Blender ${versionOf(exe)} (user_default); a running Blender needs a restart, or Edit > Preferences > Get Extensions > Installed`);
  for (const l of text.split("\n").filter((x) => x.includes("STATUS"))) console.log(l.trim());
  return 0;
}

async function run(root, p) {
  const [file] = p.pos;
  if (!file) { console.log(USAGE); return 2; }
  const script = resolve(file);
  if (!existsSync(script)) throw new Error(`no such script: ${script}`);
  const t0 = Date.now();
  const m = await chooseMode(p);
  const r = m.mode === "live"
    ? await runLive(script, p.rest, { discard: p.flags.has("--discard") })
    : runHeadless(script, p.rest, headlessOpts(p));
  console.log(`blender run: ${file} ok (${m.mode === "live" ? `live, Blender ${m.info.blender}` : `headless${m.why ? `, bridge ${m.why}` : ""}, ${versionOf(r.exe) || "Blender"}`}, ${secs(t0)})`);
  if (r.stdout.trim()) console.log(r.stdout.trimEnd());
  return 0;
}

async function exec(root, p) {
  const code = p.pos.join(" ");
  if (!code) { console.log(USAGE); return 2; }
  const t0 = Date.now();
  const m = await chooseMode(p);
  if (m.mode === "live") {
    const r = await bridge.exec(code);
    if (!r.ok) throw new Error(`exec failed in the live Blender\n${r.error}${r.stdout ? `\n${r.stdout.trimEnd()}` : ""}`);
    console.log(`blender exec: ok (live, Blender ${m.info.blender}, ${secs(t0)})`);
    if (r.stdout.trim()) console.log(r.stdout.trimEnd());
    if (r.result !== null && r.result !== undefined) console.log(`result: ${JSON.stringify(r.result)}`);
    return 0;
  }
  const dir = join(root, "tmp", "blender");
  mkdirSync(dir, { recursive: true });
  const file = join(dir, `exec-${process.pid}.py`);
  writeFileSync(file, `import bpy\n${code}\n`);
  const r = runHeadless(file, [], headlessOpts(p));
  console.log(`blender exec: ok (headless${m.why ? `, bridge ${m.why}` : ""}, ${versionOf(r.exe) || "Blender"}, ${secs(t0)})`);
  if (r.stdout.trim()) console.log(r.stdout.trimEnd());
  return 0;
}

async function shot(root, p) {
  const [outArg] = p.pos;
  if (!outArg) { console.log(USAGE); return 2; }
  const out = resolve(outArg);
  const size = p.val["--size"] ? p.val["--size"].split("x").map(Number) : undefined;
  if (size && (size.length !== 2 || !size.every((n) => n >= 16 && n <= 8192))) throw new Error("--size is WIDTHxHEIGHT, e.g. 1280x720");
  const target = p.val["--target"] ? p.val["--target"].split(",").filter(Boolean) : undefined;
  const view = p.val["--view"];
  const camera = p.val["--camera"];
  const t0 = Date.now();
  const m = p.val["--file"] && !p.flags.has("--live") ? { mode: "headless" } : await chooseMode(p);
  if (m.mode === "live") {
    const r = await bridge.shot(out, { view, camera, size, target });
    if (!r.ok) throw new Error(`shot failed in the live Blender: ${r.error}`);
    console.log(`blender shot: ${r.out} (live Blender ${m.info.blender}, ${r.view}, ${r.size.join("x")}, ${kb(r.bytes)}, ${secs(t0)})`);
    return 0;
  }
  const blend = p.val["--file"];
  if (!blend) throw new Error("a headless shot renders a saved scene: pass --file <x.blend> (or start Blender with the bridge for the live viewport)");
  mkdirSync(join(root, "tmp", "blender"), { recursive: true });
  const job = join(root, "tmp", "blender", `shot-${process.pid}.json`);
  writeFileSync(job, JSON.stringify([{ out, view: view || "iso", camera, size, target }]));
  const r = runHeadless(SHOT_SCRIPT, [job], { ...headlessOpts(p), blend: resolve(blend) });
  console.log(`blender shot: ${out} (headless, ${view || "iso"}, ${kb(existsSync(out) ? readFileSync(out).length : 0)}, ${secs(t0)})`);
  if (!existsSync(out)) throw new Error(`the headless render wrote no ${out}\n${r.stdout.trim().split("\n").slice(-6).join("\n")}`);
  return 0;
}

const SHOWABLE = [".fbx", ".glb", ".gltf", ".obj", ".blend"];

// The newest file with one of these extensions directly in `dir`, or null.
function newestIn(dir, exts) {
  if (!existsSync(dir)) return null;
  const files = readdirSync(dir).filter((f) => exts.includes(f.slice(f.lastIndexOf(".")).toLowerCase())).map((f) => join(dir, f));
  files.sort((a, b) => statSync(b).mtimeMs - statSync(a).mtimeMs);
  return files[0] || null;
}

// Every .fbx under dir (the addon's source models for one asset).
function fbxUnder(dir) {
  if (!existsSync(dir)) return [];
  const out = [];
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const full = join(dir, e.name);
    if (e.isDirectory()) out.push(...fbxUnder(full));
    else if (/\.fbx$/i.test(e.name)) out.push(full);
  }
  return out;
}

// What `show` opens for a target: { file, why } or throws. Pure over the file system, exported for the selftest.
export function resolveShowTarget(root, target) {
  const direct = [resolve(target), resolve(root, target)].find((p) => existsSync(p) && statSync(p).isFile());
  if (direct) {
    const ext = direct.slice(direct.lastIndexOf(".")).toLowerCase();
    if (ext === ".xob") throw new Error(`${target} is a compiled .xob; Blender cannot open those. Show the source model instead: the .fbx beside it (tools\\cli blender show <that .fbx>), or the asset's slug`);
    if (!SHOWABLE.includes(ext)) throw new Error(`cannot show a ${ext || "extensionless"} file; Blender takes ${SHOWABLE.join(" ")}`);
    return { file: direct, why: "file" };
  }
  const specFile = join(root, "tools", "assets", target, "spec.json");
  if (/^[a-z][a-z0-9-]*$/.test(target) && existsSync(specFile)) {
    const spec = JSON.parse(readFileSync(specFile, "utf8"));
    const cache = join(root, "tmp", "assets", target);
    const clean = newestIn(join(cache, "clean"), [".fbx"]);
    if (clean) return { file: clean, why: `asset ${target}: newest cleaned model` };
    const raw = ["raw-remesh", "raw"].map((d) => join(cache, d, "model.glb")).find((p) => existsSync(p));
    if (raw) return { file: raw, why: `asset ${target}: raw generated model (the clean stage has not run)` };
    const addon = resolveAddon(spec.addon || CORE);
    const sources = addon && spec.asset_dir ? fbxUnder(join(addonDir(root, addon.name), spec.asset_dir)) : [];
    sources.sort((a, b) => statSync(b).mtimeMs - statSync(a).mtimeMs);
    if (sources[0]) return { file: sources[0], why: `asset ${target}: the addon's source model` };
    throw new Error(`asset ${target} has no model to show: nothing in tmp/assets/${target}/, and no .fbx under ${spec.asset_dir || "its asset_dir"} in the addon. Run: tools\\cli asset run ${target} --only clean`);
  }
  throw new Error(`"${target}" is neither a model file (${SHOWABLE.join(" ")}) nor an asset slug (tools/assets/<slug>/spec.json)`);
}

async function show(root, p) {
  const [target] = p.pos;
  const clear = p.flags.has("--clear");
  if (!target && !clear) { console.log(USAGE); return 2; }
  const st = await bridge.status();
  if (st.state !== "listening") throw new Error(`open Blender 5.3 and turn on LF Bridge (${st.detail || st.state}). Install once with: tools\\cli blender install; the sidebar panel LF Bridge must say listening.`);
  const resolved = target ? resolveShowTarget(root, target) : null;
  const t0 = Date.now();
  const r = await bridge.exec(readFileSync(SHOW_SCRIPT, "utf8"), { args: [JSON.stringify({ path: resolved ? resolved.file : null, clear, colliders: p.flags.has("--colliders") })], file: SHOW_SCRIPT, timeout: 300 });
  if (!r.ok) throw new Error(`show failed in the live Blender\n${r.error}${r.stderr ? `\n${r.stderr.trim()}` : ""}`);
  const res = r.result;
  if (!resolved) {
    console.log(`blender show: cleared LF_Show (${res.removed} object(s) removed)`);
    return 0;
  }
  const dims = res.dims_m ? res.dims_m.map((v) => v.toFixed(3)).join(" x ") : "no geometry";
  console.log(`blender show: ${res.loaded} (${resolved.why}) in LF_Show: ${res.objects.length} object(s), ${res.meshes} mesh(es), ${res.tris} tris, ${dims} m, framed in ${res.framed_views} view(s)${res.colliders_hidden ? `, ${res.colliders_hidden} collider(s) hidden (--colliders shows them)` : ""}${clear ? `, ${res.removed} earlier object(s) cleared` : ""}${!clear && res.collection_total > res.objects.length + (res.colliders_hidden || 0) ? `; LF_Show holds ${res.collection_total} (--clear replaces them)` : ""}`);
  console.log(`  ${resolved.file}`);
  const shotOut = p.val["--shot"];
  if (shotOut) {
    if (!res.objects.length) throw new Error("--shot: nothing was loaded to take a picture of");
    const s = await bridge.shot(resolve(shotOut), { view: p.val["--view"] || "iso", target: res.objects, size: p.val["--size"] ? p.val["--size"].split("x").map(Number) : undefined });
    if (!s.ok) throw new Error(`shot failed: ${s.error}`);
    console.log(`  picture ${s.out} (${s.view}, ${kb(s.bytes)}, ${secs(t0)})`);
  }
  return 0;
}

export async function main(args, { root } = {}) {
  const [sub, ...rest] = args;
  const p = parseArgs(rest);
  if (sub === "status") return status(root);
  if (sub === "install") return install(root, p);
  if (sub === "run") return run(root, p);
  if (sub === "exec") return exec(root, p);
  if (sub === "shot") return shot(root, p);
  if (sub === "show") return show(root, p);
  console.log(USAGE);
  return sub ? 2 : 0;
}
