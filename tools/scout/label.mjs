// tools/scout/label.mjs - the scout's overview-top picture with the mapped buildings drawn on it (#2897).
//
//   tools\cli scout label <slug> [--view m]    img/overview-top-labelled.jpg beside img/overview-top.jpg
//
// A reviewer checks a town mapping (docs/everon/places/<slug>/mapping.json) against the overhead picture, but the plain
// picture does not say which roof is b01 or b14. The overlay is drawn from dossier.json, not from the picture: each
// mapped building's footprint outline and id, the slot names it takes, and its alternatives, projected onto the top camera.
//
// The camera (tools/visual/look.mjs shotsFor "top", the plugin's Place): above the place, `max(30, 2.2 x view)` m over the
// ground, looking straight down, north up and east to the right, 60 degrees of vertical field, a 16:9 frame. A point on
// the ground therefore lands at a fixed number of pixels per metre; a roof that stands 8 m up shifts a few metres out from
// the centre, and the outline (drawn at ground level) sits a little inside it.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

export const TOP_VERTICAL_FOV_DEG = 60;
export const DEFAULT_VIEW = 60;
export const LABELLED_FILE = "img/overview-top-labelled.jpg";
export const LABELLED_CAPTION = "from above, north up, the mapped buildings labelled";
// The contact-sheet writer (agent-box/shots-image.ps1 `sheet`, used by keepPicture) leaves a 4 px gap round the shot.
const GAP = 4;
const CHAR_W = 8.3;
const LABEL_H = 15;

export const topHeight = (view) => Math.max(30, view * 2.2);

// Pure: the picture's frame, { x, y, w, h }: the 16:9 shot inside the sheet's gap.
export function topFrame(imgW) {
  const w = imgW - GAP * 2;
  return { x: GAP, y: GAP, w, h: Math.round((w * 9) / 16) };
}

// Pure: world (x, z) -> picture pixel { x, y }, for the top shot of an area centred on `centre` with look radius `view`.
export function topProjector(centre, view, imgW) {
  const fr = topFrame(imgW);
  const scale = fr.h / (2 * topHeight(view) * Math.tan((TOP_VERTICAL_FOV_DEG / 2) * (Math.PI / 180)));
  const project = (x, z) => ({ x: fr.x + fr.w / 2 + (x - centre.x) * scale, y: fr.y + fr.h / 2 - (z - centre.z) * scale });
  project.scale = scale;
  project.frame = fr;
  return project;
}

// Pure: the buildings a mapping points at, { slots: Map id -> [slot names], alts: Map id -> [slot names] }. A none slot
// points at nothing.
export function mappedBuildings(mapping) {
  const slots = new Map();
  const alts = new Map();
  for (const [slot, s] of Object.entries(mapping?.slots ?? {})) {
    if (!s || s.none) continue;
    if (s.building) slots.set(s.building, [...(slots.get(s.building) ?? []), slot]);
    for (const a of s.alternatives ?? []) if (a?.building) alts.set(a.building, [...(alts.get(a.building) ?? []), slot]);
  }
  return { slots, alts };
}

const overlaps = (a, b) => a.x < b.x + b.w + 2 && b.x < a.x + a.w + 2 && a.y < b.y + b.h + 2 && b.y < a.y + a.h + 2;
const r1 = (n) => Math.round(n * 10) / 10;

// Pure: what to draw. -> { polys: [{ building, kind, pts }], labels: [{ building, kind, text, px, py, lx, ly, w, h }],
// offFrame: [ids], scale, frame, view }. A label sits right of its footprint and steps down until it clears the ones before.
export function labelPlan(dossier, mapping, { imgW = 1280, view } = {}) {
  const v = view ?? dossier.generated?.view ?? DEFAULT_VIEW;
  const project = topProjector({ x: dossier.place.x, z: dossier.place.z }, v, imgW);
  const fr = project.frame;
  const { slots, alts } = mappedBuildings(mapping);
  const byId = new Map(dossier.buildings.map((b) => [b.id, b]));
  const wanted = [...slots.keys()].map((id) => ({ id, kind: "slot" }));
  for (const id of alts.keys()) if (!slots.has(id)) wanted.push({ id, kind: "alt" });
  const plan = { polys: [], labels: [], offFrame: [], scale: project.scale, frame: fr, view: v };
  const placed = [];
  const inside = (p) => p.x >= fr.x && p.x <= fr.x + fr.w && p.y >= fr.y && p.y <= fr.y + fr.h;
  for (const { id, kind } of wanted) {
    const b = byId.get(id);
    if (!b) {
      plan.offFrame.push(id);
      continue;
    }
    const c = project(b.footprint?.cx ?? b.x, b.footprint?.cz ?? b.z);
    if (!inside(c)) {
      plan.offFrame.push(id);
      continue;
    }
    const corners = b.footprint?.corners?.length ? b.footprint.corners : [[b.x - 1, b.z - 1], [b.x + 1, b.z - 1], [b.x + 1, b.z + 1], [b.x - 1, b.z + 1]];
    plan.polys.push({ building: id, kind, pts: corners.map(([x, z]) => { const p = project(x, z); return [r1(p.x), r1(p.y)]; }) });
    const text = kind === "slot" ? `${id} ${slots.get(id).join(", ")}` : `${id} alt: ${alts.get(id).join(", ")}`;
    const w = text.length * CHAR_W + 8;
    let lx = c.x + 12;
    if (lx + w > fr.x + fr.w) lx = c.x - 12 - w;
    let ly = c.y - LABEL_H / 2;
    const box = () => ({ x: lx, y: ly, w, h: LABEL_H });
    for (let n = 0; n < 40 && placed.some((r) => overlaps(box(), r)); n++) ly += LABEL_H + 2;
    ly = Math.max(fr.y, Math.min(ly, fr.y + fr.h - LABEL_H));
    placed.push(box());
    plan.labels.push({ building: id, kind, text, px: r1(c.x), py: r1(c.y), lx: r1(lx), ly: r1(ly), w, h: LABEL_H });
  }
  return plan;
}

// Draws the plan on `plain` and writes `out` (a JPEG). `image` is agent-box/shots.mjs's image(). -> { ok, plan }; ok is
// false when the mapping names no building that is in the picture.
export function labelTopPicture({ dossier, mapping, plain, out, image, imgW = 1280, view, quality = 85 }) {
  const plan = labelPlan(dossier, mapping, { imgW, view });
  if (!plan.labels.length) return { ok: false, plan };
  mkdirSync(dirname(out), { recursive: true });
  const fr = plan.frame;
  image({
    op: "label",
    in: plain,
    out,
    quality,
    polys: plan.polys,
    marks: plan.labels,
    bar: { x: fr.x + 14, y: fr.y + fr.h - 18, len: r1(50 * plan.scale), text: "50 m" },
    north: { x: fr.x + fr.w - 24, y: fr.y + 40 },
  });
  return { ok: true, plan };
}

// Puts the labelled picture in a dossier's picture list, right after the plain top view; idempotent.
export function withLabelledPicture(dossier) {
  dossier.pictures = dossier.pictures.filter((p) => p.file !== LABELLED_FILE);
  const i = dossier.pictures.findIndex((p) => p.file === "img/overview-top.jpg");
  dossier.pictures.splice(i < 0 ? 0 : i + 1, 0, { role: "overview", file: LABELLED_FILE, caption: LABELLED_CAPTION });
  return dossier;
}

// The command: label a place folder's overview-top picture and list the result in its dossier.json and dossier.md.
export async function labelPlace(root, slug, { view, log = console.log } = {}) {
  const dir = join(root, "docs", "everon", "places", slug);
  const dj = join(dir, "dossier.json");
  const mj = join(dir, "mapping.json");
  const plain = join(dir, "img", "overview-top.jpg");
  for (const f of [dj, mj, plain]) {
    if (!existsSync(f)) {
      log(`SUMMARY scout label fail ${slug}: ${f.slice(root.length + 1)} is missing (scout the place and map it first)`);
      return 1;
    }
  }
  const dossier = JSON.parse(readFileSync(dj, "utf8"));
  const mapping = JSON.parse(readFileSync(mj, "utf8"));
  const { image } = await import("../../agent-box/shots.mjs");
  const r = labelTopPicture({ dossier, mapping, plain, out: join(dir, LABELLED_FILE), image, view });
  if (!r.ok) {
    log(`SUMMARY scout label ok ${slug}: the mapping names no building inside the picture, nothing to label${r.plan.offFrame.length ? ` (outside: ${r.plan.offFrame.join(", ")})` : ""}`);
    return 0;
  }
  const { renderDossier } = await import("./render.mjs");
  withLabelledPicture(dossier);
  writeFileSync(dj, JSON.stringify(dossier, null, 1) + "\n");
  writeFileSync(join(dir, "dossier.md"), renderDossier(dossier));
  log(`SUMMARY scout label ok ${slug}: ${r.plan.labels.length} building(s) labelled${r.plan.offFrame.length ? `, outside the picture: ${r.plan.offFrame.join(", ")}` : ""}; ${LABELLED_FILE}`);
  return 0;
}
