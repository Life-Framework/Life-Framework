// Local, unlimited concept images: FLUX.1 [schnell] (GGUF Q4_K_S) on stable-diffusion.cpp, CUDA build.
// Nothing here is paid and nothing lives in the repo: the runtime and the weights are fetched into
// %LOCALAPPDATA%\LifeFramework (LF_LOCAL_HOME overrides). README.md in this folder has the details.
//   node tools/concepts-local/local.mjs fetch | status | gen -p "..." -o out.png [-i ref.png --strength 0.7]
import { createHash } from "node:crypto";
import { spawn, spawnSync } from "node:child_process";
import { createReadStream, createWriteStream, existsSync, mkdirSync, readFileSync, renameSync, rmSync, statSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { homedir } from "node:os";

export const here = dirname(fileURLToPath(import.meta.url));
export const manifest = JSON.parse(readFileSync(join(here, "models.json"), "utf8"));
export const home = () => process.env.LF_LOCAL_HOME || join(process.env.LOCALAPPDATA || join(homedir(), "AppData", "Local"), "LifeFramework");
export const modelDir = () => join(home(), "models", "flux1-schnell");
export const toolDir = () => join(home(), "tools", "sd-cpp");
export const sdCli = () => join(toolDir(), "sd-cli.exe");
export const lockFile = () => join(home(), "concepts-local.lock");

// Free VRAM the run needs before it starts, MiB. The measured peak is in README.md; the margin is
// for a game session that grows while we run.
export const NEED_MIB = Number(process.env.LF_LOCAL_NEED_MIB || 7600);
export const ABORT_FREE_MIB = 200; // free VRAM below this while generating: stop, the game needs it

export function gpu() {
  const r = spawnSync("nvidia-smi", ["--query-gpu=memory.used,memory.total", "--format=csv,noheader,nounits"], { encoding: "utf8", windowsHide: true, timeout: 15000 });
  if (r.status !== 0) return null;
  const [used, total] = r.stdout.trim().split("\n")[0].split(",").map((x) => Number(x));
  return Number.isFinite(used) ? { used, total, free: total - used } : null;
}

// Pure decision, so the selftest covers it: may a run start with this much free VRAM?
export function guardDecision(g, need = NEED_MIB) {
  if (!g) return { ok: false, reason: "nvidia-smi gave no GPU reading; refusing rather than risk the game" };
  if (g.free < need) return { ok: false, reason: `only ${g.free} MiB VRAM free of ${g.total} (${g.used} used, probably a game session or Workbench); need ${need}` };
  return { ok: true, reason: `${g.free} MiB VRAM free` };
}

export async function waitForGpu(waitS = 0, need = NEED_MIB, log = console.log) {
  const end = Date.now() + waitS * 1000;
  let d = guardDecision(gpu(), need);
  while (!d.ok && Date.now() < end) {
    log(`  GPU busy: ${d.reason}; waiting`);
    await new Promise((r) => setTimeout(r, 15000));
    d = guardDecision(gpu(), need);
  }
  return d;
}

// ---- fetch ---------------------------------------------------------------------------------------
const sha256 = (f) => new Promise((res, rej) => { const h = createHash("sha256"); createReadStream(f).on("data", (d) => h.update(d)).on("end", () => res(h.digest("hex"))).on("error", rej); });

async function download(url, dest, bytes, log) {
  const part = dest + ".part";
  let have = existsSync(part) ? statSync(part).size : 0;
  if (have > bytes) { rmSync(part); have = 0; }
  if (have < bytes) {
    const r = await fetch(url, { headers: have ? { Range: `bytes=${have}-` } : {}, redirect: "follow" });
    if (!(r.status === 200 || r.status === 206)) throw new Error(`${url}: HTTP ${r.status}`);
    const out = createWriteStream(part, { flags: have && r.status === 206 ? "a" : "w" });
    let n = 0, last = Date.now();
    for await (const chunk of r.body) {
      if (!out.write(chunk)) await new Promise((x) => out.once("drain", x));
      n += chunk.length;
      if (Date.now() - last > 30000) { last = Date.now(); log(`  ${Math.round(((have + n) / bytes) * 100)}%`); }
    }
    await new Promise((x) => out.end(x));
  }
  if (statSync(part).size !== bytes) throw new Error(`${dest}: got ${statSync(part).size} bytes, expected ${bytes}`);
  renameSync(part, dest);
}

export async function fetchAll(log = console.log) {
  mkdirSync(modelDir(), { recursive: true });
  mkdirSync(toolDir(), { recursive: true });
  const rt = manifest.runtime;
  if (!existsSync(sdCli())) {
    for (const a of rt.assets) {
      const z = join(toolDir(), a.name);
      if (!existsSync(z)) { log(`fetch ${a.name}`); await download(`https://github.com/${rt.repo}/releases/download/${rt.release}/${a.name}`, z, a.bytes, log); }
      if ((await sha256(z)) !== a.sha256) { rmSync(z); throw new Error(`${a.name}: sha256 mismatch, deleted`); }
      const r = spawnSync("tar", ["-xf", z, "-C", toolDir()], { windowsHide: true, encoding: "utf8" });
      if (r.status !== 0) throw new Error(`unzip ${a.name} failed: ${r.stderr}`);
    }
  }
  for (const f of manifest.files) {
    const dest = join(modelDir(), f.name);
    if (existsSync(dest) && statSync(dest).size === f.bytes) continue;
    log(`fetch ${f.name} (${(f.bytes / 1e9).toFixed(2)} GB)`);
    await download(f.url, dest, f.bytes, log);
    if (f.sha256 && (await sha256(dest)) !== f.sha256) { rmSync(dest); throw new Error(`${f.name}: sha256 mismatch, deleted`); }
  }
  log("local concept generator ready");
}

export function status() {
  const missing = [];
  if (!existsSync(sdCli())) missing.push("sd-cli.exe");
  for (const f of manifest.files) if (!existsSync(join(modelDir(), f.name)) || statSync(join(modelDir(), f.name)).size !== f.bytes) missing.push(f.name);
  return { ready: !missing.length, missing, modelDir: modelDir(), toolDir: toolDir(), gpu: gpu() };
}

// ---- generate ------------------------------------------------------------------------------------
export function buildArgs({ prompt, out, size = 1024, seed = 42, steps = 4, init = null, strength = 0.7, dir = modelDir() }) {
  const a = ["--diffusion-model", join(dir, manifest.files[0].name), "--vae", join(dir, "ae.safetensors"), "--clip_l", join(dir, "clip_l.safetensors"), "--t5xxl", join(dir, "t5-v1_1-xxl-encoder-Q5_K_M.gguf"),
    "-p", prompt, "--cfg-scale", "1.0", "--sampling-method", "euler", "--steps", String(steps), "-W", String(size), "-H", String(size), "-s", String(seed),
    "--offload-to-cpu", "--vae-tiling", "--flash-attn", "on", "-o", out, "--log-level", "warn"];
  if (init) a.push("-i", init, "--strength", String(strength));
  return a;
}

// One picture. Returns { seconds, peakMiB }. Never leaves the process running: a hard timeout and the
// free-VRAM watchdog both kill it.
export async function generate(opts, { waitS = 0, timeoutS = 900, log = console.log } = {}) {
  const st = status();
  if (!st.ready) throw new Error(`local generator not installed (missing ${st.missing.join(", ")}): node tools/concepts-local/local.mjs fetch`);
  const d = await waitForGpu(waitS, NEED_MIB, log);
  if (!d.ok) { const e = new Error(`GPU is not free: ${d.reason}. Not starting; rerun when the game session is closed, or pass --wait <seconds>.`); e.code = "GPU_BUSY"; throw e; }
  mkdirSync(dirname(resolve(opts.out)), { recursive: true });
  if (existsSync(lockFile()) && Date.now() - statSync(lockFile()).mtimeMs < (timeoutS + 60) * 1000) throw Object.assign(new Error("another local concept run holds the lock (" + lockFile() + ")"), { code: "GPU_BUSY" });
  writeFileSync(lockFile(), String(process.pid));
  const t0 = Date.now();
  let peak = 0, aborted = "";
  try {
    const child = spawn(sdCli(), buildArgs(opts), { windowsHide: true, stdio: ["ignore", "pipe", "pipe"], cwd: toolDir() });
    let err = "";
    child.stdout.on("data", () => {}); child.stderr.on("data", (c) => { err = (err + c).slice(-2000); });
    const base = gpu()?.used ?? 0;
    const mon = setInterval(() => {
      const g = gpu(); if (!g) return;
      peak = Math.max(peak, g.used);
      if (g.free < ABORT_FREE_MIB) { aborted = `free VRAM fell to ${g.free} MiB (another program needs the GPU)`; child.kill(); }
    }, 1000);
    const kill = setTimeout(() => { aborted = `timeout after ${timeoutS}s`; child.kill(); }, timeoutS * 1000);
    const code = await new Promise((r) => child.on("close", r));
    clearInterval(mon); clearTimeout(kill);
    if (aborted) throw new Error(`local image generation stopped: ${aborted}`);
    if (code !== 0 || !existsSync(opts.out)) throw new Error(`sd-cli exit ${code}: ${err.trim().slice(-600)}`);
    return { seconds: Math.round((Date.now() - t0) / 100) / 10, peakMiB: Math.max(0, peak - base), peakUsedMiB: peak };
  } finally { rmSync(lockFile(), { force: true }); }
}

// ---- CLI -----------------------------------------------------------------------------------------
async function main(argv) {
  const [cmd, ...rest] = argv;
  const flag = (n, d) => { const i = rest.indexOf(n); return i >= 0 ? rest[i + 1] : d; };
  if (cmd === "fetch") { await fetchAll(); return 0; }
  if (cmd === "status") { console.log(JSON.stringify(status(), null, 2)); return status().ready ? 0 : 1; }
  if (cmd === "gen") {
    const r = await generate({ prompt: flag("-p"), out: flag("-o", "out.png"), init: flag("-i", null), strength: Number(flag("--strength", 0.7)), seed: Number(flag("-s", 42)), size: Number(flag("--size", 1024)) }, { waitS: Number(flag("--wait", 0)) });
    console.log(JSON.stringify(r));
    return 0;
  }
  console.log("usage: node tools/concepts-local/local.mjs fetch | status | gen -p <prompt> -o <png> [-i <ref.png> --strength 0.7] [--size 1024] [-s seed] [--wait <s>]");
  return 2;
}
if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main(process.argv.slice(2)).then((c) => process.exit(c), (e) => { console.error(e.message); process.exit(e.code === "GPU_BUSY" ? 3 : 1); });
