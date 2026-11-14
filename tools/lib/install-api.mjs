// Engine API answers from the installed game's .pak files, stamped with the build.
// api_search reads a docs dump that can lag the game; this reads the scripts the
// game actually compiles. See docs/api-from-install.md style notes in the skill.
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, statSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { pathToFileURL, fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
export const DEFAULT_GAME = "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";

export function gamePath() {
  return process.env.ENFUSION_GAME_PATH || DEFAULT_GAME;
}

// Build stamp from the Steam app manifest next to the install.
export function parseManifest(text) {
  const get = (k) => (text.match(new RegExp(`"${k}"\\s+"([^"]*)"`)) || [])[1] || "";
  const updated = Number(get("LastUpdated")) || 0;
  return { buildid: get("buildid"), updated, updatedIso: updated ? new Date(updated * 1000).toISOString().slice(0, 10) : "" };
}

export function buildStamp(game = gamePath()) {
  const mf = join(game, "..", "..", "appmanifest_1874880.acf");
  if (!existsSync(mf)) return { buildid: "unknown", updated: 0, updatedIso: "", text: "build unknown (no Steam manifest)" };
  const s = parseManifest(readFileSync(mf, "utf8"));
  return { ...s, text: `Arma Reforger build ${s.buildid} (installed ${s.updatedIso})` };
}

// api_search's dump date = the commit date of the MCP clone it ships in.
export function dumpDate(mcpDir) {
  try {
    const out = execFileSync("git", ["-C", mcpDir, "log", "-1", "--format=%ct"], { encoding: "utf8", windowsHide: true }).trim();
    return Number(out) || 0;
  } catch {
    return 0;
  }
}

export function staleNote(stamp, dump) {
  if (!stamp.updated) return "api_search staleness unknown (no manifest)";
  if (!dump) return "api_search dump date unknown; treat it as a fallback only";
  if (stamp.updated > dump) return `STALE: api_search dump (${new Date(dump * 1000).toISOString().slice(0, 10)}) predates this install; trust the install, not api_search`;
  return "api_search dump is not older than this install";
}

export function findMcpDir() {
  const rel = join("tools", "mcp", "enfusion-mcp-bk");
  if (existsSync(join(ROOT, rel, "dist"))) return join(ROOT, rel);
  try {
    const common = execFileSync("git", ["-C", ROOT, "rev-parse", "--git-common-dir"], { encoding: "utf8", windowsHide: true }).trim();
    const main = resolve(ROOT, common, "..");
    if (existsSync(join(main, rel, "dist"))) return join(main, rel);
  } catch {}
  return null;
}

export async function openVfs(mcpDir = findMcpDir(), game = gamePath()) {
  if (!mcpDir) throw new Error("MCP clone not built; run tools\\cli mcp install");
  const mod = await import(pathToFileURL(join(mcpDir, "dist", "pak", "vfs.js")).href);
  const vfs = mod.PakVirtualFS.get(game);
  if (!vfs) throw new Error(`no .pak files under ${game}/addons`);
  return vfs;
}

const esc = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

// Pure: find `class Name ... {` declarations (plain or modded) in script text.
// Returns the declaration line plus the attribute lines above it and the body.
export function extractClass(text, name) {
  const lines = text.split(/\r?\n/);
  const re = new RegExp(`^\\s*(?:modded\\s+)?class\\s+${esc(name)}\\b`);
  const out = [];
  for (let i = 0; i < lines.length; i++) {
    if (!re.test(lines[i])) continue;
    let start = i;
    while (start > 0 && /^\s*(\[[^\]]*\]|\/\/.*)\s*$/.test(lines[start - 1])) start--;
    let depth = 0, seen = false, end = i;
    for (let j = i; j < lines.length; j++) {
      for (const ch of lines[j]) {
        if (ch === "{") { depth++; seen = true; }
        else if (ch === "}") depth--;
      }
      end = j;
      if (seen && depth <= 0) break;
      if (!seen && /;\s*$/.test(lines[j])) break;
    }
    out.push({ line: i + 1, declaration: lines[i].trim(), attributes: lines.slice(start, i).map((l) => l.trim()).filter((l) => l.startsWith("[")), text: lines.slice(start, end + 1).join("\n") });
  }
  return out;
}

export function scriptPaths(vfs) {
  return vfs.allFilePaths().filter((p) => p.toLowerCase().endsWith(".c") && /scripts\//i.test(p));
}

export function findClass(vfs, name, { maxFiles = 4000 } = {}) {
  const needle = new RegExp(`\\bclass\\s+${esc(name)}\\b`);
  const hits = [];
  for (const p of scriptPaths(vfs).slice(0, maxFiles)) {
    let text;
    try { text = vfs.readTextFile(p); } catch { continue; }
    if (!needle.test(text)) continue;
    for (const c of extractClass(text, name)) hits.push({ path: p, ...c });
  }
  return hits;
}

export function grepScripts(vfs, pattern, { prefix = "", limit = 40 } = {}) {
  const re = new RegExp(pattern);
  const hits = [];
  for (const p of vfs.allFilePaths()) {
    if (prefix && !p.toLowerCase().includes(prefix.toLowerCase())) continue;
    if (!/\.(c|et|conf|layout|st)$/i.test(p)) continue;
    let text;
    try { text = vfs.readTextFile(p); } catch { continue; }
    const lines = text.split(/\r?\n/);
    for (let i = 0; i < lines.length; i++) {
      if (re.test(lines[i])) {
        hits.push({ path: p, line: i + 1, text: lines[i].trim().slice(0, 200) });
        if (hits.length >= limit) return hits;
      }
    }
  }
  return hits;
}
