// tools/lib/blender.mjs - find Blender, run a script in it, and talk to a live Blender.
//
//   blenderExe({ needs, version })             the Blender to use (LF_BLENDER overrides everything)
//   runHeadless(script, args, opts)            blender --background --python script -- args   (sync)
//   runBlender                                 alias of runHeadless (the asset pipeline's old name)
//   runLive(script, args, opts)                the same script in the user's open Blender, over the bridge (async)
//   bridge.ping() / .exec(code, opts) / .shot(out, opts) / .status()    the client of tools/blender/lf_bridge
//
// The bridge is a Blender extension (tools/blender/lf_bridge): localhost only, token required,
// switchable from Blender's sidebar. docs/blender-bridge.md has the security model.
import net from "node:net";
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join, resolve } from "node:path";

// ---------------------------------------------------------------- locating Blender

// The documented install paths on the maintainer's PC, in default-preference order: 4.5 LTS first
// (the asset pipeline's clean stage is proven on it), then 5.3 (where the bridge and ArchBuilder live).
export const KNOWN_INSTALLS = [
  "C:/Users/jaspe/Tools/blender-4.5.9-windows-x64/blender.exe",
  "C:/Program Files/Blender Foundation/Blender 5.3/blender.exe",
];

// What each `needs` requires of an install. ArchBuilder is the user's add-on; this code only checks
// it is there. Blender keeps custom extension repositories in userpref.blend, which is not readable
// from here, so a need is met by an `arch_builder` folder in either place:
//   - any repository folder of that Blender version's user folder: <APPDATA>/Blender Foundation/
//     Blender/<ver>/extensions/*/arch_builder (user_default and any other repo kept there);
//   - the local repository directory `lf_local` (module bl_ext.lf_local.arch_builder), named by the
//     env var in `dirEnv`, defaulting to `defaultDir`. It is shared by every Blender version.
export const NEEDS = {
  archbuilder: { folder: "arch_builder", dirEnv: "LF_ARCHBUILDER_DIR", defaultDir: "C:/Users/jaspe/Documents/Reforger/blender-addons" },
};

// "5.3" from ".../Blender 5.3/blender.exe" or ".../blender-4.5.9-windows-x64/blender.exe".
export function versionOf(exe) {
  const m = /blender[ -](\d+\.\d+)/i.exec(String(exe).replace(/\\/g, "/"));
  return m ? m[1] : null;
}

export function userExtensionsDir(exe, env = process.env) {
  const v = versionOf(exe);
  if (!v) return null;
  const appdata = env.APPDATA || join(homedir(), "AppData", "Roaming");
  return join(appdata, "Blender Foundation", "Blender", v, "extensions", "user_default");
}

// The `extensions` folder of an install's version in the user's profile (holds one folder per repository).
export function userRepoRoot(exe, env = process.env) {
  const v = versionOf(exe);
  if (!v) return null;
  const appdata = env.APPDATA || join(homedir(), "AppData", "Roaming");
  return join(appdata, "Blender Foundation", "Blender", v, "extensions");
}

// Where a need is met for one install: "user extensions" (a repo under its version's user folder),
// "lf_local repo" (the shared local repository directory), or null.
export function needMetBy(need, exe, { env = process.env, exists = existsSync, list = (d) => readdirSync(d) } = {}) {
  const spec = NEEDS[need];
  const repos = userRepoRoot(exe, env);
  if (repos && exists(repos)) {
    let names = [];
    try { names = list(repos); } catch { /* unreadable: treated as empty */ }
    if (names.some((n) => exists(join(repos, n, spec.folder)))) return "user extensions";
  }
  const dir = env[spec.dirEnv] || spec.defaultDir;
  if (dir && exists(join(dir, spec.folder))) return "lf_local repo";
  return null;
}

// Pure locator: { exe, source } or throws. `exists`, `list` and `candidates` are injectable for the selftest.
// For a need, the newest Blender wins (the user works in 5.3), one with the add-on in its own user
// folder ahead of one that only sees the shared local repository.
export function locate({ needs, version, env = process.env, exists = existsSync, list: readdir, candidates = KNOWN_INSTALLS } = {}) {
  if (env.LF_BLENDER) return { exe: env.LF_BLENDER, source: "LF_BLENDER" };
  let found = candidates.filter((c) => exists(c));
  if (needs) {
    if (!NEEDS[needs]) throw new Error(`blender: unknown need "${needs}" (known: ${Object.keys(NEEDS).join(", ")})`);
    const met = found.map((c) => ({ c, by: needMetBy(needs, c, { env, exists, list: readdir }) })).filter((x) => x.by);
    if (!met.length) {
      const spec = NEEDS[needs];
      throw new Error(`no installed Blender has ${spec.folder}: not in <APPDATA>/Blender Foundation/Blender/<version>/extensions/*/ and not in ${env[spec.dirEnv] || spec.defaultDir} (set ${spec.dirEnv} for another local repository directory, or LF_BLENDER to choose the Blender)`);
    }
    const newest = (a, b) => parseFloat(versionOf(b.c)) - parseFloat(versionOf(a.c));
    const own = met.filter((x) => x.by === "user extensions").sort(newest);
    const shared = met.filter((x) => x.by !== "user extensions").sort(newest);
    const best = [...own, ...shared];
    const pick = version ? best.filter((x) => versionOf(x.c) === String(version)) : best;
    if (pick.length) return { exe: pick[0].c, source: `known install (${pick[0].by})` };
    found = [];
  }
  if (version) found = found.filter((c) => versionOf(c) === String(version));
  if (!found.length) {
    // Nothing installed: hand back the default so a caller's existence check prints a useful path.
    return { exe: version ? candidates.find((c) => versionOf(c) === String(version)) || candidates[0] : candidates[0], source: "default", missing: true };
  }
  return { exe: found[0], source: "known install" };
}

export function blenderExe(opts = {}) {
  return locate(opts).exe;
}

// ---------------------------------------------------------------- headless

// Run a bpy script headless. Throws on a non-zero exit. Returns { status, stdout, stderr, exe }.
// opts: timeoutMs, log (a file to write the output to), needs/version (see blenderExe), exe,
// factory (default true: --factory-startup, so no user add-ons; pass false for ArchBuilder),
// blend (a .blend to open first).
export function runHeadless(script, args = [], { timeoutMs = 600000, log, needs, version, exe: given, factory = true, blend } = {}) {
  const exe = given || blenderExe({ needs, version });
  if (!existsSync(exe)) throw new Error(`Blender not found at ${exe}; set LF_BLENDER (docs/asset-pipeline/README.md "Setup")`);
  const argv = ["--background", ...(factory ? ["--factory-startup"] : []), ...(blend ? [blend] : []), "--python-exit-code", "3", "--python", script, "--", ...args];
  const res = spawnSync(exe, argv, { encoding: "utf8", timeout: timeoutMs, windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
  if (log) writeFileSync(log, (res.stdout || "") + "\n" + (res.stderr || ""));
  if (res.error) throw new Error(`Blender ${script}: ${res.error.message}`);
  if (res.status !== 0) {
    const tail = ((res.stdout || "") + (res.stderr || "")).trim().split("\n").slice(-12).join("\n");
    throw new Error(`Blender ${script} exited ${res.status}${log ? ` (log: ${log})` : ""}\n${tail}`);
  }
  return { status: res.status, stdout: res.stdout || "", stderr: res.stderr || "", exe };
}
export const runBlender = runHeadless;

// `blender --command <args>` (extension build/install and the like). Returns { status, stdout, stderr }.
export function blenderCommand(exe, args, { timeoutMs = 180000 } = {}) {
  const res = spawnSync(exe, ["--command", ...args], { encoding: "utf8", timeout: timeoutMs, windowsHide: true, maxBuffer: 16 * 1024 * 1024 });
  if (res.error) throw new Error(`Blender --command ${args[0]}: ${res.error.message}`);
  return { status: res.status, stdout: res.stdout || "", stderr: res.stderr || "" };
}

// ---------------------------------------------------------------- the bridge client

export const BRIDGE_HOST = "127.0.0.1";
export const DEFAULT_PORT = 9876;
export const MAX_LINE = 8 * 1024 * 1024;
const DEFAULT_TIMEOUT_S = { ping: 5, exec: 60, shot: 120 };

export const bridgePort = (env = process.env) => Number(env.LF_BRIDGE_PORT) || DEFAULT_PORT;
export const tokenFile = (env = process.env) => env.LF_BRIDGE_TOKEN_FILE || join(homedir(), ".life-framework", "blender-bridge-token");

// code: notoken (no token file yet), down (nothing listening), auth (wrong token), timeout, protocol.
export class BridgeError extends Error {
  constructor(message, code) {
    super(message);
    this.name = "BridgeError";
    this.code = code;
  }
}

export function readToken(env = process.env) {
  try {
    return readFileSync(tokenFile(env), "utf8").trim() || null;
  } catch {
    return null;
  }
}

// One request as one JSON line.
export function encodeRequest(op, payload, token, id = 1) {
  return JSON.stringify({ ...payload, id, op, token }) + "\n";
}

// Splits a byte stream into lines; throws when one grows past MAX_LINE.
export class LineReader {
  constructor(max = MAX_LINE) {
    this.buf = "";
    this.max = max;
  }
  push(chunk) {
    this.buf += chunk;
    const lines = [];
    let i;
    while ((i = this.buf.indexOf("\n")) >= 0) {
      lines.push(this.buf.slice(0, i));
      this.buf = this.buf.slice(i + 1);
    }
    if (this.buf.length > this.max) throw new BridgeError("response line too long", "protocol");
    return lines;
  }
}

// One response line to the response object; an unauthorized or malformed answer throws.
export function parseResponse(line) {
  let res;
  try {
    res = JSON.parse(line);
  } catch {
    throw new BridgeError("the bridge sent something that is not JSON", "protocol");
  }
  if (res?.error === "unauthorized") throw new BridgeError(`the bridge refused the token (${tokenFile()} does not match the running Blender's)`, "auth");
  if (typeof res?.error === "string" && res.error.startsWith("bad request")) throw new BridgeError(`the bridge: ${res.error}`, "protocol");
  return res;
}

export function request(op, payload = {}, { port = bridgePort(), env = process.env, token = readToken(env), timeoutMs } = {}) {
  return new Promise((resolve, reject) => {
    if (!token) return reject(new BridgeError(`no token at ${tokenFile(env)}: enable the LF Bridge extension in Blender once (tools\\cli blender install)`, "notoken"));
    const seconds = payload.timeout ?? DEFAULT_TIMEOUT_S[op] ?? 60;
    const wait = timeoutMs ?? (seconds + 10) * 1000;
    const sock = net.connect({ host: BRIDGE_HOST, port });
    const reader = new LineReader();
    let settled = false;
    const done = (fn, v) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      sock.destroy();
      fn(v);
    };
    const timer = setTimeout(() => done(reject, new BridgeError(`the bridge did not answer within ${Math.round(wait / 1000)}s`, "timeout")), wait);
    sock.setEncoding("utf8");
    sock.on("connect", () => sock.write(encodeRequest(op, payload, token)));
    sock.on("data", (chunk) => {
      try {
        const [line] = reader.push(chunk);
        if (line !== undefined) done(resolve, parseResponse(line));
      } catch (e) {
        done(reject, e);
      }
    });
    sock.on("error", (e) => done(reject, new BridgeError(e.code === "ECONNREFUSED" ? `nothing is listening on ${BRIDGE_HOST}:${port}` : `bridge socket: ${e.message}`, e.code === "ECONNREFUSED" ? "down" : "protocol")));
    sock.on("close", () => done(reject, new BridgeError("the bridge closed the connection without answering", "protocol")));
  });
}

// { state: "listening"|"down"|"auth"|"notoken"|"busy", info?, detail? } - never throws.
async function status(opts = {}) {
  try {
    const info = await request("ping", { timeout: 3 }, { ...opts, timeoutMs: 4000 });
    return { state: "listening", info };
  } catch (e) {
    if (e instanceof BridgeError && ["down", "auth", "notoken"].includes(e.code)) return { state: e.code, detail: e.message };
    if (e instanceof BridgeError && e.code === "timeout") return { state: "busy", detail: e.message };
    return { state: "down", detail: e.message };
  }
}

export const bridge = {
  request,
  status,
  ping: (opts) => request("ping", {}, opts),
  // exec(code, { args, file, timeout }) -> { ok, stdout, stderr, result, error, secs }
  exec: (code, { args = [], file, timeout, ...opts } = {}) => request("exec", { code, args, ...(file ? { file } : {}), ...(timeout ? { timeout } : {}) }, opts),
  // shot(out, { view, camera, size, target, timeout }) -> { ok, out, bytes, size, view }
  shot: (out, { view, camera, size, target, timeout, ...opts } = {}) =>
    request("shot", { out: resolve(out), ...(view ? { view } : {}), ...(camera ? { camera } : {}), ...(size ? { size } : {}), ...(target ? { target } : {}), ...(timeout ? { timeout } : {}) }, opts),
};

// ---------------------------------------------------------------- the same script, in the live Blender

// A headless script starts with bpy.ops.wm.read_factory_settings(use_empty=True), which in a live
// session would also reset the user's add-ons (the bridge among them) and leave the operators
// after it without a window context. File > New with an empty scene is what is meant, so that one
// call is swapped for the bridge's in-place `lf_new_scene()` (runner.py new_empty_scene).
export function liveSource(code) {
  return code.replace(/bpy\.ops\.wm\.read_factory_settings\(\s*use_empty\s*=\s*True\s*\)/g, "lf_new_scene()");
}

// Run a script file in the user's open Blender. Replaces the open scene when the script starts
// from an empty one, so it refuses when the file has unsaved changes unless discard is set.
// Returns { status: 0, stdout, stderr, live: true, info }; throws like runHeadless on failure.
export async function runLive(script, args = [], { timeoutMs = 600000, log, discard = false, ...opts } = {}) {
  const st = await status(opts);
  if (st.state !== "listening") throw new Error(`the live Blender is not reachable: ${st.detail || st.state}`);
  const code = readFileSync(script, "utf8");
  if (liveSource(code) !== code && st.info.dirty && !discard) {
    throw new Error(`the open Blender file (${st.info.file || "untitled"}) has unsaved changes and ${script} starts from an empty scene; save it, or pass --discard`);
  }
  const res = await bridge.exec(liveSource(code), { args, file: resolve(script), timeout: Math.ceil(timeoutMs / 1000), ...opts });
  if (log) {
    mkdirSync(dirname(log), { recursive: true });
    writeFileSync(log, (res.stdout || "") + "\n" + (res.stderr || ""));
  }
  if (!res.ok) {
    const tail = ((res.stdout || "") + (res.stderr || "")).trim().split("\n").slice(-12).join("\n");
    throw new Error(`Blender ${script} failed in the live session${log ? ` (log: ${log})` : ""}\n${res.error || "no error text"}${tail ? `\n${tail}` : ""}`);
  }
  return { status: 0, stdout: res.stdout || "", stderr: res.stderr || "", live: true, info: st.info, result: res.result };
}
