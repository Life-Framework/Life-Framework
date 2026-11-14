// Offline proof of the Blender bridge (no Blender needed): tools/lib/blender.mjs and tools/blender/lf_bridge.
//   1. the locator: LF_BLENDER wins; the default is 4.5; needs:'archbuilder' picks the install whose
//      has arch_builder in any repository under its user extensions folder or in the lf_local repository
//      directory (LF_ARCHBUILDER_DIR, default C:/Users/jaspe/Documents/Reforger/blender-addons); an unmet need is an error;
//   2. the client: one request is one JSON line carrying the token; a split or oversize response line;
//      no token file, a refused token and a dead port are told apart, against a fake server;
//      `show` target resolution (slug, addon path, .xob refusal) in a scratch repo;
//   3. the server (Python, tools/blender/selftest_protocol.py): token required before any handler,
//      loopback only, one line in and one out, junk and oversize refused. Skipped with a note when no
//      Python 3 can be found (Blender's own bundled one counts);
//   4. what is committed: no token, node_modules, zip or Blender path other than the documented
//      defaults in tools/blender, tools/lib/blender.mjs and tools/commands/blender.mjs.
// red-proof: 2026-11-14, one line broken, restored after its FAIL was seen:
//   locate() ignoring `needs`                         -> FAIL locator: needs archbuilder picks the 5.3 install
//   handle_line() without the hmac.compare_digest test -> FAIL (python) no token: unauthorized, and the handler never ran
import net from "node:net";
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, utimesSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { BridgeError, KNOWN_INSTALLS, LineReader, NEEDS, bridge, encodeRequest, liveSource, locate, parseResponse, tokenFile, userExtensionsDir, userRepoRoot, versionOf } from "../lib/blender.mjs";
import { parseArgs, resolveShowTarget } from "../commands/blender.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");

export async function main() {
  let failed = 0;
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    console.log(`FAIL  ${what}`);
  };

  // ------------------------------------------------------------ 1. the locator
  const [b45, b53] = KNOWN_INSTALLS;
  ok(versionOf(b45) === "4.5" && versionOf(b53) === "5.3", "locator: versions are read from both documented install paths");
  const appdata = "C:/Users/test/AppData/Roaming";
  const env = { APPDATA: appdata };
  // A fake file system: the listed files, every folder above them, and readdir over both.
  const norm = (p) => String(p).replaceAll("\\", "/").replace(/\/$/, "");
  const fsOf = (...files) => {
    const all = new Set();
    for (const f of files) {
      const parts = norm(f).split("/");
      for (let i = 1; i <= parts.length; i++) all.add(parts.slice(0, i).join("/"));
    }
    return {
      exists: (p) => all.has(norm(p)),
      list: (d) => [...all].filter((x) => x.startsWith(norm(d) + "/") && !x.slice(norm(d).length + 1).includes("/")).map((x) => x.slice(norm(d).length + 1)),
    };
  };
  const repos = (exe) => norm(userRepoRoot(exe, env));
  const LOCAL = "C:/Users/jaspe/Documents/Reforger/blender-addons";
  const base = [b45, b53];
  const world = (...extra) => fsOf(...base, ...extra);
  const loc = (w, o = {}) => locate({ env, exists: w.exists, list: w.list, ...o });
  ok(locate({ env: { LF_BLENDER: "D:/x/blender.exe" }, exists: world().exists }).exe === "D:/x/blender.exe", "locator: LF_BLENDER overrides everything");
  ok(loc(world()).exe === b45, "locator: the default is Blender 4.5 (the clean stage's proven version)");
  ok(loc(fsOf(b53)).exe === b53, "locator: with only 5.3 installed the default is 5.3");
  ok(loc(world(), { version: "5.3" }).exe === b53, "locator: a version request picks it");
  ok(loc(world(), { needs: undefined }).exe === b45, "locator: no need, no extension check");
  // ArchBuilder in a user repository of one version
  ok(loc(world(`${repos(b53)}/user_default/arch_builder/blender_manifest.toml`), { needs: "archbuilder" }).exe === b53, "locator: needs archbuilder finds user_default/arch_builder under 5.3");
  ok(loc(world(`${repos(b45)}/some_other_repo/arch_builder/blender_manifest.toml`), { needs: "archbuilder" }).exe === b45, "locator: any repository folder under the version's extensions counts, not only user_default");
  // ArchBuilder in the shared local repository lf_local (the default directory), seen by every version
  const shared = world(`${LOCAL}/arch_builder/blender_manifest.toml`);
  const sh = loc(shared, { needs: "archbuilder" });
  ok(sh.exe === b53 && /lf_local/.test(sh.source), "locator: the local repo directory is found at its default path, newest Blender first");
  ok(loc(shared, { needs: "archbuilder", version: "4.5" }).exe === b45, "locator: a version request still applies to the shared repository");
  ok(loc(world(`${LOCAL}/arch_builder/x`, `${repos(b45)}/user_default/arch_builder/x`), { needs: "archbuilder" }).exe === b45, "locator: an install with the add-on in its own user folder beats one that only sees the shared repo");
  // LF_ARCHBUILDER_DIR moves the shared repository
  const moved = fsOf(...base, "D:/ab/arch_builder/x");
  ok(locate({ env: { ...env, LF_ARCHBUILDER_DIR: "D:/ab" }, exists: moved.exists, list: moved.list, needs: "archbuilder" }).exe === b53, "locator: LF_ARCHBUILDER_DIR names another local repository directory");
  let threw = "";
  try { locate({ env: { ...env, LF_ARCHBUILDER_DIR: "D:/elsewhere" }, exists: shared.exists, list: shared.list, needs: "archbuilder" }); } catch (e) { threw = e.message; }
  ok(/arch_builder/.test(threw) && /D:\/elsewhere/.test(threw) && /LF_ARCHBUILDER_DIR/.test(threw), "locator: with the env var pointing away from the add-on the error names the folder, the directory and the variable");
  threw = "";
  try { loc(world(), { needs: "archbuilder" }); } catch (e) { threw = e.message; }
  ok(/arch_builder/.test(threw) && threw.includes(LOCAL), "locator: needs archbuilder with ArchBuilder nowhere is an error that names the default directory");
  threw = "";
  try { loc(world(), { needs: "teleporter" }); } catch (e) { threw = e.message; }
  ok(/unknown need/.test(threw), "locator: an unknown need is an error");
  const none = locate({ env, exists: () => false, list: () => [] });
  ok(none.missing && none.exe === b45, "locator: nothing installed hands back the default path, flagged missing");
  ok(userExtensionsDir(b53, env).replace(/\\/g, "/") === `${appdata}/Blender Foundation/Blender/5.3/extensions/user_default`, "locator: the user extensions folder is under APPDATA by Blender version");

  // ------------------------------------------------------------ 2. the client
  const line = encodeRequest("exec", { code: "print(1)\nprint(2)" }, "tok", 7);
  ok(line.endsWith("\n") && line.indexOf("\n") === line.length - 1, "client: a request is exactly one line, newlines in the code escaped");
  const parsed = JSON.parse(line);
  ok(parsed.token === "tok" && parsed.op === "exec" && parsed.id === 7 && parsed.code === "print(1)\nprint(2)", "client: the line carries the token, op, id and code");
  const lr = new LineReader(32);
  ok(lr.push('{"a":').length === 0 && lr.push("1}\n{").join("|") === '{"a":1}' && lr.push('"b":2}\n').join("|") === '{"b":2}', "client: a response split across chunks is rejoined");
  threw = "";
  try { new LineReader(8).push("123456789"); } catch (e) { threw = e.code; }
  ok(threw === "protocol", "client: an endless response line is cut off");
  threw = "";
  try { parseResponse('{"ok":false,"error":"unauthorized"}'); } catch (e) { threw = e.code; }
  ok(threw === "auth", "client: an unauthorized answer is an auth error");
  threw = "";
  try { parseResponse("<html>"); } catch (e) { threw = e.code; }
  ok(threw === "protocol", "client: a non-JSON answer is a protocol error");
  ok(parseResponse('{"ok":true,"x":1}').x === 1, "client: a normal answer parses");

  const tmp = mkdtempSync(join(tmpdir(), "lf-bridge-selftest-"));
  const fileEnv = { LF_BRIDGE_TOKEN_FILE: join(tmp, "token") };
  try {
    ok(tokenFile(fileEnv) === join(tmp, "token") && /\.life-framework[\\/]blender-bridge-token$/.test(tokenFile({})), "client: the token file is ~/.life-framework/blender-bridge-token, overridable");
    const noToken = await bridge.ping({ env: fileEnv, port: 1 }).catch((e) => e);
    ok(noToken instanceof BridgeError && noToken.code === "notoken", "client: no token file is told apart from a dead port");
    writeFileSync(fileEnv.LF_BRIDGE_TOKEN_FILE, "secret\n");
    const dead = await bridge.ping({ env: fileEnv, port: 1 }).catch((e) => e);
    ok(dead instanceof BridgeError && dead.code === "down", "client: a dead port is 'down'");
    ok((await bridge.status({ env: fileEnv, port: 1 })).state === "down", "client: status() reports down without throwing");

    // A fake bridge: answers a request that carries "secret", refuses any other token.
    const seen = [];
    const fake = net.createServer((sock) => {
      let buf = "";
      sock.on("data", (d) => {
        buf += d;
        let i;
        while ((i = buf.indexOf("\n")) >= 0) {
          const req = JSON.parse(buf.slice(0, i));
          buf = buf.slice(i + 1);
          seen.push(req);
          const res = req.token === "secret" ? { id: req.id, ok: true, blender: "9.9.9", dirty: false, file: "", stdout: `ran ${req.op}\n`, result: { n: 1 } } : { id: req.id, ok: false, error: "unauthorized" };
          sock.write(JSON.stringify(res).slice(0, 10)); // split the answer on purpose
          setTimeout(() => sock.write(JSON.stringify(res).slice(10) + "\n"), 5);
        }
      });
    });
    await new Promise((r) => fake.listen(0, "127.0.0.1", r));
    const port = fake.address().port;
    try {
      const pong = await bridge.ping({ env: fileEnv, port });
      ok(pong.ok && pong.blender === "9.9.9", "client: ping against the fake bridge, answer split in two chunks");
      const ex = await bridge.exec("print(1)", { env: fileEnv, port, args: ["a"], file: "x.py" });
      ok(ex.stdout === "ran exec\n" && ex.result.n === 1 && seen.at(-1).args[0] === "a" && seen.at(-1).file === "x.py", "client: exec sends code, args and file and returns stdout and result");
      const sh = await bridge.shot(join(tmp, "a.png"), { env: fileEnv, port, view: "iso", size: [640, 360] });
      ok(sh.ok && seen.at(-1).op === "shot" && seen.at(-1).view === "iso" && seen.at(-1).size[0] === 640 && seen.at(-1).out.endsWith("a.png"), "client: shot sends the path, view and size");
      writeFileSync(fileEnv.LF_BRIDGE_TOKEN_FILE, "wrong\n");
      const bad = await bridge.ping({ env: fileEnv, port }).catch((e) => e);
      ok(bad instanceof BridgeError && bad.code === "auth", "client: a refused token is an auth error, not 'down'");
      ok((await bridge.status({ env: fileEnv, port })).state === "auth", "client: status() reports auth");
      writeFileSync(fileEnv.LF_BRIDGE_TOKEN_FILE, "secret\n");
      ok((await bridge.status({ env: fileEnv, port })).state === "listening", "client: status() reports listening");
    } finally {
      fake.close();
    }
    // a bridge that accepts and never answers
    const mute = net.createServer(() => {});
    await new Promise((r) => mute.listen(0, "127.0.0.1", r));
    try {
      const slow = await bridge.ping({ env: fileEnv, port: mute.address().port, timeoutMs: 150 }).catch((e) => e);
      ok(slow instanceof BridgeError && slow.code === "timeout", "client: a bridge that never answers times out");
    } finally {
      mute.close();
    }
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }

  // the live shim and the argument parser
  ok(liveSource("bpy.ops.wm.read_factory_settings(use_empty=True)\nx = 1") === "lf_new_scene()\nx = 1", "live: a script's factory reset becomes the in-place empty scene");
  ok(liveSource("print(1)") === "print(1)", "live: a script without the reset is untouched");
  const pa = parseArgs(["x.py", "--live", "--needs", "archbuilder", "--", "--live", "a"]);
  ok(pa.pos[0] === "x.py" && pa.flags.has("--live") && pa.val["--needs"] === "archbuilder" && pa.rest.join(",") === "--live,a", "command: arguments after -- belong to the script");

  // `show`: what a target resolves to, in a scratch repo
  const rt = mkdtempSync(join(tmpdir(), "lf-show-selftest-"));
  try {
    const put = (rel, mtimeS) => {
      const f = join(rt, rel);
      mkdirSync(dirname(f), { recursive: true });
      writeFileSync(f, "x");
      if (mtimeS) utimesSync(f, mtimeS, mtimeS);
      return f;
    };
    mkdirSync(join(rt, "tools/assets/widget"), { recursive: true });
    writeFileSync(join(rt, "tools/assets/widget/spec.json"), JSON.stringify({ slug: "widget", asset_dir: "Assets/Items/Widget" }));
    const src = put("addons/LifeFramework/Assets/Items/Widget/Widget.fbx", 1000);
    ok(resolveShowTarget(rt, "widget").file === src, "show: a slug with nothing built shows the addon's source .fbx");
    const raw = put("tmp/assets/widget/raw/model.glb");
    ok(resolveShowTarget(rt, "widget").file === raw, "show: a slug prefers the raw generated GLB to the addon source");
    put("tmp/assets/widget/clean/Old.fbx", 2000);
    const fresh = put("tmp/assets/widget/clean/Widget.fbx", 3000);
    ok(resolveShowTarget(rt, "widget").file === fresh, "show: a slug prefers the newest cleaned .fbx");
    ok(resolveShowTarget(rt, "addons/LifeFramework/Assets/Items/Widget/Widget.fbx").file === src, "show: an addon path is read relative to the repo");
    ok(resolveShowTarget(rt, src).file === src, "show: an absolute model path is taken as it is");
    const bad = (t) => { try { resolveShowTarget(rt, t); return ""; } catch (e) { return e.message; } };
    put("addons/LifeFramework/Assets/Items/Widget/Widget.xob");
    ok(/compiled \.xob/.test(bad("addons/LifeFramework/Assets/Items/Widget/Widget.xob")), "show: a compiled .xob is refused with the way out");
    ok(/neither a model file/.test(bad("nonsense")), "show: an unknown target is refused");
    mkdirSync(join(rt, "tools/assets/empty"), { recursive: true });
    writeFileSync(join(rt, "tools/assets/empty/spec.json"), JSON.stringify({ slug: "empty", asset_dir: "Assets/Items/Empty" }));
    ok(/no model to show/.test(bad("empty")), "show: a slug with no model anywhere says what to run");
    put("notes.txt");
    ok(/cannot show a \.txt/.test(bad("notes.txt")), "show: a file Blender cannot open is refused");
  } finally {
    rmSync(rt, { recursive: true, force: true });
  }

  // ------------------------------------------------------------ 3. the server (Python)
  const py = findPython();
  if (!py) {
    console.log("NOTE  blender-bridge-selftest: no Python 3 found (PATH, LF_PYTHON, Blender's own); the server checks were skipped");
  } else {
    const res = spawnSync(py.cmd, [...py.args, "-B", join(ROOT, "tools", "blender", "selftest_protocol.py")], { encoding: "utf8", windowsHide: true, timeout: 120000 });
    const out = (res.stdout || "") + (res.stderr || "");
    const fails = out.split("\n").filter((l) => l.startsWith("FAIL"));
    for (const f of fails) console.log(`FAIL  (python) ${f.slice(5)}`);
    ok(res.status === 0 && !fails.length && /ok   the server is bound to 127.0.0.1/.test(out), `server: protocol checks (python ${py.cmd}) exit ${res.status}${res.status ? "\n" + out.trim().split("\n").slice(-6).join("\n") : ""}`);
  }

  // ------------------------------------------------------------ 4. what is committed
  const files = [];
  const walk = (dir) => {
    for (const name of readdirSync(dir)) {
      const full = join(dir, name);
      if (statSync(full).isDirectory()) {
        if (name === "__pycache__") continue; // generated by any Python run, gitignored
        ok(name !== "node_modules", `hygiene: no ${name} folder under tools/blender`);
        walk(full);
      } else files.push(full);
    }
  };
  walk(join(ROOT, "tools", "blender"));
  for (const f of files) ok(!/\.(zip|pyc|token)$/i.test(f) && !/blender-bridge-token$/.test(f), `hygiene: ${f} is not a build product or a token`);
  const scanned = [...files, join(ROOT, "tools", "lib", "blender.mjs"), join(ROOT, "tools", "commands", "blender.mjs"), join(ROOT, "tools", "assets", "pipeline", "shot.mjs")];
  const allowed = new Set([...KNOWN_INSTALLS, NEEDS.archbuilder.defaultDir].map((p) => p.toLowerCase()));
  for (const f of scanned) {
    const text = readFileSync(f, "utf8");
    ok(!/[0-9a-f]{64}/i.test(text), `hygiene: ${f} holds no 64-hex string (a token)`);
    for (const m of text.matchAll(/[A-Za-z]:[\\/][^"'`\s)]*[Bb]lender[^"'`\s)]*/g)) {
      const path = m[0].replace(/\\/g, "/").toLowerCase();
      ok([...allowed].some((a) => path === a || path.startsWith(a.replace(/\/blender\.exe$/, ""))) || /^c:\/windows|^c:\/users\/test\//.test(path), `hygiene: ${f} names a Blender path that is not a documented default: ${m[0]}`);
    }
  }
  ok(existsSync(join(ROOT, "tools", "blender", "lf_bridge", "blender_manifest.toml")), "hygiene: the extension has its manifest");

  if (failed) {
    console.log(`blender-bridge-selftest: FAILED (${failed})`);
    return 1;
  }
  console.log("blender-bridge-selftest: OK");
  return 0;
}

// A Python 3 that runs: LF_PYTHON, python3, python, py -3, then the one bundled with a known Blender.
function findPython() {
  const tries = [];
  if (process.env.LF_PYTHON) tries.push({ cmd: process.env.LF_PYTHON, args: [] });
  tries.push({ cmd: "python3", args: [] }, { cmd: "python", args: [] }, { cmd: "py", args: ["-3"] });
  for (const exe of KNOWN_INSTALLS) {
    const v = versionOf(exe);
    const bundled = join(dirname(exe), v || "", "python", "bin", "python.exe");
    if (v && existsSync(bundled)) tries.push({ cmd: bundled, args: [] });
  }
  for (const t of tries) {
    const r = spawnSync(t.cmd, [...t.args, "-c", "import sys; sys.exit(0 if sys.version_info[0] == 3 else 1)"], { windowsHide: true, timeout: 20000 });
    if (r.status === 0) return t;
  }
  return null;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
