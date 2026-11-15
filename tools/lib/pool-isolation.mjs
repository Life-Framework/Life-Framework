// tools/lib/pool-isolation.mjs - keeps a selftest of the pool away from the REAL pool.
//
// The pool's state lives where every agent on this PC shares it: <main checkout>/tmp/wt (locks, queue, hub
// state), the session dirs under %LOCALAPPDATA%\LifeFramework\session, and live server/client processes. A
// selftest that exercises the pool code against a fixture root must never read, write or kill any of it: on
// 2026-11-15 an agent reported pool-selftest "closed live sessions mid-run".
//
// isolatePool({ deny }) installs a guard in this process:
//   - fs: any path argument that resolves inside a denied directory throws and is recorded. Sync and
//     promise calls both; the throw is not enough on its own, so every violation is also kept in
//     violations() (code such as readJson swallows exceptions) and the selftest fails on a non-empty list.
//   - child processes: `taskkill` and process.kill only for pids this process started itself (a fixture
//     "live" holder), `node ... session.mjs down` never, and no child may run inside a denied directory.
// The selftest also moves everything the pool resolves by environment into a temp dir: LOCALAPPDATA and
// USERPROFILE (the session base), LF_SESSION_DIR (unset) and LF_POOL_JSON (a fixture pool.json).

import childProcess from "node:child_process";
import fs from "node:fs";
import { syncBuiltinESMExports } from "node:module";
import { basename, resolve, sep } from "node:path";

const norm = (p) => resolve(String(p)).replace(/[\\/]+$/, "").toLowerCase();

export function isInside(child, dir) {
  const c = norm(child);
  const d = norm(dir);
  return c === d || c.startsWith(d + sep) || c.startsWith(d + "\\") || c.startsWith(d + "/");
}

const PATH_FNS = [
  "accessSync", "appendFileSync", "chmodSync", "copyFileSync", "cpSync", "existsSync", "lstatSync", "mkdirSync",
  "openSync", "opendirSync", "readFileSync", "readdirSync", "readlinkSync", "realpathSync", "renameSync", "rmSync", "rmdirSync",
  "statSync", "symlinkSync", "truncateSync", "unlinkSync", "utimesSync", "writeFileSync",
];
const TWO_PATH = new Set(["copyFileSync", "cpSync", "renameSync", "symlinkSync"]);

let state = null;

// deny: directories the selftest must never touch. Returns { violations(), uninstall(), spawned }.
export function isolatePool({ deny, label = "selftest" }) {
  if (state) throw new Error("pool isolation is already installed");
  const denied = deny.filter(Boolean).map((d) => resolve(d));
  const found = [];
  const ownPids = new Set([process.pid]);
  const saved = [];
  const hit = (what, target) => {
    // the first frames outside this file say which pool call did it
    const frames = (new Error().stack ?? "").split(/\r?\n/).slice(2).filter((l) => !l.includes("pool-isolation.mjs")).slice(0, 3).map((l) => l.trim().replace(/^at /, ""));
    const msg = `${label} touched the real pool: ${what} ${target} <- ${frames.join(" < ")}`;
    found.push(msg);
    return new Error(`GUARD: ${msg}`);
  };
  const check = (what, target) => {
    if (target === undefined || target === null || typeof target === "number") return;
    let p = target;
    if (target instanceof URL) p = target.pathname.replace(/^\/([A-Za-z]:)/, "$1");
    else if (typeof target !== "string" && !Buffer.isBuffer(target)) return;
    for (const d of denied) if (isInside(Buffer.isBuffer(p) ? p.toString() : p, d)) throw hit(what, d === resolve(String(p)) ? d : String(p));
  };

  const patch = (obj, name, wrap) => {
    const orig = obj[name];
    if (typeof orig !== "function") return;
    saved.push([obj, name, orig]);
    obj[name] = wrap(orig);
  };

  for (const fn of PATH_FNS) {
    patch(fs, fn, (orig) => function guarded(...a) {
      check(fn, a[0]);
      if (TWO_PATH.has(fn)) check(fn, a[1]);
      return orig.apply(this, a);
    });
    if (fs.promises[fn.replace(/Sync$/, "")]) {
      const pn = fn.replace(/Sync$/, "");
      patch(fs.promises, pn, (orig) => function guarded(...a) {
        check(pn, a[0]);
        if (TWO_PATH.has(fn)) check(pn, a[1]);
        return orig.apply(this, a);
      });
    }
  }

  const guardSpawn = (name) =>
    patch(childProcess, name, (orig) => function guarded(cmd, args, opts) {
      const argv = Array.isArray(args) ? args.map(String) : [];
      const o = (Array.isArray(args) ? opts : args) ?? {};
      const exe = basename(String(cmd)).toLowerCase().replace(/\.exe$/, "");
      if (o.cwd) check(`${name} in cwd`, o.cwd);
      if (exe === "taskkill") {
        const i = argv.findIndex((x) => x.toLowerCase() === "/pid");
        const pid = Number(argv[i + 1]);
        if (!ownPids.has(pid)) throw hit("taskkill of a process this selftest did not start, pid", String(argv[i + 1]));
      }
      if (argv.some((x) => /session\.mjs$/i.test(x)) && argv.includes("down")) throw hit("session.mjs down", argv.join(" "));
      const child = orig.call(this, cmd, args, opts);
      if (child?.pid) ownPids.add(child.pid);
      return child;
    });
  for (const n of ["spawn", "spawnSync", "execFile", "execFileSync", "exec", "execSync"]) guardSpawn(n);
  patch(process, "kill", (orig) => function guarded(pid, sig) {
    // signal 0 only probes whether a pid lives; anything else must hit a process this selftest started
    if (sig !== 0 && !ownPids.has(Number(pid))) throw hit("process.kill of a process this selftest did not start, pid", String(pid));
    return orig.call(this, pid, sig);
  });

  syncBuiltinESMExports();
  state = { found };
  return {
    violations: () => [...found],
    // Runs fn, which must trip the guard (a deliberate probe); the probe is not kept as a violation.
    // Returns true when the guard threw.
    expectViolation(fn) {
      const before = found.length;
      let threw = false;
      try {
        fn();
      } catch (e) {
        threw = /^GUARD:/.test(String(e?.message));
      }
      found.length = before;
      return threw;
    },
    // Pids a fixture started by other means (a `deadPid` it captured) may be killed too.
    allowPid: (pid) => ownPids.add(Number(pid)),
    uninstall() {
      for (const [obj, name, orig] of saved.reverse()) obj[name] = orig;
      syncBuiltinESMExports();
      state = null;
    },
  };
}
