// The "Live now" section of the World of Everon page (docs/architecture/everon-world.md section 6).
//
// Reads the flagship header's `m_aLF_LiveStates` and the place dossiers in docs/everon/places/<slug>/, and
// renders one block per place whose state is live: a public picture, then one line. Node built-ins only, so
// tools/validation/validate-public-bible.mjs can import it and check the same text the site prints.
//
// A place's state is `state` in its mapping.json (written when the place's layers are baked). A place
// with no `state` is not shown. The site is stricter than the game here: the game treats an empty
// `m_aLF_LiveStates` as "every state is live" (LF_PlaceLiveState), the page treats it as "nothing is live",
// so the section stays empty until a state has shipped (D4). Spoilers never come through this file: it
// prints only the dossier's picture caption, its counts, and an optional `mapping.line`.

import fs from "node:fs";
import path from "node:path";

export const HEADER = "addons/LF_Everon1989/Missions/LF_Everon1989.conf";
export const PLACES = "docs/everon/places";

/** The header's m_aLF_LiveStates as numbers; [] when the header lists none or does not exist. */
export function liveStates(root) {
  const f = path.join(root, HEADER);
  if (!fs.existsSync(f)) return [];
  const m = fs.readFileSync(f, "utf8").match(/m_aLF_LiveStates\s*\{([^}]*)\}/);
  return m ? m[1].split(/[\s,]+/).filter(Boolean).map(Number).filter(Number.isFinite) : [];
}

const readJson = (f) => {
  try {
    return JSON.parse(fs.readFileSync(f, "utf8"));
  } catch {
    return null;
  }
};

// "saint-philippe" -> "Saint-Philippe"
const titleOf = (slug) => slug.split("-").map((w) => w.charAt(0).toUpperCase() + w.slice(1)).join("-");

function pickPicture(dir, dossier) {
  const pics = (dossier.pictures || []).filter((p) => p.role === "overview" && p.file && fs.existsSync(path.join(dir, p.file)));
  return pics.find((p) => /wide/.test(p.file)) || pics[0] || null;
}

function lineOf(mapping, dossier) {
  if (mapping.line) return String(mapping.line);
  const s = dossier.summary || {};
  if (!s.buildings) return "";
  const open = s.enterable ? `, ${s.enterable} you can walk into` : "";
  return `${s.buildings} buildings${open}.`;
}

/** The live places, in slug order: { slug, title, state, line, picture: { abs, ext, caption } | null }. */
export function collectLivePlaces(root) {
  const states = liveStates(root);
  const base = path.join(root, PLACES);
  if (!states.length || !fs.existsSync(base)) return [];
  const out = [];
  for (const slug of fs.readdirSync(base).sort()) {
    const dir = path.join(base, slug);
    const mapping = readJson(path.join(dir, "mapping.json"));
    const dossier = readJson(path.join(dir, "dossier.json"));
    if (!mapping || !dossier || !Number.isFinite(mapping.state) || !states.includes(mapping.state)) continue;
    const pic = pickPicture(dir, dossier);
    out.push({
      slug,
      title: mapping.title ? String(mapping.title) : titleOf(slug),
      state: mapping.state,
      line: lineOf(mapping, dossier),
      picture: pic ? { abs: path.join(dir, pic.file), ext: path.extname(pic.file), caption: String(pic.caption || "") } : null,
    });
  }
  return out;
}

/**
 * The section's markdown, "" when no state is live. With `publicDir` each picture is copied there and linked
 * as `${urlBase}/<slug><ext>`; without it the picture is left out (the validator only reads the text).
 */
export function liveNowMarkdown(root, { publicDir, urlBase = "/everon-live" } = {}) {
  const places = collectLivePlaces(root);
  if (!places.length) return "";
  const states = liveStates(root).join(", ");
  const lines = ["## Live now", "", `What a player can visit today. Generated from the flagship header's live states (${states}) and the place dossiers.`, ""];
  for (const p of places) {
    lines.push(`### ${p.title}`, "");
    if (p.picture) {
      lines.push(`![${p.title}: ${p.picture.caption}](${publicDir ? `${urlBase}/${p.slug}${p.picture.ext}` : p.picture.abs})`, "");
      if (publicDir) {
        fs.mkdirSync(publicDir, { recursive: true });
        fs.copyFileSync(p.picture.abs, path.join(publicDir, `${p.slug}${p.picture.ext}`));
      }
    }
    if (p.line) lines.push(p.line, "");
  }
  return lines.join("\n");
}

/** Puts the section before the page's "## Contents" (or at the end); a page with no live place is unchanged. */
export function withLiveNow(body, section) {
  if (!section) return body;
  const i = body.search(/^## Contents\s*$/m);
  return i < 0 ? `${body.replace(/\s+$/, "")}\n\n${section}` : `${body.slice(0, i)}${section}\n${body.slice(i)}`;
}
