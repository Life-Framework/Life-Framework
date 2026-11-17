// The slim-gate risk rules (docs/worktrees.md "Gate tiers"). A T2 diff gets the slim gate (validate --full with
// the Workbench compile, plus the LOGIC tier) unless it hits one of these classes; then it keeps the full gate
// (compile + build, the whole suite, core-only, placement audit, config sessions, pre-move e2e). The main guard
// (tools/main-guard.mjs) runs the full suite after the merge and reverts what broke it.
//
//   persistence  a Persistence/ folder, a *Persist*/*Serializ*/*Save*/*Record*.c file, or a changed line that
//                registers a payload or touches a save/load context
//   money        shop, bank, ATM, ledger, robbery reward and payout, fence, vehicle buy scripts, or a changed
//                line that moves cash (LF_MoneyUtils, AddCash/TakeCash/..., payout, reward)
//   rpc          a changed line naming RplRpc / RpcAsk_ / RpcDo_, or one inside an RPC handler's body
//   guid         a .meta added, changed or removed, or a hunk that swaps one {GUID} reference for another
//   boundary     a .gproj, a mission header, tools/lib/addons.mjs or the addon-boundary check
//   review       an interrogate file (tmp/review-*.md) in the worktree
//   unreadable   a changed script whose patch could not be read
//
// Pure functions; proven offline by tools/validation/gate-risk-selftest.mjs.

const GUID_G = /\{[0-9A-Fa-f]{16}\}/g;

function base(path) {
  return path.split("/").pop();
}

const PERSIST_PATH = /\/Persistence\//;
const PERSIST_FILE = /(Persist|Serializ|Save|Record)[^/]*\.c$/;
const PERSIST_LINE = /RegisterPersistencePayloads|LF_Persistence\w*|Serializer\b|SaveContext|LoadContext|PersistenceManager/;

const MONEY_PATH = [
  /\/Feature\/(Money|Shop|ATM|Banking|Trader)\//,
  /\/Feature\/Crime\/LF_(Bank|Rob|Fence|HeistVault|CashVan|ChopShop|StolenVehicles|Contraband)\w*\.c$/,
  /\/Feature\/Vehicles\/LF_Vehicle(Lot|Prices|Catalog|Ownership)\w*\.c$/,
  /\/LF_\w*(Money|Wallet|Cash|Payout|Reward|Ledger|Price|Fence)\w*\.c$/,
];
const MONEY_LINE = /\bLF_MoneyUtils\b|\bLF_BankAccount\b|\bLF_Ledger\b|\b(AddCash|RemoveCash|GiveCash|TakeCash|AddAmount|RemoveAmount)\s*\(|Payout|Reward/;

const RPC_LINE = /RplRpc|RpcAsk_|RpcDo_/;
const RPC_DECL = /\b(RpcAsk_|RpcDo_)\w*\s*\(/;

const BOUNDARY = (p) => /\.gproj$/.test(p) || (p.startsWith("addons/") && /\/Missions\//.test(p)) || p === "tools/lib/addons.mjs" || p === "tools/validation/validate-addon-boundary.mjs";

// `git diff -U0` text for one file -> [{ oldStart, oldLines, newStart, newLines, minus: [text], plus: [text] }].
export function parsePatch(text) {
  const hunks = [];
  let h = null;
  for (const line of String(text ?? "").split(/\r?\n/)) {
    const m = /^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@/.exec(line);
    if (m) {
      h = { oldStart: +m[1], oldLines: m[2] === undefined ? 1 : +m[2], newStart: +m[3], newLines: m[4] === undefined ? 1 : +m[4], minus: [], plus: [] };
      hunks.push(h);
      continue;
    }
    if (!h) continue;
    if (line.startsWith("-") && !line.startsWith("---")) h.minus.push(line.slice(1));
    else if (line.startsWith("+") && !line.startsWith("+++")) h.plus.push(line.slice(1));
  }
  return hunks;
}

// Splits a whole `git diff -U0` (many files) into { path: patchText }.
export function splitPatches(text) {
  const out = {};
  let cur = null;
  let buf = [];
  const flush = () => { if (cur) out[cur] = buf.join("\n"); };
  for (const line of String(text ?? "").split(/\r?\n/)) {
    const m = /^diff --git a\/(.+?) b\/(.+)$/.exec(line);
    if (m) {
      flush();
      cur = m[2];
      buf = [];
    }
    buf.push(line);
  }
  flush();
  return out;
}

// [start, end] line ranges (1-based) of RPC handler bodies: a function whose declaration names RpcAsk_/RpcDo_ or
// sits under an [RplRpc ...] attribute, from the declaration to its closing brace.
export function rpcRegions(text) {
  if (typeof text !== "string") return [];
  const lines = text.replace(/\r\n/g, "\n").split("\n");
  const out = [];
  for (let i = 0; i < lines.length; i++) {
    const l = lines[i];
    let decl = RPC_DECL.test(l) && !/;\s*$/.test(l) && !/\bRpc\s*\(/.test(l);
    if (!decl && /\(/.test(l) && !/;\s*$/.test(l)) {
      for (let k = i - 1; k >= 0 && k >= i - 2; k--) {
        if (/\[\s*RplRpc\b/.test(lines[k])) decl = true;
        if (lines[k].trim() && !/^\s*\[/.test(lines[k])) break;
      }
    }
    if (!decl) continue;
    let depth = 0;
    let opened = false;
    let end = lines.length;
    for (let j = i; j < lines.length; j++) {
      const s = lines[j].replace(/"(?:[^"\\]|\\.)*"/g, "").replace(/\/\/.*$/, "");
      for (const ch of s) {
        if (ch === "{") { depth++; opened = true; }
        else if (ch === "}") depth--;
      }
      if (opened && depth <= 0) { end = j + 1; break; }
    }
    let start = i + 1;
    if (i > 0 && /\[\s*RplRpc\b/.test(lines[i - 1])) start = i;
    out.push([start, end]);
    i = end - 1;
  }
  return out;
}

const inRegions = (regions, from, count) => {
  const to = from + Math.max(count, 1) - 1;
  return regions.some(([a, b]) => from <= b && to >= a);
};

// files: [{ path, status, before?, after?, patch? }] (gate-tier collectDiff). Returns [{ cls, why }], empty
// when the slim gate may run. `review`: the interrogate files found in the worktree.
export function riskClasses(files, { review = [] } = {}) {
  const out = [];
  const add = (cls, why) => { if (!out.some((r) => r.cls === cls)) out.push({ cls, why }); };
  if (review.length) add("review", `interrogate file ${review[0]}`);
  for (const f of files) {
    const p = f.path;
    if (BOUNDARY(p)) add("boundary", `${p}: the addon boundary, a mission header or a .gproj`);
    if (!p.startsWith("addons/")) continue;
    if (p.endsWith(".meta")) add("guid", `${p}: a .meta (GUID) ${f.status === "M" ? "changed" : "added or removed"}`);
    const script = p.endsWith(".c");
    if (script) {
      if (PERSIST_PATH.test(p) || PERSIST_FILE.test(base(p))) add("persistence", `${p}: persistence code or a record shape`);
      if (MONEY_PATH.some((r) => r.test(p))) add("money", `${p}: a money path`);
    }
    if (!f.patch) {
      // A script with no readable patch: we cannot see what it touches. Data files without a patch are judged by path only.
      if (script && f.status !== "D") add("unreadable", `${p}: the diff could not be read`);
      continue;
    }
    const hunks = parsePatch(f.patch);
    if (script) {
      const changed = hunks.flatMap((h) => [...h.minus, ...h.plus]);
      if (changed.some((l) => PERSIST_LINE.test(l))) add("persistence", `${p}: a changed line touches save/load or payload registration`);
      if (changed.some((l) => MONEY_LINE.test(l))) add("money", `${p}: a changed line moves money`);
      if (changed.some((l) => RPC_LINE.test(l))) add("rpc", `${p}: a changed line names an RPC`);
      else {
        const after = rpcRegions(f.after);
        const before = rpcRegions(f.before);
        if (hunks.some((h) => (h.newLines > 0 || h.oldLines === 0 ? inRegions(after, h.newStart, h.newLines) : inRegions(after, h.newStart, 1)) || (h.oldLines > 0 && inRegions(before, h.oldStart, h.oldLines))))
          add("rpc", `${p}: a change inside an RPC handler`);
      }
    } else {
      for (const h of hunks) {
        const was = new Set(h.minus.join("\n").match(GUID_G) ?? []);
        const now = new Set(h.plus.join("\n").match(GUID_G) ?? []);
        if (was.size && now.size && ([...was].some((g) => !now.has(g)) || [...now].some((g) => !was.has(g)))) {
          add("guid", `${p}: a {GUID} reference changed`);
          break;
        }
      }
    }
  }
  return out;
}
