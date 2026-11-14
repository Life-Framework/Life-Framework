// tools/validation/validate-ui-kit.mjs
// inputs: addons/**, docs/**
//
// A new player screen is built from UI kit parts (LF_UIScreen + LF_UIPart, themed by
// LF_UITheme), not from a new .layout with its own fonts and colours. Before the kit,
// each feature drew its own menu and no two looked alike; the sheets players like today
// (the R-89 form, the sign card, the ATM) are all kit screens.
//
// Fails when a .layout outside UI/Layouts/UIKit/ is added that is not listed in
// tools/validation/ui-kit-baseline.json:
//   "layouts"     the layouts that existed when the check landed (never grows)
//   "exceptions"  { "<path>": "<why this is not a kit screen>" } for a layout that is not a
//                 screen at all: a render-target face on a world prop, a HUD strip.
// An exception still has to use the theme's type: any font other than the two
// RobotoCondensed faces LF_UITheme names fails.
// docs/style-guide.md "Screens" has the rules; `tools\cli feature new <Area> <Name> --kind ui`
// writes a kit sheet.
// A UI kit script (other than the theme's own) that writes a colour literal fails too (#966).
// red-proof: tools/validation/agent-quality-selftest.mjs asserts a new layout, a new layout
// with an exception, an exception with a foreign font, and a colour literal in a kit script.

import { existsSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export const BASELINE = "tools/validation/ui-kit-baseline.json";
export const THEME_FONTS = ["{EABA4FE9D014CCEF}", "{3E7733BAC8C831F6}"];
const KIT = "/UI/Layouts/UIKit/";

export function fontsOf(text) {
  return [...new Set([...String(text).matchAll(/"(\{[0-9A-F]{16}\})[^"]*\.fnt"/g)].map((m) => m[1]))];
}

// layouts: [{ path, text }]. Returns the failure lines.
export function problems(layouts, baseline) {
  const known = new Set(baseline.layouts ?? []);
  const exceptions = baseline.exceptions ?? {};
  const out = [];
  for (const l of layouts) {
    if (l.path.includes(KIT) || known.has(l.path)) continue;
    const why = exceptions[l.path];
    if (!why || String(why).trim().length < 15) {
      out.push(`${l.path}: a new layout outside the UI kit. Build the screen from LF_UIPart parts (tools\\cli feature new <Area> <Name> --kind ui), or, if this is not a screen (a face on a world prop, a HUD strip), add it to "exceptions" in ${BASELINE} with the reason`);
      continue;
    }
    const foreign = fontsOf(l.text).filter((f) => !THEME_FONTS.includes(f));
    if (foreign.length) out.push(`${l.path}: uses font ${foreign.join(", ")}; the theme's faces are LF_UITheme.FONT and FONT_REGULAR (docs/style-guide.md)`);
  }
  return out;
}

// #966: the kit's colours are theme tokens (Configs/UI/LF_UITheme.conf, LF_UIThemeConfig). A kit
// script other than the theme's own two files must not write a colour literal, so a pack that
// overrides the theme restyles every sheet. A fully transparent colour (hiding a glyph) is no colour.
export const THEME_SCRIPTS = ["LF_UITheme.c", "LF_UIThemeConfig.c"];
const LITERAL_COLOR = /Color[.]FromSRGBA?[(] *[0-9]/;
const TRANSPARENT = /Color[.]FromSRGBA[(] *0 *, *0 *, *0 *, *0 *[)]/g;
const NEWLINE = String.fromCharCode(10);

// scripts: [{ path, text }] of addons/**/Feature/UIKit/*.c. Returns the failure lines.
export function colorProblems(scripts) {
  const out = [];
  for (const s of scripts) {
    if (THEME_SCRIPTS.some((n) => s.path.endsWith(`/${n}`))) continue;
    s.text.split(NEWLINE).forEach((line, i) => {
      const code = line.split("//")[0].replace(TRANSPARENT, "");
      if (LITERAL_COLOR.test(code)) out.push(`${s.path}:${i + 1}: a colour literal in a UI kit script; add a token to LF_UIThemeRecord / Configs/UI/LF_UITheme.conf and read it through LF_UITheme so a pack theme can override it (docs/style-guide.md)`);
    });
  }
  return out;
}

export function scanScripts(root) {
  const out = [];
  const addons = join(root, "addons");
  if (!existsSync(addons)) return out;
  for (const addon of readdirSync(addons)) {
    const dir = join(addons, addon, "Scripts", "Game", "Feature", "UIKit");
    if (!existsSync(dir)) continue;
    for (const name of readdirSync(dir)) {
      if (name.endsWith(".c")) out.push({ path: `addons/${addon}/Scripts/Game/Feature/UIKit/${name}`, text: readFileSync(join(dir, name), "utf8") });
    }
  }
  return out;
}

function walk(root, rel, out) {
  for (const name of readdirSync(join(root, rel))) {
    const r = `${rel}/${name}`;
    if (statSync(join(root, r)).isDirectory()) walk(root, r, out);
    else if (name.endsWith(".layout")) out.push({ path: r, text: readFileSync(join(root, r), "utf8") });
  }
  return out;
}

export function scan(root) {
  const out = [];
  const addons = join(root, "addons");
  if (!existsSync(addons)) return out;
  for (const addon of readdirSync(addons)) {
    if (existsSync(join(addons, addon, "UI"))) walk(root, `addons/${addon}/UI`, out);
  }
  return out.sort((a, b) => a.path.localeCompare(b.path));
}

const isMain = process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];
if (isMain) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const file = join(root, BASELINE);
  const layouts = scan(root);
  if (process.argv.includes("--write-baseline")) {
    if (existsSync(file)) {
      console.log("validate-ui-kit: the baseline exists and never grows; add an exception with a reason instead");
      process.exit(1);
    }
    const list = layouts.filter((l) => !l.path.includes(KIT)).map((l) => l.path);
    writeFileSync(file, `${JSON.stringify({ note: "Layouts outside the UI kit on the day the check landed (validate-ui-kit.mjs). 'layouts' never grows; a new non-screen layout goes in 'exceptions' with a reason.", layouts: list, exceptions: {} }, null, 2)}\n`);
    console.log(`validate-ui-kit: baseline written (${list.length})`);
    process.exit(0);
  }
  const baseline = existsSync(file) ? JSON.parse(readFileSync(file, "utf8")) : { layouts: [], exceptions: {} };
  const bad = [...problems(layouts, baseline), ...colorProblems(scanScripts(root))];
  if (bad.length) {
    console.log("validate-ui-kit: FAIL");
    for (const b of bad) console.log(`  ${b}`);
    process.exit(1);
  }
  console.log(`validate-ui-kit: OK (${layouts.length} layouts; ${(baseline.layouts ?? []).length} predate the kit rule, ${Object.keys(baseline.exceptions ?? {}).length} exception(s))`);
}
