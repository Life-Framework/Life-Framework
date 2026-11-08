// A point-anchored FrameWidgetSlot stores its rectangle twice: Position/Size and
// Offset*. The engine reads the Offset* pair, so they must agree:
//   OffsetRight = -(OffsetLeft + SizeX), OffsetBottom = -(OffsetTop + SizeY).
// EL_NotificationToast.layout had OffsetBottom -92 with OffsetTop 140, a height
// of -48: no toast ever appeared on screen (#157). Hand-edited layouts drift
// this way; the Workbench layout editor writes the pairs consistently.
//
// red-proof: on main 2026-11-08 this printed
//   FAIL addons/LifeFramework/UI/Layouts/Notifications/EL_NotificationToast.layout:3 NotificationToast: Offset* gives 444 x -48, Size* says 420 x 92
// and exited 1; after the layout fix it prints OK.
import { readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const ui = join(root, "addons", "LifeFramework", "UI");

function walk(dir, out = []) {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (name.endsWith(".layout")) out.push(p);
  }
  return out;
}

export function checkLayout(text) {
  const lines = text.replace(/\r\n/g, "\n").split("\n");
  const fails = [];
  let name = "";
  for (let i = 0; i < lines.length; i++) {
    const n = /^\s*Name "([^"]*)"/.exec(lines[i]);
    if (n) name = n[1];
    if (!/^\s*Slot FrameWidgetSlot\b.*\{\s*$/.test(lines[i])) continue;
    const v = {};
    let j = i + 1;
    for (; j < lines.length && !/^\s*\}\s*$/.test(lines[j]); j++) {
      const m = /^\s*(\w+)\s+(.+?)\s*$/.exec(lines[j]);
      if (m) v[m[1]] = m[2];
    }
    const anchor = (v.Anchor || "0 0 0 0").split(/\s+/).map(Number);
    const pointX = anchor[0] === anchor[2];
    const pointY = anchor[1] === anchor[3];
    const num = (k) => Number(v[k] ?? 0);
    const w = -num("OffsetRight") - num("OffsetLeft");
    const h = -num("OffsetBottom") - num("OffsetTop");
    const badX = pointX && v.SizeX !== undefined && Math.abs(w - num("SizeX")) > 0.01;
    const badY = pointY && v.SizeY !== undefined && Math.abs(h - num("SizeY")) > 0.01;
    if (badX || badY) {
      const fmt = (x) => Number(x.toFixed(3));
      fails.push({ line: i + 1, name, msg: `Offset* gives ${fmt(w)} x ${fmt(h)}, Size* says ${num("SizeX")} x ${num("SizeY")}` });
    }
    i = j;
  }
  return fails;
}

function main() {
  const fails = [];
  for (const f of walk(ui)) {
    for (const x of checkLayout(readFileSync(f, "utf8"))) {
      fails.push(`${relative(root, f).split("\\").join("/")}:${x.line} ${x.name}: ${x.msg}`);
    }
  }
  for (const f of fails) console.log(`FAIL ${f}`);
  console.log(fails.length ? `validate-layout-slots: FAILED (${fails.length})` : "validate-layout-slots: OK");
  return fails.length ? 1 : 0;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) process.exit(main());
