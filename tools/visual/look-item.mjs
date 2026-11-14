// `tools\cli look item <prefab> ...`: the two pictures agents used to take on a session, a dropped item
// and an inventory preview, without a session. Pure planning here; look.mjs runs the hidden Workbench.
//
//   ground     the item posed the way the inventory drops it: the model axis `RestingUP` names faces up,
//              `RestingAdditiveOffsetLS` nudges it, its lowest point on the ground (docs/asset-pipeline/
//              states.md "Ground": a drop is placed, not a physics fall). Then a close-up.
//   inventory  the item frozen in the air and a camera that has the prefab's own PreviewRenderAttributes:
//              orbit angles, distance, FOV and offset. The 3D view the engine's picture is made from; not
//              the inventory UI (no slot frame, no UI lighting), so a PR picture of the sheet is still a session.
//
// Values come from the prefab file itself. A value the prefab inherits and does not state is read as the
// engine default and said so, so a wrong-looking picture points at the right file.
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";

export const ITEM_STATES = ["ground", "inventory"];
export const DEFAULT_PREVIEW = { angles: [-15, -45, 0], distance: 0.6, fov: 20, offset: [0, 0, 0] };
export const RESTING = ["Up", "Down", "Left", "Right", "Forward", "Backward"];

// Where the item's own axes land in the world when the model axis `up` faces +y: [image of x, of y, of z].
const BASIS = {
  Up: [[1, 0, 0], [0, 1, 0], [0, 0, 1]],
  Down: [[1, 0, 0], [0, -1, 0], [0, 0, -1]],
  Right: [[0, 1, 0], [-1, 0, 0], [0, 0, 1]],
  Left: [[0, -1, 0], [1, 0, 0], [0, 0, 1]],
  Forward: [[1, 0, 0], [0, 0, -1], [0, 1, 0]],
  Backward: [[1, 0, 0], [0, 0, 1], [0, -1, 0]],
};

export const restingBasis = (up) => BASIS[up] ?? null;

const vec3 = (s) => {
  const m = String(s ?? "").trim().split(/\s+/).map(Number);
  return m.length === 3 && m.every(Number.isFinite) ? m : null;
};
const read = (text, key) => {
  const m = new RegExp(`^\\s*${key}\\s+([^\\n]+?)\\s*$`, "m").exec(text);
  return m ? m[1].replace(/^"|"$/g, "") : null;
};

// What the prefab file states. `stated` lists the keys it did not carry (inherited, so defaults are used).
export function itemAttributes(text) {
  const missing = [];
  const take = (key, parse, fallback) => {
    const raw = read(text, key);
    const v = raw === null ? null : parse(raw);
    if (v === null || (typeof v === "number" && !Number.isFinite(v))) {
      missing.push(key);
      return fallback;
    }
    return v;
  };
  const up = take("RestingUP", (s) => (RESTING.includes(s) ? s : null), "Up");
  return {
    missing,
    ground: {
      up,
      offset: take("RestingAdditiveOffsetLS", vec3, [0, 0, 0]),
      rotationIgnored: (vec3(read(text, "RestingAdditiveRotationLS")) ?? [0, 0, 0]).some((v) => Math.abs(v) > 1e-6),
    },
    preview: {
      angles: take("CameraOrbitAngles", vec3, DEFAULT_PREVIEW.angles),
      distance: take("CameraDistanceToItem", Number, DEFAULT_PREVIEW.distance),
      fov: take("FOV", Number, DEFAULT_PREVIEW.fov),
      offset: take("CameraOffset", vec3, DEFAULT_PREVIEW.offset),
    },
  };
}

// The fixed-width `item` plan line the plugin reads (tools/visual/wb/LF_VisualShotsPlugin.c).
// item NAME MODE X Z  ix iy iz jx jy jz kx ky kz  OX OY OZ  PITCH YAW ROLL DIST FOV  CX CY CZ  PREFAB
export function itemLine({ name, mode, x, z, attrs, prefab }) {
  const basis = restingBasis(attrs.ground.up) ?? BASIS.Up;
  const [ox, oy, oz] = attrs.ground.offset;
  // The nudge is in the item's own axes: into the world through the basis.
  const world = [0, 1, 2].map((i) => ox * basis[0][i] + oy * basis[1][i] + oz * basis[2][i]);
  const p = attrs.preview;
  const f = (n) => String(+Number(n).toFixed(4));
  return ["item", name, mode, x, z, ...basis.flat(), ...world, ...p.angles, p.distance, p.fov, ...p.offset, prefab]
    .map((v) => (typeof v === "number" ? f(v) : v))
    .join(" ");
}

// "{GUID}Prefabs/X.et" | "Prefabs/X.et" | an asset slug -> { file, ref, leaf }, or throws a one-line reason.
export function resolvePrefab(root, arg) {
  const addons = join(root, "addons");
  const rel = arg.replace(/^\{[0-9A-Fa-f]{16}\}/, "");
  const dirs = existsSync(addons) ? readdirSync(addons, { withFileTypes: true }).filter((d) => d.isDirectory()).map((d) => d.name) : [];
  let file = null;
  if (/\.et$/.test(rel)) file = dirs.map((d) => join(addons, d, rel)).find(existsSync) ?? null;
  else {
    const specFile = join(root, "tools", "assets", arg, "spec.json");
    if (existsSync(specFile)) {
      const spec = JSON.parse(readFileSync(specFile, "utf8"));
      const path = spec.states?.prefab || `${spec.prefab_dir}/${spec.name}.et`;
      file = dirs.map((d) => join(addons, d, path)).find(existsSync) ?? null;
    }
  }
  if (!file) throw new Error(`no prefab for "${arg}" (give {GUID}path, a path under addons/<addon>/, or an asset slug)`);
  const meta = `${file}.meta`;
  const guid = existsSync(meta) ? /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1] : null;
  if (!guid) throw new Error(`no GUID in ${meta}`);
  const path = file.slice(addons.length + 1).split(/[\\/]/).slice(1).join("/");
  return { file, ref: `{${guid}}${path}`, leaf: path.split("/").pop().replace(/\.et$/, "") };
}

const safe = (s) => String(s).replace(/[^A-Za-z0-9_-]/g, "_");

// The pseudo-areas and shots look.mjs feeds its normal pipeline (lock, Workbench, sheet, flags, index.json).
export function itemPlan(root, prefabs, { states = ITEM_STATES, at }) {
  const bad = states.filter((s) => !ITEM_STATES.includes(s));
  if (bad.length) throw new Error(`unknown state ${bad.join(",")}; states are ${ITEM_STATES.join(", ")}`);
  const perArea = [];
  const areas = [];
  const notes = [];
  prefabs.forEach((arg, n) => {
    const r = resolvePrefab(root, arg);
    const attrs = itemAttributes(readFileSync(r.file, "utf8"));
    // Items stand 3 m apart along x so several prefabs in one run never touch.
    const x = at.x + n * 3;
    const area = { name: `item-${safe(r.leaf)}`, x, y: 0, z: at.z, radius: 3, front: 180 };
    areas.push(area);
    const shots = states.map((mode) => {
      const name = `${area.name}__${mode}`;
      const label = mode === "ground" ? `dropped, ${attrs.ground.up} up` : `inventory view ${attrs.preview.angles.join(" ")}, ${attrs.preview.distance} m, FOV ${attrs.preview.fov}`;
      return { name, area: area.name, kind: mode, label, line: itemLine({ name, mode, x, z: at.z, attrs, prefab: r.ref }) };
    });
    perArea.push({ area, shots });
    if (attrs.missing.length) notes.push(`${r.leaf}: not stated in the prefab file (inherited; engine defaults assumed): ${attrs.missing.join(", ")}`);
    if (attrs.ground.rotationIgnored) notes.push(`${r.leaf}: RestingAdditiveRotationLS is not applied to the dropped pose`);
  });
  return { areas, perArea, notes };
}
