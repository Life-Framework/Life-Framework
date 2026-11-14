// tools/validation/dialogue-patch-selftest.mjs
//
// Proves validate-dialogue-patch passes a sound pack patch and fails on each violation it exists
// for, in a fixture tree (#957).
// red-proof: with the unknown-node check disabled this printed "FAIL unknown reply target: expected
// /unknown node/" and exited 1 (2026-11-14).

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { checkDialoguePatches } from "./validate-dialogue-patch.mjs";

function put(root, path, text) {
  const full = join(root, path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}

const CORE = `LF_DialogueConfig {
 m_sId "clerk"
 m_sStart "greet"
 m_aNodes {
  LF_DialogueNode "{1}" {
   m_sId "greet"
   m_sLine "greet"
   m_aOptions {
    LF_DialogueOption "{2}" {
     m_sTextKey "#Reply_Bye"
     m_sNext "bye"
     m_sAction "KnownHook"
    }
   }
  }
  LF_DialogueNode "{3}" {
   m_sId "bye"
   m_sLine "bye"
  }
 }
 m_aLines {
  LF_DialogueLine "{4}" {
   m_sId "greet"
   m_sTextKey "#Line_Greet"
   m_sEvent "SOUND_GREET"
   m_fSeconds 2
  }
  LF_DialogueLine "{5}" {
   m_sId "bye"
   m_sTextKey "#Line_Bye"
   m_sEvent "SOUND_BYE"
   m_fSeconds 1
  }
 }
}
`;
const ST = (...ids) => `StringTable {\n Items {\n${ids.map((i) => `    CustomStringTableItem "{A}" {\n     Id "${i}"\n     Target_en_us "x"\n    }\n`).join("")} }\n}\n`;
const line = (op, id, extra = "") => `  LF_DialogueLine "{9}" {\n   m_sOp "${op}"\n   m_sId "${id}"\n${extra}  }\n`;
const node = (op, id, extra = "") => `  LF_DialogueNode "{8}" {\n   m_sOp "${op}"\n   m_sId "${id}"\n${extra}  }\n`;
const patch = (lines = "", nodes = "", id = "clerk") => `LF_DialogueConfig {\n m_sId "${id}"\n${lines ? ` m_aLines {\n${lines} }\n` : ""}${nodes ? ` m_aNodes {\n${nodes} }\n` : ""}}\n`;

function tree(patchText, extra = {}) {
  const root = mkdtempSync(join(tmpdir(), "lf-dialoguepatch-"));
  put(root, "addons/LifeFramework/Configs/Dialogue/LF_Dialogue_Clerk.conf", CORE);
  put(root, "addons/LifeFramework/Configs/Dialogue/LF_DialogueIndex.conf", `LF_DialogueIndexConfig {\n m_aEntries {\n  LF_DialogueIndexEntry "{7}" {\n   m_sId "clerk"\n   m_Conf "{AA}Configs/Dialogue/LF_Dialogue_Clerk.conf"\n  }\n }\n}\n`);
  put(root, "addons/LifeFramework/Language/core.st", ST("Reply_Bye", "Line_Greet", "Line_Bye"));
  put(root, "addons/LifeFramework/Scripts/Game/Hooks.c", "class LF_DialogueHook_KnownHook : LF_DialogueHook\n{\n}\n");
  put(root, "addons/LF_Pack/Language/pack.st", ST("Pack_Greet"));
  put(root, "addons/LF_Pack/Configs/Dialogue/Patch.conf", patchText);
  for (const [p, t] of Object.entries(extra)) put(root, p, t);
  return root;
}

const greetKeyLine = (key) => line("patch", "greet", `   m_sTextKey "${key}"\n`);
const cases = [
  { name: "a text patch of a line holds", patch: patch(greetKeyLine("#Pack_Greet")), expect: null },
  { name: "a text-only line added with a node holds", patch: patch(line("add", "extra", '   m_sTextKey "#Pack_Greet"\n'), node("add", "extra", '   m_sLine "extra"\n   m_aOptions {\n    LF_DialogueOption "{6}" {\n     m_sTextKey "#Reply_Bye"\n     m_sNext "bye"\n    }\n   }\n')), expect: null },
  { name: "unknown dialogue", patch: patch(greetKeyLine("#Pack_Greet"), "", "baker"), expect: /which the core does not ship/ },
  { name: "patch of a missing line", patch: patch(line("patch", "ghost", '   m_sTextKey "#Pack_Greet"\n')), expect: /patch names an id the core dialogue lacks/ },
  { name: "add of an existing node", patch: patch("", node("add", "bye", '   m_sLine "bye"\n')), expect: /add names an id the core dialogue already has/ },
  { name: "unknown verb", patch: patch(line("delete", "greet")), expect: /is not add, replace, patch or remove/ },
  { name: "unknown reply target", patch: patch("", node("patch", "greet", '   m_aOptions {\n    LF_DialogueOption "{6}" {\n     m_sTextKey "#Reply_Bye"\n     m_sNext "nowhere"\n    }\n   }\n')), expect: /unknown node/ },
  { name: "a removed node a reply still leads to", patch: patch("", node("remove", "bye")), expect: /unknown node 'bye'|says unknown/ },
  { name: "a line with an event and no length", patch: patch(line("add", "half", '   m_sTextKey "#Pack_Greet"\n   m_sEvent "SOUND_X"\n')), expect: /half-made recording/ },
  { name: "a text key in no string table", patch: patch(greetKeyLine("#Pack_Missing")), expect: /text key #Pack_Missing is in no \.st/ },
  { name: "a hook with no class", patch: patch("", node("patch", "greet", '   m_aOptions {\n    LF_DialogueOption "{6}" {\n     m_sTextKey "#Reply_Bye"\n     m_sNext "bye"\n     m_sAction "NoSuchHook"\n    }\n   }\n')), expect: /no class LF_DialogueHook_NoSuchHook/ },
  { name: "more than four replies", patch: patch("", node("patch", "greet", `   m_aOptions {\n${'    LF_DialogueOption "{6}" {\n     m_sTextKey "#Reply_Bye"\n     m_sNext "bye"\n    }\n'.repeat(5)}   }\n`)), expect: /offers 5 replies/ },
  { name: "index add of a known id", patch: patch(), extra: { "addons/LF_Pack/Configs/Dialogue/Index.conf": `LF_DialogueIndexConfig {\n m_aEntries {\n  LF_DialogueIndexEntry "{7}" {\n   m_sOp "add"\n   m_sId "clerk"\n   m_Conf "{AA}Configs/Dialogue/LF_Dialogue_Clerk.conf"\n  }\n }\n}\n` }, expect: /index clerk: add names an id the core index already has/ },
  { name: "index file in no addon", patch: patch(), extra: { "addons/LF_Pack/Configs/Dialogue/Index.conf": `LF_DialogueIndexConfig {\n m_aEntries {\n  LF_DialogueIndexEntry "{7}" {\n   m_sOp "add"\n   m_sId "baker"\n   m_Conf "{AB}Configs/Dialogue/Missing.conf"\n  }\n }\n}\n` }, expect: /is in no addon/ },
];

let failed = 0;
const roots = [];
try {
  for (const c of cases) {
    const root = tree(c.patch, c.extra);
    roots.push(root);
    const errors = checkDialoguePatches(root);
    if (c.expect === null ? errors.length > 0 : !errors.some((e) => c.expect.test(e))) {
      failed++;
      console.log(`FAIL ${c.name}: expected ${c.expect ?? "no violations"}, got:\n  ${errors.join("\n  ") || "(no violations)"}`);
    }
  }
} finally {
  for (const r of roots) rmSync(r, { recursive: true, force: true });
}

if (failed) {
  console.log(`dialogue-patch-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log(`dialogue-patch-selftest: OK (${cases.length} cases)`);
