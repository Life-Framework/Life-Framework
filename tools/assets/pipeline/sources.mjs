// Source adapters: where a model or texture can come from before anything is generated.
// The ladder (docs/asset-pipeline/sources.md), tried in order and every rung shown on the same sheet:
//   1 vanilla     the Arma Reforger base game: native quality, already in the engine, no import
//   2 open source Poly Haven (CC0, source.mjs), Smithsonian Open Access (CC0), Sketchfab (CC0 / CC-BY only)
//   3 generate    concept -> Meshy (the rest of the pipeline)
// Textures: ambientCG (CC0). Sounds are not a ladder: they stay on ElevenLabs (docs/asset-pipeline/sounds.md).
// Animations: vanilla graphs and item animation attributes first, then our own Blender anim stage.
//
// An adapter is { rung, name, search(root, query, opts) -> [candidate] }. A candidate is
// { source, id, name, url, licence, author, thumb?, tris?, note? }. Add a provider by adding an entry to
// ADAPTERS; nothing else changes. A licence we cannot ship in a commercial mod never reaches a candidate.
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";

export const USER_AGENT = "LifeFrameworkAssetPipeline/1.0 (+https://life-framework.org)";

// CC0 needs nothing; CC-BY needs its credit in docs/credits.md. Everything else is out: NC and ND cannot
// ship in a commercial mod, SA would bind the mod's own licence, and an unlabelled model is unknown.
export function licenceVerdict(raw) {
  const l = String(raw || "").toLowerCase().replace(/[\s_]+/g, "-");
  if (/^(cc0|cc-?0|cc0-1\.0|public-domain|pd|zero)$/.test(l) || l.includes("cc0")) return { ok: true, licence: "CC0 1.0", credit: false };
  if (/^(by|cc-by|cc-by-(1|2|3|4)\.0|cc-by-4\.0|attribution)$/.test(l)) return { ok: true, licence: "CC-BY", credit: true };
  return { ok: false, licence: raw || "unknown", why: /nc/.test(l) ? "non-commercial" : /nd/.test(l) ? "no-derivatives" : /sa/.test(l) ? "share-alike" : "unclear" };
}

async function getJson(url, headers = {}) {
  const res = await fetch(url, { headers: { "User-Agent": USER_AGENT, ...headers }, signal: AbortSignal.timeout(30000) });
  if (!res.ok) throw new Error(`${url.split("?")[0]}: HTTP ${res.status}`);
  return res.json();
}

const keyFile = (name) => join(homedir(), ".life-framework", name);
const readKey = (env, file) => process.env[env] || (existsSync(keyFile(file)) ? readFileSync(keyFile(file), "utf8").trim() : "");

// ---------------------------------------------------------------- rung 1: the base game
// asset_search matches words inside a path, so one query finds only one word. Run several (synonyms and
// the part of a larger prop) and keep the models: the xobs, minus damage states and LOD copies.
export const VANILLA_SKIP = /_dst|\/Dst\/|_LOD|wreck|_pile|ruined/i;
export function vanillaQuery(root, q, runner = spawnSync) {
  const file = join(root, "tmp", "vanilla-query.json");
  writeFileSync(file, JSON.stringify({ query: q, limit: 60 }));
  const r = runner("node", ["tools/cli.mjs", "call", "asset_search", "@tmp/vanilla-query.json"], { cwd: root, encoding: "utf8", timeout: 90000, windowsHide: true });
  return String(r.stdout || "").split("\n").map((l) => l.trim()).filter((l) => /\.xob$/.test(l) && !VANILLA_SKIP.test(l)).map((l) => l.replace(/^Assets\//, ""));
}

export const vanilla = {
  rung: 1, name: "vanilla",
  async search(root, query, { queries = [], limit = 12, runner } = {}) {
    const words = [...new Set([...queries, ...query.words.slice(0, 4)])];
    const seen = new Map();
    for (const q of words) for (const p of vanillaQuery(root, q, runner)) seen.set(p, (seen.get(p) || 0) + 1);
    // A path that several queries found, or whose file name holds a query word, ranks first.
    const rank = (p) => (seen.get(p) || 0) * 2 + words.filter((w) => p.split("/").pop().toLowerCase().includes(w.toLowerCase())).length;
    return [...seen.keys()].sort((a, b) => rank(b) - rank(a)).slice(0, limit).map((p) => ({
      source: "vanilla", id: `Assets/${p}`, name: p.split("/").pop().replace(/\.xob$/, ""), url: `Assets/${p}`, licence: "Bohemia Interactive base game (in the engine, no import)", author: "Bohemia Interactive",
    }));
  },
};

// ---------------------------------------------------------------- rung 2: open-source libraries
export const smithsonian = {
  rung: 2, name: "smithsonian",
  // Smithsonian Open Access 3D: CC0 scans of museum objects. api.data.gov key: SMITHSONIAN_API_KEY or
  // ~/.life-framework/smithsonian-key; DEMO_KEY works at a low rate.
  async search(root, query, { limit = 4 } = {}) {
    const key = readKey("SMITHSONIAN_API_KEY", "smithsonian-key") || "DEMO_KEY";
    const q = encodeURIComponent(`${query.words.slice(0, 3).join(" ")} online_media_type:"3D Models"`);
    const j = await getJson(`https://api.si.edu/openaccess/api/v1.0/search?q=${q}&api_key=${key}&rows=${limit}`);
    return (j.response?.rows || []).map((r) => {
      const d = r.content?.descriptiveNonRepeating || {};
      const lic = licenceVerdict(d.metadata_usage?.access);
      return { source: "smithsonian", id: r.id, name: r.title, url: d.record_link || d.guid, licence: lic.licence, author: d.data_source || "Smithsonian", thumb: d.online_media?.media?.[0]?.thumbnail, ok: lic.ok, why: lic.why };
    }).filter((c) => c.ok);
  },
};

export const sketchfab = {
  rung: 2, name: "sketchfab",
  // Search is open; DOWNLOAD needs an API token: set SKETCHFAB_TOKEN or write it to
  // ~/.life-framework/sketchfab-token (sketchfab.com/settings#password, "API token"). Without a token this
  // adapter still lists candidates so a human can fetch the glTF by hand.
  async search(root, query, { limit = 4 } = {}) {
    const out = [];
    for (const lic of ["cc0", "by"]) {
      const q = encodeURIComponent(query.words.slice(0, 3).join(" "));
      const j = await getJson(`https://api.sketchfab.com/v3/search?type=models&q=${q}&downloadable=true&license=${lic}&count=${limit}`);
      for (const m of j.results || []) {
        const v = licenceVerdict(m.license?.slug || lic);
        if (!v.ok) continue;
        const thumbs = m.thumbnails?.images || [];
        out.push({ source: "sketchfab", id: m.uid, name: m.name, url: m.viewerUrl, licence: v.licence, credit: v.credit, author: m.user?.displayName, tris: m.faceCount,
          thumb: (thumbs.find((t) => t.width >= 400) || thumbs[0])?.url, note: readKey("SKETCHFAB_TOKEN", "sketchfab-token") ? "" : "download needs a token" });
      }
    }
    return out.slice(0, limit * 2);
  },
};

// ---------------------------------------------------------------- textures: ambientCG (CC0)
export const ambientcg = {
  rung: 2, name: "ambientcg", textures: true,
  async search(root, query, { limit = 4 } = {}) {
    const j = await getJson(`https://ambientcg.com/api/v2/full_json?q=${encodeURIComponent(query.words.slice(0, 3).join(" "))}&type=Material&limit=${limit}`);
    return (j.foundAssets || []).map((a) => ({ source: "ambientcg", id: a.assetId, name: a.displayName || a.assetId, url: `https://ambientcg.com/view?id=${a.assetId}`, licence: "CC0 1.0", author: "ambientCG", thumb: a.previewImage?.["256-PNG"] || a.previewImage?.["128-PNG"] }));
  },
};

export const ADAPTERS = [vanilla, smithsonian, sketchfab, ambientcg];

// Run every adapter; one that fails (no network, a changed API) reports and does not stop the others.
export async function searchAll(root, query, opts = {}, adapters = ADAPTERS) {
  const out = {};
  for (const a of adapters) {
    try { out[a.name] = await a.search(root, query, opts); } catch (e) { out[a.name] = []; console.log(`  ${a.name}: skipped (${e.message})`); }
  }
  return out;
}
