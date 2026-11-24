// The handbook tour as flow lines, read from the handbook configs instead of typed by hand (#4793).
// ui-handbook-everon used to click HandbookPage1..6 and wait for seven hard-coded titles; adding pages
// (#4736 made it eleven) shifted every index. A flow line
//
//   @handbook-tour LF_Everon1989 shot:parcel=handbook_everon_parcel
//
// expands (harness.mjs readFlow) to one open-read-back-to-contents round per page, in the order the game
// lists them: the core `LF_HandbookConfig` pages, then the pack overlay's add / patch / remove rows, sorted
// by m_iOrder (stable, like LF_Handbook.Collect). Row N is `HandbookPage<N>_Action`, its title the pack's
// en_us string (pack row wins over core, like the game). `shot:<page id>=<name>` takes a UI shot on that page.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..");
const CORE_CONF = "addons/LifeFramework/Configs/Rules/LF_Handbook.conf";
const CORE_STRINGS = "addons/LifeFramework/Language/everonlife_localization.st";

// One LF_HandbookPage block per `LF_HandbookPage "{GUID}" {` line; only the scalar fields matter here.
export function parseHandbookConf(text) {
  const pages = [];
  let page = null;
  for (const line of text.split(/\r?\n/)) {
    if (/^\s*LF_HandbookPage\b/.test(line)) {
      page = { op: "add", id: "", titleKey: "", order: -1 };
      pages.push(page);
      continue;
    }
    if (!page) continue;
    let m;
    if ((m = /^\s*m_sOp\s+"([^"]*)"/.exec(line))) page.op = m[1] || "add";
    else if ((m = /^\s*m_sId\s+"([^"]*)"/.exec(line))) page.id = m[1];
    else if ((m = /^\s*m_sTitleKey\s+"#?([^"]*)"/.exec(line))) page.titleKey = m[1];
    else if ((m = /^\s*m_iOrder\s+(-?\d+)/.exec(line))) page.order = Number(m[1]);
  }
  return pages;
}

// Id -> Target_en_us from a .st string table.
export function parseStrings(text) {
  const map = new Map();
  let id = "";
  for (const line of text.split(/\r?\n/)) {
    let m;
    if ((m = /^\s*Id\s+"([^"]*)"/.exec(line))) id = m[1];
    else if (id && (m = /^\s*Target_en_us\s+"(.*)"\s*$/.exec(line))) {
      map.set(id, m[1]);
      id = "";
    }
  }
  return map;
}

// The pages a player reads, in list order: [{ id, title }].
export function handbookPages(coreConf, overlayConfs, strings) {
  const pages = [];
  const apply = (row) => {
    const at = pages.findIndex((p) => p.id === row.id);
    if (row.op === "remove") {
      if (at >= 0) pages.splice(at, 1);
    } else if (row.op === "patch") {
      if (at < 0) return;
      if (row.titleKey) pages[at].titleKey = row.titleKey;
      if (row.order >= 0) pages[at].order = row.order;
    } else if (at >= 0) {
      pages[at] = { ...row }; // replace
    } else {
      pages.push({ ...row });
    }
  };
  for (const row of coreConf) apply(row);
  for (const conf of overlayConfs) for (const row of conf) apply(row);
  const key = (p) => (p.order < 0 ? Number.MAX_SAFE_INTEGER : p.order);
  // Array.prototype.sort is stable, so equal orders keep config order, as the game's insertion does.
  pages.sort((a, b) => key(a) - key(b));
  return pages.map((p) => ({ id: p.id, title: strings.get(p.titleKey) ?? "" }));
}

// Flow lines for the tour. `pages` as from handbookPages; `shots` maps page id -> shot name.
export function tourLines(pages, shots = {}) {
  const lines = [];
  pages.forEach((page, index) => {
    if (!page.title) throw new Error(`handbook page ${page.id}: no en_us title in the string tables`);
    lines.push(`click HandbookPage${index}_Action`, `wait-text ${page.title}`);
    if (shots[page.id]) lines.push(`shot ui ${shots[page.id]}`);
    lines.push("click HandbookContentsButton", "wait-text Pick a page to read.");
  });
  return lines;
}

// Expands `@handbook-tour <pack> [shot:<id>=<name> ...]` for a pack folder under addons/.
export function expandHandbookTour(line, root = ROOT) {
  const [, pack, ...opts] = line.trim().split(/\s+/);
  if (!pack || !/^[A-Za-z0-9_]+$/.test(pack)) throw new Error(`@handbook-tour needs a pack folder: ${line}`);
  const shots = {};
  for (const opt of opts) {
    const m = /^shot:([A-Za-z0-9_-]+)=([A-Za-z0-9_-]+)$/.exec(opt);
    if (!m) throw new Error(`@handbook-tour: bad option ${opt}`);
    shots[m[1]] = m[2];
  }
  const read = (rel) => readFileSync(join(root, rel), "utf8");
  const overlays = [];
  const packRules = join(root, "addons", pack, "Configs", "Rules");
  for (const file of ["LF_Handbook_Everon1989.conf"]) {
    if (existsSync(join(packRules, file))) overlays.push(parseHandbookConf(read(`addons/${pack}/Configs/Rules/${file}`)));
  }
  if (overlays.length === 0) throw new Error(`@handbook-tour: no handbook overlay in addons/${pack}/Configs/Rules`);
  const strings = parseStrings(read(CORE_STRINGS));
  const packStrings = join(root, "addons", pack, "Language", "lf_everon1989_localization.st");
  if (existsSync(packStrings)) for (const [k, v] of parseStrings(readFileSync(packStrings, "utf8"))) strings.set(k, v);
  const pages = handbookPages(parseHandbookConf(read(CORE_CONF)), overlays, strings);
  return tourLines(pages, shots);
}

export function selftest() {
  const core = parseHandbookConf(`LF_HandbookConfig {
 m_aPages {
  LF_HandbookPage "{A}" {
   m_sId "read"
   m_sTitleKey "#K_Read"
   m_iOrder 10
  }
  LF_HandbookPage "{B}" {
   m_sId "rules"
   m_sTitleKey "#K_Rules"
   m_iOrder 20
  }
 }
}`);
  const overlay = parseHandbookConf(`LF_HandbookConfig {
 m_aPages {
  LF_HandbookPage "{C}" {
   m_sOp "add"
   m_sId "work"
   m_sTitleKey "#K_Work"
   m_aBodyKeys {
    "#K_WorkBody"
   }
   m_iOrder 40
  }
  LF_HandbookPage "{D}" {
   m_sOp "add"
   m_sId "trades"
   m_sTitleKey "#K_Trades"
   m_iOrder 45
  }
  LF_HandbookPage "{E}" {
   m_sOp "patch"
   m_sId "rules"
   m_sTitleKey "#K_Law"
   m_iOrder 80
  }
  LF_HandbookPage "{F}" {
   m_sOp "add"
   m_sId "gone"
   m_sTitleKey "#K_Gone"
   m_iOrder 5
  }
  LF_HandbookPage "{G}" {
   m_sOp "remove"
   m_sId "gone"
  }
 }
}`);
  const strings = parseStrings(`    CustomStringTableItem "{1}" {
     Id "K_Read"
     Target_en_us "How to read"
    }
    CustomStringTableItem "{2}" {
     Id "K_Rules"
     Target_en_us "The rules"
    }
    CustomStringTableItem "{3}" {
     Id "K_Work"
     Target_en_us "Work in town"
    }
    CustomStringTableItem "{4}" {
     Id "K_Trades"
     Target_en_us "Trades"
    }
    CustomStringTableItem "{5}" {
     Id "K_Law"
     Target_en_us "The law and the permit"
    }`);
  const pages = handbookPages(core, [overlay], strings);
  const ids = pages.map((p) => p.id).join(",");
  if (ids !== "read,work,trades,rules") throw new Error(`handbookPages order: ${ids}`);
  if (pages[3].title !== "The law and the permit") throw new Error(`patch title: ${pages[3].title}`);
  const lines = tourLines(pages, { work: "shot_work" });
  if (lines.length !== 4 * 4 + 1 || lines[0] !== "click HandbookPage0_Action" || lines[2] !== "click HandbookContentsButton") {
    throw new Error(`tourLines: ${lines.join("|")}`);
  }
  if (!lines.includes("shot ui shot_work") || !lines.includes("click HandbookPage3_Action") || !lines.includes("wait-text The law and the permit")) {
    throw new Error(`tourLines rows: ${lines.join("|")}`);
  }
  let threw = false;
  try {
    tourLines([{ id: "x", title: "" }]);
  } catch {
    threw = true;
  }
  if (!threw) throw new Error("a page with no title must fail the expansion");
  // The real repo: every page of the pack overlay is toured, none by a typed index.
  const real = expandHandbookTour("@handbook-tour LF_Everon1989");
  const rows = real.filter((l) => /^click HandbookPage\d+_Action$/.test(l)).length;
  const overlayRows = parseHandbookConf(readFileSync(join(ROOT, "addons/LF_Everon1989/Configs/Rules/LF_Handbook_Everon1989.conf"), "utf8"));
  const added = overlayRows.filter((r) => r.op === "add").length;
  if (rows < added + 1) throw new Error(`tour covers ${rows} pages, overlay adds ${added}`);
  return `handbook-flow selftest ok (${rows} pages from the configs)`;
}

if (process.argv[1] && process.argv[1].endsWith("handbook-flow.mjs") && process.argv.includes("--selftest")) {
  console.log(selftest());
}
