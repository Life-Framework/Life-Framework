// Proves tools\cli asset dialogue (tools/assets/pipeline/dialogue.mjs) offline: no ElevenLabs call,
// no ffmpeg. The network is a fake that counts what it is asked for. The last block checks the
// dialogues in this repo: the generated config and audio project match their dialogue.json, and
// every line's recording is in the addon, so a reworded line cannot ship with its old voice.
// red-proof: 2026-11-13, one line broken per block, each restored after its FAIL was seen:
//   specProblems without the reachability walk  -> FAIL spec: a node nothing leads to is refused
//   staleLines comparing nothing                -> FAIL cache: an unchanged dialogue buys nothing
//   run() without the budget check              -> FAIL budget: a run over the NPC's budget is refused before any call
//   elevenlabs.call without scrub()             -> FAIL key: an error never carries the key
//   upsertStrings appending without the lookup  -> FAIL strings: a second run adds nothing
//   envelope without the loudness scale         -> FAIL envelope: loud speech opens the mouth fully
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import * as dlg from "../assets/pipeline/dialogue.mjs";
import * as eleven from "../assets/pipeline/elevenlabs.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const ADDON = "addons/LifeFramework";

// A mono 16-bit wav: `parts` is [[seconds, amplitude 0..1], ...] of a 220 Hz tone.
function wav(parts, rate = 44100) {
  const samples = [];
  for (const [seconds, amp] of parts) for (let i = 0; i < Math.round(seconds * rate); i++) samples.push(Math.round(Math.sin(i * 2 * Math.PI * 220 / rate) * amp * 32000));
  const buf = Buffer.alloc(44 + samples.length * 2);
  buf.write("RIFF", 0, "latin1"); buf.writeUInt32LE(36 + samples.length * 2, 4); buf.write("WAVEfmt ", 8, "latin1");
  buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20); buf.writeUInt16LE(1, 22); buf.writeUInt32LE(rate, 24); buf.writeUInt32LE(rate * 2, 28);
  buf.writeUInt16LE(2, 32); buf.writeUInt16LE(16, 34); buf.write("data", 36, "latin1"); buf.writeUInt32LE(samples.length * 2, 40);
  samples.forEach((s, i) => buf.writeInt16LE(s, 44 + i * 2));
  return buf;
}

const spec = () => ({
  id: "baker", name: "Baker", speaker_key: "#LF-Test_Baker", voice: { id: "voice1", name: "Test", model: "eleven_multilingual_v2" }, budget_chars: 400, start: "greet",
  nodes: [
    { id: "greet", line: "greet", options: [{ text: "buy", next: "sold", action: "ShopOpen" }, { text: "bye", next: "bye", condition: "" }] },
    { id: "sold", line: "sold", options: [{ text: "bye", next: "bye" }] },
    { id: "bye", line: "bye" },
  ],
  lines: { greet: { text: "Good morning. Fresh bread today." }, sold: { text: "Here you are.", for_key: "#LF-Test_Sold" }, bye: { text: "Good day." } },
  replies: { buy: "A loaf, please.", bye: "Goodbye." },
});
const ST = `StringTable {\n ItemClassName "CustomStringTableItem"\n Items {\n    CustomStringTableItem "{AAAAAAAAAAAAAAAA}" {\n     Id "LF-Test_Sold"\n     Target_en_us "Sold."\n     Comment "c"\n    }\n    CustomStringTableItem "{BBBBBBBBBBBBBBBB}" {\n     Id "LF-Test_Baker"\n     Target_en_us "Baker"\n     Comment "c"\n    }\n  }\n }\n}\n`;

export async function main() {
  let failed = 0;
  const say = console.log;
  const ok = (cond, what) => { if (!cond) { failed++; say(`FAIL ${what}`); } };
  const strings = dlg.parseStrings(ST);

  // ---- spec: what would waste characters or strand the player is refused before any call ------
  {
    ok(dlg.specProblems(spec(), strings).length === 0, `spec: a complete dialogue passes (${dlg.specProblems(spec(), strings)[0]})`);
    const bad = (edit, pattern, what) => { const s = spec(); edit(s); ok(dlg.specProblems(s, strings).some((p) => pattern.test(p)), `spec: ${what} (got: ${dlg.specProblems(s, strings).join(" | ")})`); };
    bad((s) => { s.nodes[0].options[0].next = "nowhere"; }, /unknown node 'nowhere'/, "a reply that leads nowhere is refused");
    bad((s) => { s.nodes.push({ id: "island", line: "bye" }); }, /island cannot be reached/, "a node nothing leads to is refused");
    bad((s) => { s.nodes[0].options = [1, 2, 3, 4, 5].map(() => ({ text: "bye", next: "bye" })); }, /5 options/, "more replies than the screen has rows are refused");
    bad((s) => { s.nodes[1].options[0].condition = "Never"; }, /every option has a condition/, "a node whose every reply can be hidden is refused");
    bad((s) => { s.lines.greet.text = "Pay <b>10</b> now"; }, /markup/, "markup the voice would read aloud is refused");
    bad((s) => { s.lines.extra = { text: "Nobody says this." }; }, /extra is used by no node/, "a line nothing says is refused");
    bad((s) => { s.lines.sold.for_key = "#LF-Test_Missing"; }, /not in the string table/, "a for_key that is not a real string is refused");
    bad((s) => { s.nodes[0].options[0].action = "shop.open"; }, /must be PascalCase/, "a hook name that cannot be a class is refused");
    bad((s) => { s.budget_chars = 99999; }, /budget_chars/, "a budget over the per-NPC cap is refused");
    bad((s) => { s.lines.greet.say = "[cheerful] Good morning. Stale bread today."; }, /read exactly as text/, "a voiced say that differs from its caption is refused");
    bad((s) => { s.lines.greet.say = "Good morning. Fresh bread today."; }, /adds no \[audio tag\]/, "a say with no audio tag is refused");
    const tagged = spec();
    tagged.lines.greet.say = "[cheerful] Good morning. [warmly] Fresh bread today.";
    ok(dlg.specProblems(tagged, strings).length === 0, `spec: a say with audio tags over the same words passes (${dlg.specProblems(tagged, strings)[0]})`);
    ok(dlg.voicedText(tagged.lines.greet) === tagged.lines.greet.say && dlg.voicedText(tagged.lines.bye) === "Good day.", "spec: the voice is sent say when there is one, else the caption");
  }

  // ---- envelope: the mouth follows the loudness of the recording -------------------------------
  {
    const env = dlg.envelope(wav([[0.5, 0], [0.5, 0.2], [0.5, 0.02]]));
    ok(env.seconds === 1.5 && env.mouth.length === 30, `envelope: one digit per 50 ms (got ${env.mouth.length} for ${env.seconds} s)`);
    ok(/^0{10}/.test(env.mouth), `envelope: silence keeps the mouth shut (got ${env.mouth})`);
    ok(/^9+$/.test(env.mouth.slice(11, 19)), `envelope: loud speech opens the mouth fully (got ${env.mouth})`);
    ok(/^[1-3]+$/.test(env.mouth.slice(21, 29)), `envelope: quiet speech opens it a little (got ${env.mouth})`);
    let message = "";
    try { dlg.envelope(Buffer.from("not a wav file at all")); } catch (e) { message = e.message; }
    ok(/not a RIFF wav/.test(message), "envelope: a file that is not a wav is refused");
  }

  // ---- strings: append what is missing, update only this tool's own rows -----------------------
  {
    let n = 0;
    const guid = () => (++n).toString(16).toUpperCase().padStart(16, "C");
    const entries = dlg.stringEntries(spec());
    const once = dlg.upsertStrings(ST, entries, guid);
    ok(entries.every((e) => once.includes(`Id "${e.id}"`)) && once.endsWith("  }\n }\n}\n"), "strings: every caption and reply is appended inside the Items block");
    ok(dlg.upsertStrings(once, entries, guid) === once, "strings: a second run adds nothing");
    const s = spec();
    s.lines.bye.text = "Good day to you.";
    const twice = dlg.upsertStrings(once, dlg.stringEntries(s), guid);
    ok(twice.includes('Target_en_us "Good day to you."') && !twice.includes('Target_en_us "Good day."') && twice.includes('Target_en_us "Sold."'), "strings: a reworded line updates its own row and no other");
  }

  // ---- generated files: stable, and they name what the game reads -------------------------------
  {
    const s = spec();
    const rows = dlg.lineRows(s, { lines: { greet: { seconds: 2.1, mouth: "0590" } } }, null);
    const conf = dlg.confText(s, rows, "{1111111111111111}Sounds/Dialogue/baker/Dialogue_baker.acp");
    ok(conf === dlg.confText(s, rows, "{1111111111111111}Sounds/Dialogue/baker/Dialogue_baker.acp"), "conf: the same dialogue gives the same file (no churn)");
    ok(conf.includes('m_sAction "ShopOpen"') && conf.includes('m_sForKey "#LF-Test_Sold"') && conf.includes('m_sEvent "SOUND_DLG_BAKER_GREET"') && conf.includes('m_sMouth "0590"'), "conf: actions, answered keys, events and envelopes are written");
    const acp = dlg.acpText(s, rows);
    ok((acp.match(/SoundClass \{/g) || []).length === 3 && (acp.match(/BankLocalClass \{/g) || []).length === 3 && acp.includes('name "SOUND_DLG_BAKER_BYE"'), "acp: one sound event and one sample bank per line");
    const ids = [...acp.matchAll(/^\s+id (\d+)$/gm)].map((m) => m[1]);
    ok(new Set(ids.filter((i) => Number(i) > 1023)).size === 4 + 9, `acp: every node id is its own (got ${new Set(ids.filter((i) => Number(i) > 1023)).size})`);
  }

  // ---- cache, budget and the key: a fake service that counts what it is asked for --------------
  {
    const root = mkdtempSync(join(tmpdir(), "lf-dialogue-selftest-"));
    const keyFile = join(root, "key");
    const KEY = "sk_selftest_0123456789";
    writeFileSync(keyFile, KEY + "\n");
    const oldKeyFile = process.env.LF_ELEVENLABS_KEY_FILE;
    process.env.LF_ELEVENLABS_KEY_FILE = keyFile;
    const put = (rel, text) => { mkdirSync(dirname(join(root, rel)), { recursive: true }); writeFileSync(join(root, rel), text); };
    const save = (s) => put("tools/assets/dialogue/baker/dialogue.json", JSON.stringify(s));
    put(`${ADDON}/Language/everonlife_localization.st`, ST);
    save(spec());
    let spoken = [], used = 100, fail = "";
    const fetchImpl = async (url, init) => {
      if (fail) throw new Error(fail);
      const json = (o) => ({ ok: true, json: async () => o, headers: new Map() });
      if (url.endsWith("/v1/user/subscription")) return json({ tier: "starter", character_count: used, character_limit: 40000 });
      if (url.endsWith("/v1/voices")) return json({ voices: [{ voice_id: "voice1" }] });
      if (url.includes("/v1/text-to-speech/voice1")) {
        const text = JSON.parse(init.body).text;
        spoken.push(text);
        used += text.length;
        return { ok: true, arrayBuffer: async () => Buffer.alloc(2000, 7), headers: new Map([["request-id", "req" + spoken.length], ["character-cost", String(text.length)]]) };
      }
      throw new Error(`selftest: unexpected call ${url}`);
    };
    const convert = (ff, src, dst) => { mkdirSync(dirname(dst), { recursive: true }); writeFileSync(dst, wav([[0.2, 0.3]])); };
    const go = (extra = {}) => dlg.run(root, "baker", { fetchImpl, convert, log: () => {}, ...extra });
    try {
      const first = await go();
      ok(spoken.length === 3 && first.chars === Object.values(spec().lines).reduce((n, l) => n + l.text.length, 0), `cache: a new dialogue buys every line once (calls ${spoken.length}, chars ${first.chars})`);
      const addon = join(root, ADDON);
      ok(["greet", "sold", "bye"].every((l) => existsSync(join(addon, `Sounds/Dialogue/baker/baker_${l}.wav`)) && existsSync(join(addon, `Sounds/Dialogue/baker/baker_${l}.wav.meta`))), "import: every line has its wav and meta in the addon");
      ok(existsSync(join(addon, "Configs/Dialogue/LF_Dialogue_Baker.conf.meta")) && existsSync(join(addon, "Sounds/Dialogue/baker/Dialogue_baker.acp.meta")), "import: the config and the audio project have metas");
      const prov = JSON.parse(readFileSync(dlg.provenanceFile(root, "baker"), "utf8"));
      ok(prov.lines.greet.request_id === "req1" && prov.lines.greet.mouth.length === 4 && prov.lines.greet.chars_total === 32, "provenance: each line records its request, its envelope and what it cost");
      ok(readFileSync(join(root, `${ADDON}/Language/everonlife_localization.st`), "utf8").includes('Id "LF-Dialogue_Baker_Greet"'), "strings: the captions are in the string table after a run");

      spoken = [];
      const again = await go();
      ok(spoken.length === 0 && again.generated === 0, `cache: an unchanged dialogue buys nothing (calls ${spoken.length})`);

      const s = spec();
      s.lines.bye.text = "Good day to you.";
      save(s);
      await go();
      ok(spoken.length === 1 && spoken[0] === "Good day to you.", `cache: a reworded line is the only one bought again (bought: ${spoken.join(" | ")})`);

      spoken = [];
      ok(existsSync(dlg.cacheDir(root, "baker")), "cache: raw downloads are kept under tmp/");
      // A fresh worktree: the tracked provenance and the addon wavs are there, tmp/ is not.
      const fresh = mkdtempSync(join(tmpdir(), "lf-dialogue-selftest-fresh-"));
      for (const rel of ["tools/assets/dialogue/baker/dialogue.json", "tools/assets/dialogue/baker/elevenlabs.json", `${ADDON}/Language/everonlife_localization.st`]) {
        mkdirSync(dirname(join(fresh, rel)), { recursive: true });
        writeFileSync(join(fresh, rel), readFileSync(join(root, rel)));
      }
      for (const f of readdirSync(join(addon, "Sounds/Dialogue/baker"))) {
        mkdirSync(join(fresh, ADDON, "Sounds/Dialogue/baker"), { recursive: true });
        writeFileSync(join(fresh, ADDON, "Sounds/Dialogue/baker", f), readFileSync(join(addon, "Sounds/Dialogue/baker", f)));
      }
      await dlg.run(fresh, "baker", { fetchImpl, convert, log: () => {} });
      ok(spoken.length === 0, `cache: a fresh worktree pays nothing for lines already in the addon (calls ${spoken.length})`);

      const gone = spec();
      delete gone.lines.sold;
      gone.nodes = gone.nodes.filter((n) => n.id !== "sold");
      gone.nodes[0].options = gone.nodes[0].options.slice(1);
      delete gone.replies.buy;
      gone.lines.bye.text = "Good day to you.";
      save(gone);
      await go();
      ok(!existsSync(join(addon, "Sounds/Dialogue/baker/baker_sold.wav")) && !existsSync(join(addon, "Sounds/Dialogue/baker/baker_sold.wav.meta")), "import: a line that left the dialogue leaves the addon");

      spoken = [];
      const dear = spec();
      dear.lines.greet.text = "A much longer greeting than before, well past what is left of this baker's budget of characters. ".repeat(3).trim();
      dear.budget_chars = 120;
      save(dear);
      let message = "";
      try { await go(); } catch (e) { message = e.message; }
      ok(/over its budget of 120/.test(message) && spoken.length === 0, `budget: a run over the NPC's budget is refused before any call (got: ${message}; calls ${spoken.length})`);

      const dry = await go({ dryRun: true }).catch((e) => e.message);
      ok(typeof dry === "string" && spoken.length === 0, "budget: a dry run checks the budget too and calls nothing");

      save(spec());
      fail = `connect failed for xi-api-key ${KEY}`;
      message = "";
      try { await go({ force: "all" }); } catch (e) { message = e.message; }
      ok(message.length > 0 && !message.includes(KEY) && message.includes("<elevenlabs-key>"), `key: an error never carries the key (got: ${message.replace(KEY, "LEAKED")})`);
      fail = "";
      ok(eleven.cost("abcd", "eleven_flash_v2_5") === 2 && eleven.cost("abcd") === 4, "cost: the half-price models are estimated at half");
    } finally {
      if (oldKeyFile === undefined) delete process.env.LF_ELEVENLABS_KEY_FILE; else process.env.LF_ELEVENLABS_KEY_FILE = oldKeyFile;
    }
  }

  // ---- this repo's dialogues: what is committed matches its dialogue.json ----------------------
  {
    const base = join(ROOT, "tools", "assets", "dialogue");
    const st = readFileSync(join(ROOT, ADDON, "Language", "everonlife_localization.st"), "utf8");
    const real = dlg.parseStrings(st);
    for (const id of existsSync(base) ? readdirSync(base).filter((d) => existsSync(dlg.specFile(ROOT, d))) : []) {
      const s = dlg.loadSpec(ROOT, id);
      const prov = dlg.loadProvenance(ROOT, id);
      const fix = `run: tools\\cli asset dialogue ${id}`;
      const problems = dlg.specProblems(s, real);
      ok(problems.length === 0, `${id}: dialogue.json has problems: ${problems.join(" | ")}`);
      const stale = dlg.staleLines(ROOT, s, prov);
      ok(stale.length === 0, `${id}: these lines were reworded or are missing their recording: ${stale.join(", ")} (${fix})`);
      for (const [rel, text] of Object.entries(dlg.expectedFiles(ROOT, s, prov))) {
        const file = join(ROOT, ADDON, rel);
        ok(existsSync(file) && readFileSync(file, "utf8") === text, `${id}: ${ADDON}/${rel} does not match dialogue.json (${fix})`);
      }
      for (const e of dlg.stringEntries(s)) ok(real.get(e.id) === e.text.replace(/"/g, '\\"'), `${id}: string ${e.id} does not match dialogue.json (${fix})`);
      const total = Object.values(prov.lines).reduce((n, l) => n + (l.chars_total || 0), 0);
      ok(total <= s.budget_chars, `${id}: ${total} characters spent, over its budget of ${s.budget_chars}`);
    }
  }

  if (failed) {
    console.log(`dialogue-pipeline-selftest: FAILED (${failed})`);
    return 1;
  }
  console.log("dialogue-pipeline-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
