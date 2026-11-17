// The writer half of the pub's decor (a copy of tools/buildings/bank/decor-lib.mjs): turns { items, lights } into child entities of a furniture
// or kit prefab, and swaps them in without touching the vanilla props around them. Everything written is a base-game
// prop or a plain LightEntity (no shadow, no physics). Entity IDs start with the block's prefix, so a re-run
// removes its own old block and writes the new one; the same layout always writes the same bytes.
import { createHash } from "node:crypto";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";
import { gameDb } from "../pipeline/gamedb.mjs";

const num = (n) => String(+(+n).toFixed(3));
export const hex16 = (prefix, s) => prefix + createHash("sha1").update(s).digest("hex").slice(0, 12).toUpperCase();

// items: { path, x, y, z, yaw?, scale?, angles? }; lights: { x, y, z, color?, lv?, radius? }
export async function renderBlock(root, prefix, { items, lights = [] }) {
  const vfs = await gameVfs();
  const db = gameDb();
  if (!db) throw new Error("the game's resource database is not readable here (ENFUSION_GAME_PATH)");
  const memo = new Map();
  const info = (given) => {
    if (memo.has(given)) return memo.get(given);
    // a path may come as {GUID}path when the game database does not index it by path
    const pre = /^\{([0-9A-F]{16})\}(.+)$/.exec(given);
    const path = pre ? pre[2] : given;
    const g = pre ? pre[1] : db.guidOf(path);
    if (!g) throw new Error(`no GUID for ${path} in the game's resource database`);
    const text = readText(root, vfs, path);
    const cls = (text && /^(\w+)/.exec(text.trimStart()))?.[1];
    if (!cls) throw new Error(`cannot read the root class of ${path}`);
    const v = { ref: `{${g}}${path}`, cls };
    memo.set(given, v);
    return v;
  };
  const rows = [];
  items.forEach((o, i) => {
    const { ref, cls } = info(o.path);
    const l = [`  ${cls} : "${ref}" {`, `   ID "${hex16(prefix, `item|${i}|${o.path}`)}"`];
    if (o.scale && o.scale !== 1) l.push(`   scale ${num(o.scale)}`);
    l.push(`   coords ${num(o.x)} ${num(o.y)} ${num(o.z)}`);
    if (o.angles) l.push(`   angles ${o.angles}`);
    else if (o.yaw) l.push(`   angleY ${num(o.yaw)}`);
    l.push("  }");
    rows.push(l.join("\n"));
  });
  lights.forEach((o, i) => {
    rows.push(
      [
        "  LightEntity {",
        `   ID "${hex16(prefix, `light|${i}`)}"`,
        `   coords ${num(o.x)} ${num(o.y)} ${num(o.z)}`,
        `   Color ${o.color ?? "1 0.88 0.68 1"}`,
        `   LV ${o.lv ?? 6.5}`,
        "   ClipEV -10",
        "   CastShadow 0",
        `   Radius ${o.radius ?? 6}`,
        "   NearPlane 0.3",
        "   LensFlareType Disabled",
        "  }",
      ].join("\n"),
    );
  });
  return rows.join("\n");
}

// Remove every child entity whose ID starts with `prefix`, then append `block` as the last children.
export function swapBlock(text, prefix, block) {
  const lines = text.split("\n");
  const out = [];
  for (let i = 0; i < lines.length; i++) {
    const open = /^ {2}\w+(?: : "[^"]+")? \{$/.exec(lines[i]);
    const id = open && /^ {3}ID "([0-9A-F]+)"$/.exec(lines[i + 1] ?? "")?.[1];
    if (id && id.startsWith(prefix)) {
      while (i < lines.length && lines[i] !== "  }") i++;
      continue;
    }
    out.push(lines[i]);
  }
  const body = out.join("\n");
  const at = body.lastIndexOf("\n }\n}");
  if (at < 0) throw new Error("the prefab has no child list");
  return `${body.slice(0, at)}\n${block}${body.slice(at)}`;
}

// The child entities whose ID starts with `prefix`, as text (what swapBlock would remove): prune.mjs keeps decor.mjs's
// block when it rewrites the vanilla set.
export function blockOf(text, prefix) {
  const lines = text.split("\n");
  const out = [];
  for (let i = 0; i < lines.length; i++) {
    const open = /^ {2}\w+(?: : "[^"]+")? \{$/.exec(lines[i]);
    const id = open && /^ {3}ID "([0-9A-F]+)"$/.exec(lines[i + 1] ?? "")?.[1];
    if (!id || !id.startsWith(prefix)) continue;
    while (i < lines.length && lines[i] !== "  }") out.push(lines[i++]);
    out.push("  }");
  }
  return out.join("\n");
}
