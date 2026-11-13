// Proves the deterministic parts of the asset pipeline's sound stage (tools/assets/pipeline/sound.mjs,
// docs/asset-pipeline/sounds.md) offline: no ElevenLabs call, no ffmpeg, no game. The network is a
// fake fetch. It also checks what the stage shipped: every committed sample is the format the engine
// plays, and every committed sound project is what the generator writes from sounds.json today.
// red-proof: 2026-11-13, one line broken per block, each restored after its FAIL was seen:
//   soundProblems without the duration test   -> FAIL spec: a duration under the API's 0.5 s is refused
//   http() without scrub()                    -> FAIL key: an error never carries the key
//   stepGenerate without the cap test         -> FAIL cap: a run over the generation cap is refused before any call
//   trimSilence without the lone-burst rule   -> FAIL trim: a stray tick after the sound does not keep the silence before it
//   normalisePeak ignoring the target         -> FAIL level: the peak is normalised to -3 dBFS
//   loopSeam returning its input              -> FAIL loop: a seamed loop wraps without a step
//   encodeWav writing 2 channels              -> FAIL wav: the header says PCM, mono, 48 kHz, 16 bit
//   checkWav without the rate test            -> FAIL wav: a 44.1 kHz file is flagged
//   acpText linking the shader to bank id + 1 -> FAIL acp: every link names a node the project has
//   acpText dropping the loop properties      -> FAIL acp: a loop's bank loops forever and fades out
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import * as sound from "../assets/pipeline/sound.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const ADDON = join(ROOT, "addons", "LifeFramework");

export async function main() {
  let failed = 0;
  const say = console.log;
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    say(`FAIL ${what}`);
  };
  const rec = () => ({
    slug: "buzzer", name: "Buzzer", kind: "gadget", project_dir: "Sounds/Items/Buzzer", project_prefix: "Items_Buzzer",
    output_format: "pcm_48000", budget: { generations: 4 },
    sounds: [
      { id: "use", event: "SOUND_LF_USE", project: "Use", bus: "sfx", range: "reload60", volume_db: -14, prompt: "A door buzzer, dry, no reverb.", duration_seconds: 1, prompt_influence: 0.6, variations: 2 },
      { id: "hum", event: "SOUND_LF_HUM_LP", project: "Use", bus: "sfx", range: "near22", volume_db: -20, loop: true, prompt: "A steady mains hum.", duration_seconds: 2, prompt_influence: 0.6, variations: 1 },
    ],
    generations: [],
  });

  // ---- spec ---------------------------------------------------------------------------------------
  {
    ok(sound.soundProblems(rec()).length === 0, `spec: a complete sounds.json passes (${sound.soundProblems(rec())[0]})`);
    const bad = (edit, pattern, what) => {
      const r = rec();
      edit(r);
      ok(sound.soundProblems(r).some((p) => pattern.test(p)), `spec: ${what}`);
    };
    bad((r) => (r.sounds[0].duration_seconds = 0.3), /0\.5 to 30/, "a duration under the API's 0.5 s is refused");
    bad((r) => (r.sounds[0].event = "Use"), /SOUND_NAME/, "an event that is not SOUND_NAME is refused");
    bad((r) => (r.sounds[1].event = "SOUND_LF_USE"), /used twice/, "two sounds on one event are refused");
    bad((r) => (r.sounds[0].bus = "music"), /bus must be/, "an unknown mixer input is refused");
    bad((r) => (r.sounds[0].range = "moon"), /range must be/, "an unknown distance curve is refused");
    bad((r) => (r.budget.generations = sound.MAX_GENERATIONS + 1), /budget\.generations/, "a generation cap over the limit is refused");
    bad((r) => (r.sounds[0].pick = [1]), /needs a reason/, "a pick with no reason is refused");
    bad((r) => (r.sounds[0].layer = { id: "hum", take: 1 }), /layer_only/, "a layer that is not a layer-only sound is refused");
    bad((r) => (r.project_dir = "Assets/x"), /project_dir/, "a project_dir outside Sounds/ is refused");
    const body = sound.requestBody(rec().sounds[1]);
    ok(body.text === "A steady mains hum." && body.duration_seconds === 2 && body.prompt_influence === 0.6 && body.loop === true, "spec: the request carries the prompt, duration, influence and the loop flag");
    ok(!("loop" in sound.requestBody(rec().sounds[0])), "spec: a one-shot does not ask for a loop");
    ok(sound.estimateCredits({ duration_seconds: 1.5 }) === 200 && sound.estimateCredits({ duration_seconds: 10 }) === 400, "spec: the estimate is 40 credits a second with a floor of 200");
    ok(sound.rawChannels("pcm_48000", 288000, 1.5) === 2 && sound.rawChannels("pcm_48000", 96000, 1) === 1 && sound.rawChannels("mp3_44100_128", 5000, 1) === undefined, "spec: the channel count of a raw PCM answer is read from its size");
  }

  // ---- key, cap, record and resume ---------------------------------------------------------------
  {
    const root = mkdtempSync(join(tmpdir(), "lf-sound-selftest-"));
    mkdirSync(join(root, "tools", "assets", "buzzer"), { recursive: true });
    const file = sound.soundsFile(root, "buzzer");
    const keyFile = join(root, "key");
    const KEY = "sk_selftest_not_a_real_key";
    writeFileSync(keyFile, KEY + "\n");
    const oldKey = process.env.LF_ELEVENLABS_KEY_FILE;
    process.env.LF_ELEVENLABS_KEY_FILE = keyFile;
    const quiet = console.log;
    console.log = () => {};
    try {
      let message = "";
      let sent = null;
      const refuse = async (url, init) => { sent = { url, init }; return { ok: false, status: 401, text: async () => `invalid api key ${KEY}` }; };
      try { await sound.subscription({ fetchImpl: refuse }); } catch (e) { message = e.message; }
      ok(sent?.init.headers["xi-api-key"] === KEY, "key: the key goes in the xi-api-key header");
      ok(/HTTP 401/.test(message) && !message.includes(KEY) && message.includes("<elevenlabs-key>"), `key: an error never carries the key (got: ${message.replace(KEY, "<the key>")})`);
      const boom = async () => { throw new Error(`socket closed for ${KEY}`); };
      try { await sound.subscription({ fetchImpl: boom }); } catch (e) { message = e.message; }
      ok(!message.includes(KEY), "key: a network error never carries the key");

      // A fake account: each generation costs 40 credits a second, seen by the next balance read.
      let used = 100;
      const calls = [];
      const api = async (url, init) => {
        calls.push(url);
        if (url.endsWith("/v1/user/subscription")) return { ok: true, status: 200, json: async () => ({ tier: "starter", character_count: used, character_limit: 40000 }) };
        const body = JSON.parse(init.body);
        used += body.duration_seconds * 40;
        return { ok: true, status: 200, arrayBuffer: async () => new ArrayBuffer(48000 * 2 * 2 * body.duration_seconds) };
      };

      const over = rec();
      over.budget.generations = 2; // three are asked for
      writeFileSync(file, JSON.stringify(over));
      message = "";
      try { await sound.stepGenerate(root, "buzzer", { fetchImpl: api, settleMs: 0 }); } catch (e) { message = e.message; }
      ok(/cap is 2/.test(message) && calls.length === 0, `cap: a run over the generation cap is refused before any call (got: ${message}; calls ${calls.length})`);

      writeFileSync(file, JSON.stringify(rec()));
      const done = await sound.stepGenerate(root, "buzzer", { fetchImpl: api, settleMs: 0, now: new Date("2026-11-13T10:00:00Z") });
      const posts = calls.filter((u) => u.includes("/v1/sound-generation"));
      ok(posts.length === 3 && posts.every((u) => u.endsWith("output_format=pcm_48000")), "record: one call per variation, in the asked output format");
      ok(done.generations.length === 3 && done.generations.map((g) => `${g.id}${g.take}`).join() === "use1,use2,hum1", "record: sounds.json has a row per generation");
      const row = done.generations[0];
      ok(row.date === "2026-11-13" && row.prompt === "A door buzzer, dry, no reverb." && row.duration_seconds === 1 && row.prompt_influence === 0.6 && row.channels === 2 && row.credits_estimate === 200, "record: a row keeps the prompt, the parameters, the date and the channel count");
      ok(done.runs.length === 1 && done.runs[0].credits_before === 100 && done.runs[0].credits === 160 && done.runs[0].plan === "starter", "record: the run keeps the account's balance before and after");
      ok(existsSync(join(sound.soundCache(root, "buzzer"), "raw", "use_02.pcm48000")), "record: the raw take is cached");
      ok(JSON.stringify(JSON.parse(readFileSync(file, "utf8"))) === JSON.stringify(done), "record: what was returned is what is on disk");

      const before = calls.length;
      await sound.stepGenerate(root, "buzzer", { fetchImpl: api, settleMs: 0 });
      ok(calls.length === before, "resume: a second run generates nothing and calls nothing");
      await sound.stepGenerate(root, "buzzer", { fetchImpl: api, settleMs: 0, again: "use" });
      const after = JSON.parse(readFileSync(file, "utf8"));
      ok(after.generations.length === 4 && after.generations[3].id === "use" && after.generations[3].take === 3 && after.runs.length === 2, "resume: --again adds one take and one run");

      message = "";
      try { sound.stepWire(root, "buzzer"); } catch (e) { message = e.message; }
      ok(/no pick for: use, hum/.test(message), `wire: it stops until every sound has a pick (got: ${message})`);
    } finally {
      console.log = quiet;
      if (oldKey === undefined) delete process.env.LF_ELEVENLABS_KEY_FILE;
      else process.env.LF_ELEVENLABS_KEY_FILE = oldKey;
    }
  }

  // ---- samples: trim, level, fades, loop seam ----------------------------------------------------
  const R = sound.RATE;
  const tone = (seconds, hz, amp) => Int16Array.from({ length: Math.round(seconds * R) }, (_, i) => Math.round(amp * Math.sin((2 * Math.PI * hz * i) / R)));
  const join16 = (...parts) => {
    const out = new Int16Array(parts.reduce((n, p) => n + p.length, 0));
    let at = 0;
    for (const p of parts) { out.set(p, at); at += p.length; }
    return out;
  };
  const silence = (seconds) => new Int16Array(Math.round(seconds * R));
  {
    const take = join16(silence(0.2), tone(0.3, 1000, 8000), silence(0.4));
    const cut = sound.trimSilence(take);
    ok(Math.abs(cut.length / R - 0.33) < 0.012, `trim: the silence before and after a sound is cut (kept ${(cut.length / R).toFixed(3)} s of 0.9)`);
    const tick = join16(silence(0.1), tone(0.3, 1000, 8000), silence(0.4), Int16Array.from([9000, -9000, 9000]), silence(0.01));
    ok(sound.trimSilence(tick).length / R < 0.36, `trim: a stray tick after the sound does not keep the silence before it (kept ${(sound.trimSilence(tick).length / R).toFixed(3)} s)`);
    ok(sound.trimSilence(silence(0.5)) === null, "trim: a silent take is reported, not cut to nothing");

    const level = (x) => 20 * Math.log10(sound.peakOf(x) / 32768);
    ok(Math.abs(level(sound.normalisePeak(tone(0.1, 500, 900))) + 3) < 0.05 && Math.abs(level(sound.normalisePeak(tone(0.1, 500, 32000))) + 3) < 0.05, "level: the peak is normalised to -3 dBFS");
    const faded = sound.fade(tone(0.2, 12000, 20000));
    ok(faded[0] === 0 && Math.abs(faded[faded.length - 1]) < 400, "level: a one-shot starts and ends at zero");

    // A sine cut at a bad phase: the raw loop wraps with a step, the seamed loop does not.
    const raw = tone(1.0037, 220, 16000);
    const rawInfo = sound.wavInfo(sound.encodeWav(raw));
    const seamed = sound.wavInfo(sound.encodeWav(sound.loopSeam(raw)));
    ok(rawInfo.seam > 0.1 && sound.checkWav(rawInfo, { loop: true }).some((p) => /would click/.test(p)), "loop: a loop cut mid-wave is flagged");
    ok(seamed.seam <= Math.max(0.01, seamed.maxStep) && sound.checkWav(seamed, { loop: true }).length === 0, `loop: a seamed loop wraps without a step (seam ${(seamed.seam * 100).toFixed(1)}%)`);

    const mixed = sound.mixLayer(tone(0.5, 300, 4000), tone(0.05, 2000, 12000), R, { delayMs: 100 });
    ok(mixed.length === Math.round(0.6 * R) && sound.peakOf(mixed.subarray(0, 2400)) > 11000 && sound.peakOf(mixed.subarray(3000, 4700)) === 0 && sound.peakOf(mixed.subarray(4800)) > 3900, "layer: the layer starts the sound and the base follows after the delay");

    const done = sound.processSamples(take);
    const info = sound.wavInfo(sound.encodeWav(done));
    ok(sound.checkWav(info).length === 0 && Math.abs(info.peakDb + 3) < 0.05, `process: a processed take passes the checks (${sound.checkWav(info)[0]})`);
    let threw = "";
    try { sound.processSamples(silence(0.3)); } catch (e) { threw = e.message; }
    ok(/silent/.test(threw), "process: a silent take is an error");
  }

  // ---- WAV: what the engine reads -----------------------------------------------------------------
  {
    const wav = sound.encodeWav(sound.normalisePeak(tone(0.25, 800, 9000)));
    const info = sound.wavInfo(wav);
    ok(wav.length === 44 + 12000 * 2 && wav.toString("latin1", 0, 4) === "RIFF" && wav.readUInt32LE(4) === wav.length - 8 && wav.readUInt32LE(40) === 24000, "wav: a 44-byte header and the sizes it promises");
    ok(info.tag === 1 && info.channels === 1 && info.rate === 48000 && info.bits === 16, "wav: the header says PCM, mono, 48 kHz, 16 bit");
    ok(Math.abs(info.seconds - 0.25) < 1e-6 && Math.abs(info.zeroCrossHz - 800) < 10, "wav: the reader measures what was written");
    const edit = (fn) => { const b = Buffer.from(wav); fn(b); return sound.checkWav(sound.wavInfo(b)); };
    ok(edit((b) => b.writeUInt32LE(44100, 24)).some((p) => /44100 Hz/.test(p)), "wav: a 44.1 kHz file is flagged");
    ok(edit((b) => b.writeUInt16LE(2, 22)).some((p) => /2 channels/.test(p)), "wav: a stereo file is flagged");
    ok(edit((b) => b.writeUInt16LE(3, 20)).some((p) => /format tag 3/.test(p)), "wav: a float file is flagged");
    const check = (x, o) => sound.checkWav(sound.wavInfo(sound.encodeWav(x)), o);
    ok(check(join16(silence(0.1), tone(0.2, 800, 23000))).some((p) => /silence before/.test(p)), "wav: a sound that starts late is flagged");
    ok(check(tone(0.2, 800, 32767)).some((p) => /no headroom/.test(p)), "wav: a file at full scale is flagged");
    ok(check(tone(0.2, 800, 3000)).some((p) => /too quiet/.test(p)), "wav: a file that was not normalised is flagged");
    ok(check(sound.normalisePeak(tone(8, 800, 3000))).some((p) => /over 6 s/.test(p)), "wav: an overlong sample is flagged");
    let threw = "";
    try { sound.wavInfo(Buffer.from("ID3 this is an mp3, not a wav, whatever its name says")); } catch (e) { threw = e.message; }
    ok(/not a RIFF WAVE/.test(threw), "wav: a file that is not a WAV is refused");
  }

  // ---- .meta and .acp text ------------------------------------------------------------------------
  const events = [
    { event: "SOUND_SHOT", bus: "shots", range: "voice70", volume_db: -6, pitch_variance: 0.3, samples: ["{00000000000000A1}Sounds/X/Samples/a.wav"] },
    { event: "SOUND_DRY", bus: "shots", range: "near22", volume_db: -20, samples: ["{00000000000000A2}Sounds/X/Samples/b.wav", "{00000000000000A3}Sounds/X/Samples/c.wav"] },
    { event: "SOUND_LF_STUN_LP", bus: "sfx", range: "near22", volume_db: -12, loop: true, samples: ["{00000000000000A4}Sounds/X/Samples/d.wav"] },
  ];
  {
    const text = sound.acpText(events);
    ok(sound.acpProblems(text).length === 0, `acp: every link names a node the project has (${sound.acpProblems(text)[0]})`);
    for (const e of events) ok(new RegExp(`SoundClass \\{\\s+id \\d+\\s+name "${e.event}"`).test(text) && new RegExp(`ShaderClass \\{\\s+id \\d+\\s+name "${e.event}"`).test(text), `acp: ${e.event} has its sound and its shader`);
    ok((text.match(/BankLocalClass \{/g) || []).length === 3 && /Filename "\{00000000000000A3\}Sounds\/X\/Samples\/c\.wav"\n     Index 1/.test(text), "acp: one bank per event, a second sample at index 1");
    ok(/name "LF_STUN_LP"[\s\S]*?"Loop count" 255\n   "Infinite loop" 1\n   "Termination Fade Out" 150/.test(text), "acp: a loop's bank loops forever and fades out");
    ok(!/name "DRY"[^}]*Infinite loop/.test(text), "acp: a one-shot's bank does not loop");
    ok(text.includes(`outStatePort ${sound.BUS.shots}`) && text.includes(`outStatePort ${sound.BUS.sfx}`) && !text.includes(`outStatePort ${sound.BUS.handling}`), "acp: each sound goes to its own mixer input");
    ok((text.match(/AmplitudeClass : /g) || []).length === 2 && text.includes(sound.RANGE.voice70) && text.includes(sound.RANGE.near22), "acp: one amplitude node per distance curve in use");
    ok(text.includes('res "{B764D803219C775E}Sounds/FinalMix.afm"') && (text.match(/AuxOutClass \{/g) || []).length === 3, "acp: the vanilla final mix and its three reverb sends");
    ok(sound.acpProblems(text.replace(/(ConnectionClass connection \{\s+id )\d+/, "$1999")).some((p) => /node 999/.test(p)), "acp: a link to a node that is not there is flagged");
    ok(sound.acpProblems(text.replace("{00000000000000A1}Sounds/X/Samples/a.wav", "Sounds/X/Samples/a.wav")).some((p) => /not a \{GUID\}/.test(p)), "acp: a sample with no GUID is flagged");
    let threw = "";
    try { sound.acpText([{ ...events[0], samples: [] }]); } catch (e) { threw = e.message; }
    ok(/no samples/.test(threw), "acp: an event with no sample is refused");

    const wm = sound.wavMeta("Sounds/X/Samples/a.wav", "00000000000000A1");
    ok(wm.startsWith('MetaFileClass {\n Name "{00000000000000A1}Sounds/X/Samples/a.wav"') && (wm.match(/WAVResourceClass/g) || []).length === 6 && wm.includes("ResourceTypes/HEADLESS/Sound.conf"), "meta: a sample's .meta is a WAV resource on every platform");
    const am = sound.acpMeta("Sounds/X/X.acp", "00000000000000B1");
    ok(am.includes('Name "{00000000000000B1}Sounds/X/X.acp"') && (am.match(/AudioProjectResourceClass/g) || []).length === 5, "meta: a project's .meta is an audio project resource");
  }

  // ---- what the stage shipped: every sounds.json in the repo against the addon -------------------
  {
    const base = join(ROOT, "tools", "assets");
    for (const slug of readdirSync(base).filter((d) => existsSync(join(base, d, "sounds.json")))) {
      const r = JSON.parse(readFileSync(join(base, slug, "sounds.json"), "utf8"));
      ok(sound.soundProblems(r).length === 0, `${slug}: sounds.json passes (${sound.soundProblems(r)[0]})`);
      ok((r.generations || []).length <= r.budget.generations, `${slug}: ${(r.generations || []).length} generations are within the cap of ${r.budget.generations}`);
      for (const s of r.sounds.filter((x) => x.pick)) for (const n of s.pick) ok((r.generations || []).some((g) => g.id === s.id && g.take === n), `${slug}: pick ${s.id} ${n} is a recorded generation`);
      for (const [project, w] of Object.entries(r.wired || {})) {
        const rel = w.project.slice(18);
        const file = join(ADDON, rel);
        ok(existsSync(file) && existsSync(file + ".meta"), `${slug}: ${rel} and its .meta are in the addon`);
        if (!existsSync(file)) continue;
        ok(readFileSync(file + ".meta", "utf8").includes(`Name "${w.project}"`), `${slug}: ${rel}.meta carries the recorded GUID`);
        // The project on disk is the generator's output for the recorded events and samples.
        let at = 0;
        const evs = r.sounds.filter((s) => s.project === project && !s.layer_only).map((s) => ({ ...s, samples: w.samples.slice(at, (at += s.pick.length)) }));
        ok(readFileSync(file, "utf8") === sound.acpText(evs), `${slug}: ${rel} is what the generator writes from sounds.json (rerun --only wire)`);
        for (const ref of w.samples) {
          const wav = join(ADDON, ref.slice(18));
          ok(existsSync(wav) && existsSync(wav + ".meta"), `${slug}: sample ${ref.slice(18)} and its .meta are in the addon`);
          if (!existsSync(wav)) continue;
          const s = evs.find((e) => e.samples.includes(ref));
          const problems = sound.checkWav(sound.wavInfo(readFileSync(wav)), { loop: !!s?.loop });
          ok(problems.length === 0, `${slug}: ${ref.slice(18)}: ${problems[0]}`);
          ok(readFileSync(wav + ".meta", "utf8").includes(`Name "${ref}"`), `${slug}: ${ref.slice(18)}.meta carries the recorded GUID`);
        }
      }
    }
  }

  if (failed) {
    console.log(`asset-sound-selftest: FAILED (${failed})`);
    return 1;
  }
  console.log("asset-sound-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
