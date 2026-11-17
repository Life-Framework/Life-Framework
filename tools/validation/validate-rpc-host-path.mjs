// A client-to-server request must also work on a listen host (AGENTS.md "RPC direction", #2511).
// inputs: addons/**/Scripts/**/*.c, tools/validation/validate-rpc-host-path.mjs, tools/validation/rpc-host-path-baseline.json
//
// A `RplRcver.Server` handler named `RpcAsk_*` runs only when a client calls it. On a listen host the player IS
// the server, and `Rpc(RpcAsk_X, ...)` from the host does not reach its own handler, so the request is silently
// dropped (the debug menu did nothing for the host, #2497). Every method that calls `Rpc(RpcAsk_...)` therefore
// branches on `Replication.IsServer()` and calls the handler directly there.
//
// The check flags each `Rpc(RpcAsk_...)` whose enclosing method has no `Replication.IsServer()` anywhere in its
// body. A site it cannot judge (the host branch lives in the caller, or the request is a client-only path) is an
// explicit `file::method` line in rpc-host-path-baseline.json with a reason. The baseline is never a blanket
// allow, and an entry that no longer matches a flagged site fails so the list shrinks as sites are fixed.
//
// LF_RPCHOST_ROOT points the scan at another tree (the selftest uses a temp one).
//
// red-proof: 2026-11-17, on main this printed `FAIL ...LF_DebugMenu.c:213 ... LF_AskDebugAction` and exited 1; with
// the host branch added it passed. tools/validation/rpc-host-path-selftest.mjs holds the bare-versus-branch fixtures.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = process.env.LF_RPCHOST_ROOT || join(here, "..", "..");

// Replaces comments and string/char literal contents with spaces; newlines stay so line numbers hold.
export function scrub(text) {
  let out = "";
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    const n = text[i + 1];
    if (c === "/" && n === "/") {
      while (i < text.length && text[i] !== "\n") out += " ", i++;
      i--;
    } else if (c === "/" && n === "*") {
      out += "  ";
      i += 2;
      while (i < text.length && !(text[i] === "*" && text[i + 1] === "/")) out += text[i] === "\n" ? "\n" : " ", i++;
      out += "  ";
      i++;
    } else if (c === '"' || c === "'") {
      out += c;
      i++;
      while (i < text.length && text[i] !== c && text[i] !== "\n") {
        if (text[i] === "\\") out += " ", i++;
        out += " ";
        i++;
      }
      out += text[i] === c ? c : "";
    } else out += c;
  }
  return out;
}

// One entry per `Rpc(RpcAsk_...)` call: { line, rpc, method, hostBranch }. `method` is the function whose body
// directly sits in a class block; `hostBranch` is true when that body mentions Replication.IsServer().
export function rpcAskSites(text) {
  const code = scrub(text);
  const stack = []; // { header, start, classLike }
  const methods = []; // { name, start, end }
  let segStart = 0;
  const closed = [];
  for (let i = 0; i < code.length; i++) {
    const c = code[i];
    if (c === "{") {
      const header = code.slice(segStart, i).trim();
      const classLike = /(^|\s)(class|enum|struct)\s/.test(header) || /^(modded\s+)?class\b/.test(header);
      const parentIsClass = stack.length > 0 && stack[stack.length - 1].classLike;
      stack.push({ header, start: i, classLike, isMethod: parentIsClass && !classLike });
      segStart = i + 1;
    } else if (c === "}") {
      const b = stack.pop();
      if (b && b.isMethod) {
        const name = /([A-Za-z_]\w*)\s*\([^()]*(?:\([^()]*\)[^()]*)*\)\s*(?:override\s*)?$/.exec(b.header);
        closed.push({ name: name ? name[1] : b.header.slice(-40), start: b.start, end: i });
      }
      segStart = i + 1;
    } else if (c === ";") segStart = i + 1;
  }
  methods.push(...closed);
  const sites = [];
  const re = /\bRpc\s*\(\s*(RpcAsk_\w+)/g;
  let m;
  while ((m = re.exec(code))) {
    const owner = methods.find((x) => x.start < m.index && m.index < x.end);
    if (!owner) continue;
    const body = code.slice(owner.start, owner.end);
    sites.push({
      line: code.slice(0, m.index).split("\n").length,
      rpc: m[1],
      method: owner.name,
      hostBranch: /\bReplication\s*\.\s*IsServer\s*\(/.test(body),
    });
  }
  return sites;
}

export function scriptFiles(base) {
  const out = [];
  const addons = join(base, "addons");
  const walk = (dir, rel) => {
    for (const n of readdirSync(dir)) {
      const full = join(dir, n);
      if (statSync(full).isDirectory()) walk(full, `${rel}/${n}`);
      else if (n.endsWith(".c")) out.push(`${rel}/${n}`);
    }
  };
  for (const a of existsSync(addons) ? readdirSync(addons) : []) {
    const scripts = join(addons, a, "Scripts");
    if (existsSync(scripts)) walk(scripts, `addons/${a}/Scripts`);
  }
  return out;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const baselinePath = process.env.LF_RPCHOST_BASELINE || join(here, "rpc-host-path-baseline.json");
  const baseline = existsSync(baselinePath) ? JSON.parse(readFileSync(baselinePath, "utf8")) : { sites: [] };
  const allowed = new Map(baseline.sites.map((s) => [s.site, s.reason]));
  const used = new Set();
  const bad = [];
  let total = 0;
  for (const f of scriptFiles(root)) {
    for (const s of rpcAskSites(readFileSync(join(root, f), "utf8"))) {
      total++;
      if (s.hostBranch) continue;
      const key = `${f}::${s.method}`;
      if (allowed.has(key)) {
        used.add(key);
        continue;
      }
      bad.push(`FAIL ${f}:${s.line} ${s.method}() calls Rpc(${s.rpc}) with no Replication.IsServer() host branch: on a listen host the request is dropped. Call the handler directly when Replication.IsServer(), else Rpc(...)`);
    }
  }
  for (const key of allowed.keys()) if (!used.has(key)) bad.push(`FAIL rpc-host-path-baseline.json: "${key}" is no longer flagged; delete the entry`);
  if (bad.length) {
    console.log(bad.join("\n"));
    console.log(`validate-rpc-host-path: ${bad.length} problem(s) (AGENTS.md "RPC direction", #2511)`);
    process.exit(1);
  }
  console.log(`validate-rpc-host-path: OK (${total} Rpc(RpcAsk_) site(s), ${used.size} baselined)`);
}
