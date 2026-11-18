// A server-to-client Rpc the host's own player must see needs a local path on a listen host (#2620, reshaped by #2641).
// inputs: addons/**/Scripts/**/*.c, tools/validation/validate-rpc-owner-host-path.mjs, tools/validation/rpc-owner-host-path-baseline.json
//
// The file name is the card's (#2620 asked for an owner-reply check). The engine settled that premise
// (docs/architecture/rpc-routing.md, #2641): on the server, `Rpc(RpcDo_X)` with `RplRcver.Owner` on an entity the
// server owns already runs the handler inside the call, so a check demanding `IsOwner()` or a direct call would
// make it run twice. What a listen host really lacks is `RplRcver.Broadcast`: it never runs on the server, so the
// host's own player sees no visual, hears no sound and gets no list unless the sender also runs the effect locally.
//
// This check flags each `Rpc(<handler>, ...)` whose `<handler>` is declared `RplRcver.Broadcast` and whose
// enclosing method shows no local path: neither a direct call of the handler (`RpcDo_X(...)`) nor a host guard
// (`RplSession.Mode()`, `System.IsConsoleApp()`, `m_bClient`) that gates the effect it runs itself. A site that is
// right for another reason (the host draws through a different path, or never needs the effect) is an explicit
// `file::method` line in rpc-owner-host-path-baseline.json with a reason. The baseline is never a blanket allow,
// and an entry that no longer matches a flagged site fails, so the list only shrinks.
//
// LF_RPCOWNER_ROOT points the scan at another tree and LF_RPCOWNER_BASELINE at another baseline (the selftest).
//
// red-proof: 2026-11-18, with the local-path test in broadcastSites forced to true, "a bare Broadcast Rpc is
// flagged" and three more checks printed FAIL in tools/validation/rpc-owner-host-path-selftest.mjs. On main with an
// empty baseline the check named the four sites now baselined (the config stream and three plant broadcasts).
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = process.env.LF_RPCOWNER_ROOT || join(here, "..", "..");

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

// Every function whose body sits directly in a class block: { name, start, end } over the scrubbed text.
export function methodsOf(code) {
  const stack = []; // { header, classLike, isMethod, start }
  const closed = [];
  let segStart = 0;
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
  return closed;
}

// Names of the handlers declared `[RplRpc(..., RplRcver.Broadcast)]` in one file.
export function broadcastHandlers(text) {
  const code = scrub(text);
  const names = [];
  const re = /RplRcver\s*\.\s*Broadcast\s*\)\s*\]\s*(?:\w+\s+)*(\w+)\s*\(/g;
  let m;
  while ((m = re.exec(code))) names.push(m[1]);
  return names;
}

const HOST_GUARD = /\bRplSession\s*\.\s*Mode\s*\(|\bSystem\s*\.\s*IsConsoleApp\s*\(|\bm_bClient\b/;

// One entry per `Rpc(<broadcast handler>, ...)` call: { line, rpc, method, localPath }. `method` is the function
// whose body holds the call; `localPath` is true when that body also calls the handler directly or has a host guard.
export function broadcastSites(text, handlers) {
  const code = scrub(text);
  const methods = methodsOf(code);
  const set = new Set(handlers);
  const sites = [];
  const re = /\bRpc\s*\(\s*(\w+)/g;
  let m;
  while ((m = re.exec(code))) {
    if (!set.has(m[1])) continue;
    const owner = methods.find((x) => x.start < m.index && m.index < x.end);
    if (!owner) continue;
    const body = code.slice(owner.start, owner.end);
    const direct = new RegExp(`(^|[^\\w.])${m[1]}\\s*\\(`).test(body) || new RegExp(`\\.\\s*${m[1]}\\s*\\(`).test(body);
    sites.push({
      line: code.slice(0, m.index).split("\n").length,
      rpc: m[1],
      method: owner.name,
      localPath: direct || HOST_GUARD.test(body),
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
  const baselinePath = process.env.LF_RPCOWNER_BASELINE || join(here, "rpc-owner-host-path-baseline.json");
  const baseline = existsSync(baselinePath) ? JSON.parse(readFileSync(baselinePath, "utf8")) : { sites: [] };
  const allowed = new Map(baseline.sites.map((s) => [s.site, s.reason]));
  const used = new Set();
  const bad = [];
  const files = scriptFiles(root).map((f) => ({ f, text: readFileSync(join(root, f), "utf8") }));
  const handlers = files.flatMap(({ text }) => broadcastHandlers(text));
  let total = 0;
  for (const { f, text } of files) {
    // The Rpc probe tests send a bare broadcast on purpose (they measure routing); they are not a host path.
    if (/\/Tests\//.test(f)) continue;
    for (const s of broadcastSites(text, handlers)) {
      total++;
      if (s.localPath) continue;
      const key = `${f}::${s.method}`;
      if (allowed.has(key)) {
        used.add(key);
        continue;
      }
      bad.push(
        `FAIL ${f}:${s.line} ${s.method}() calls Rpc(${s.rpc}), a RplRcver.Broadcast handler that never runs on the server: a listen host's own player gets nothing. Also call ${s.rpc}() (or run its effect) locally, guarded with RplSession.Mode() != RplMode.Dedicated`,
      );
    }
  }
  for (const key of allowed.keys()) if (!used.has(key)) bad.push(`FAIL rpc-owner-host-path-baseline.json: "${key}" is no longer flagged; delete the entry`);
  if (bad.length) {
    console.log(bad.join("\n"));
    console.log(`validate-rpc-owner-host-path: ${bad.length} problem(s) (docs/architecture/rpc-routing.md rule 3, #2620)`);
    process.exit(1);
  }
  console.log(`validate-rpc-owner-host-path: OK (${total} Rpc(<Broadcast handler>) site(s), ${used.size} baselined)`);
}
