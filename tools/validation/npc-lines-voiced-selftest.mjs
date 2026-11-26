// tools/validation/npc-lines-voiced-selftest.mjs - validate-npc-lines-voiced and tools/lib/npc-lines-voiced.mjs.
// inputs: tools/lib/npc-lines-voiced.mjs, tools/validation/validate-npc-lines-voiced.mjs, tools/validation/npc-lines-voiced-selftest.mjs
//
// Builds a throwaway repo (a pack NPC prefab that is a quest giver, its dialogue, a hook class, a quest conf, a rumor
// row, an announce site and the string table) and plants each gap the check exists for:
//   - a hook's declared key with no for_key line, a key literal the hook does not declare, a hook that puts keys in
//     `spoken` with no declaration;
//   - a quest giver's description with no line, a rumor row naming no recorded line;
//   - an announce site with a computed key and no comment, and one with a literal key nobody records;
//   - a string that changed after its line was recorded (the market errand of #4931), and a line with no pin.
//
// red-proof: 2026-11-27, with the `pin !== pinOf(text)` comparison in check() removed the "string changed" case printed FAIL.
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { scratchDir } from "../lib/scratch.mjs";
import { check, pinOf } from "../lib/npc-lines-voiced.mjs";

let failed = 0;
const ok = (cond, msg) => {
  console.log(`${cond ? "PASS" : "FAIL"}  ${msg}`);
  if (!cond) failed++;
};

const st = (rows) => `StringTable {\n Items {\n${Object.entries(rows).map(([id, text]) => `  CustomStringTableItem "{0000000000000001}" {\n   Id "${id}"\n   Target_en_us "${text}"\n  }\n`).join("")} }\n}\n`;
const STRINGS = { "LF-Quest_Job_Desc": "Take the job.", "LF-Shop_Reply": "Take a look.", "LF-Rumor_Line": "A rumor." };

// opts: spoken (hook comment), literals (extra literal in the hook), inserts, lines (extra dialogue lines), quest, rumorKey, announce, strings
function repo(opts = {}) {
  const root = scratchDir("lf-npc-lines-voiced-");
  const pack = join(root, "addons", "LF_Everon1989");
  const core = join(root, "addons", "LifeFramework");
  for (const d of [join(pack, "Prefabs"), join(pack, "Configs", "Quests"), join(pack, "Configs", "Residents"), join(pack, "Language"), join(core, "Scripts", "Game", "Feature", "Shop"), join(core, "Language")]) mkdirSync(d, { recursive: true });
  const strings = { ...STRINGS, ...(opts.strings || {}) };
  writeFileSync(join(core, "Language", "everonlife_localization.st"), st(strings));

  const pin = (key) => pinOf(strings[key.replace(/^#/, "")]);
  const lines = {
    greet: { text: "Good day." },
    job: { for_key: "#LF-Quest_Job_Desc", key_pin: opts.noPin ? undefined : pin("#LF-Quest_Job_Desc"), text: "Take the job, friend." },
    reply: { for_key: "#LF-Shop_Reply", key_pin: pin("#LF-Shop_Reply"), text: "Take a look." },
    ...(opts.lines || {}),
  };
  if (opts.noJobLine) delete lines.job;
  if (opts.noReplyLine) delete lines.reply;
  const spec = {
    id: "baker", name: "Baker", addon: "LF_Everon1989", start: "greet", voice: { id: "AAA", name: "A", model: "eleven_multilingual_v2" }, budget_chars: 500,
    lines,
    nodes: [{ id: "greet", line: "greet", options: [{ text: "shop", action: "OpenShop" }, { text: "work", action: "QuestConverse" }] }],
    replies: { shop: "Show me.", work: "Work?" },
  };
  mkdirSync(join(root, "tools", "assets", "dialogue", "baker"), { recursive: true });
  writeFileSync(join(root, "tools", "assets", "dialogue", "baker", "dialogue.json"), JSON.stringify(spec));

  writeFileSync(join(pack, "Prefabs", "Baker.et"), `LF_NpcBaseEntity : "{0000000000000000}Prefabs/Characters/Base.et" {\n components {\n  LF_DialogueComponent "{1111111111111111}" {\n   m_sDialogueId "baker"\n  }\n  LF_QuestGiverComponent "{2222222222222222}" {\n   m_sGiverId "baker"\n  }\n }\n}\n`);
  writeFileSync(join(pack, "Configs", "Quests", "LF_Quest_Baker.conf"), `LF_QuestGiverConfig {\n m_aGivers {\n  LF_QuestGiverRow "{3000000000000001}" {\n   m_sGiverId "baker"\n   m_aQuests {\n    LF_QuestDefinition "{3000000000000002}" {\n     m_sQuestId "job"\n     m_sDescriptionKey "#LF-Quest_Job_Desc"\n    }\n   }\n  }\n }\n}\n`);
  writeFileSync(join(pack, "Configs", "Residents", "LF_Rumors_Baker.conf"), `LF_RumorsConfig {\n m_aRows {\n  LF_RumorDef "{4000000000000001}" {\n   m_sId "r1"\n   m_sPlaceId "x.y"\n   m_sLineKey "${opts.rumorKey || "#LF-Dialogue_Baker_Reply"}"\n  }\n }\n}\n`);

  const spoken = opts.spoken === undefined ? "// lf-spoken: #LF-Shop_Reply\n" : opts.spoken;
  const body = `\toverride void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)\n\t{\n${opts.inserts === false ? "" : '\t\tspoken.Insert("#LF-Shop_Reply");\n'}${opts.literals || ""}\t}\n`;
  writeFileSync(join(core, "Scripts", "Game", "Feature", "Shop", "LF_ShopTalk.c"), `//! The shop talk.\n${spoken}class LF_DialogueHook_OpenShop : LF_DialogueHook\n{\n${body}}\n\nclass LF_Stores\n{\n${opts.announce || ""}}\n`);
  return root;
}

const run = (opts) => check(repo(opts)).fails;
const only = (f, re) => f.length === 1 && re.test(f[0]);

let f = run();
ok(f.length === 0, `a hook key, a quest description and a rumor row with recorded lines, pinned, passes (${f[0] || ""})`);

f = run({ noReplyLine: true });
ok(f.some((x) => /baker: nothing records #LF-Shop_Reply/.test(x)), "a hook's declared key with no for_key line fails, naming the NPC and the key");

f = run({ noJobLine: true });
ok(f.some((x) => /nothing records #LF-Quest_Job_Desc \(quest giver 'baker'\)/.test(x)), "a quest giver's description with no recorded line fails");

f = run({ literals: '\t\tNotify("#LF-Shop_Greeting");\n', strings: { "LF-Shop_Greeting": "Hi." } });
ok(only(f, /names #LF-Shop_Greeting but does not declare it/), "a key literal the hook does not declare fails");

f = run({ literals: '\t\tNotify("#LF-Shop_Greeting");\n', spoken: "// lf-spoken: #LF-Shop_Reply\n// lf-not-spoken: #LF-Shop_Greeting\n", strings: { "LF-Shop_Greeting": "Hi." } });
ok(f.length === 0, "a key marked lf-not-spoken passes");

f = run({ spoken: "" });
ok(f.some((x) => /puts keys in 'spoken' with no/.test(x)), "a hook that speaks with no declaration fails");

f = run({ rumorKey: "#LF-Dialogue_Baker_Nothing" });
ok(f.some((x) => /rumor 'r1' says #LF-Dialogue_Baker_Nothing/.test(x)), "a rumor row naming no recorded line fails");

f = run({ announce: "\tstatic void A(IEntity npc, string key)\n\t{\n\t\tLF_Dialogue.Announce(npc, key);\n\t}\n" });
ok(f.some((x) => /Announce with a computed key needs/.test(x)), "an announce with a computed key and no comment fails");

f = run({ announce: '\tstatic void A(IEntity npc)\n\t{\n\t\tLF_Dialogue.Announce(npc, "#LF-Shop_Nobody");\n\t}\n', strings: { "LF-Shop_Nobody": "Nobody." } });
ok(f.some((x) => /announces #LF-Shop_Nobody and no recorded line/.test(x)), "an announced literal key nobody records fails");

f = run({ announce: '\tstatic void A(IEntity npc)\n\t{\n\t\t// lf-spoken: #LF-Shop_Reply\n\t\tLF_Dialogue.Announce(npc, key);\n\t}\n' });
ok(f.length === 0, "an announce with a computed key and a lf-spoken comment of recorded keys passes");

// The market errand of #4931: the string moved on after the clerk's line was recorded.
const stale = repo();
writeFileSync(join(stale, "addons", "LifeFramework", "Language", "everonlife_localization.st"), st({ ...STRINGS, "LF-Quest_Job_Desc": "Take the job. Scroll to the last row and press F." }));
f = check(stale).fails;
ok(only(f, /baker\/job: #LF-Quest_Job_Desc changed since the line was recorded/), "a string that changed after its line was recorded fails (the market errand)");

f = run({ noPin: true });
ok(only(f, /baker\/job: no key_pin for #LF-Quest_Job_Desc/), "a for_key line with no pin fails");

if (failed) {
  console.log(`npc-lines-voiced-selftest: ${failed} check(s) failed`);
  process.exit(1);
}
console.log("npc-lines-voiced-selftest: OK");
