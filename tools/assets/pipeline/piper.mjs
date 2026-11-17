// Piper (MIT, offline) as the dialogue stage's second speech engine (#1959, chapter Story: The relocated #1015).
// A line whose voice says `"engine": "piper"` is made here: free, no key, no character budget.
// The binary and the voice live in tools/film/.tts (`tools\cli film-make --setup` fetches them).
//
// LICENCE RULE: only Piper voices trained from public-domain or otherwise clean data. `en_US-ljspeech` is
// (LJ Speech, public domain). `lessac` and everything fine-tuned from it (`libritts_r`) is NOT, nor are the Ryan
// and HiFi-TTS voices. The allow-list below is the guard; adding a voice means reading its dataset licence first
// and recording it in docs/credits.md "Voice-over".
import { existsSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { spawnSync } from "node:child_process";

export const ENGINE = "piper";
export const ALLOWED = ["en_US-ljspeech-high", "en_US-ljspeech-medium"];
export const TTS_DIR = (root) => join(root, "tools", "film", ".tts");

export const isPiper = (voice) => voice?.engine === ENGINE;

// What is wrong with a piper voice block, as a list (licence first).
export function voiceProblems(voice, where = "voice") {
  const out = [];
  if (!isPiper(voice)) return out;
  if (!ALLOWED.includes(voice.id)) out.push(`${where}: Piper voice '${voice.id}' is not licence-safe for anything public; only ${ALLOWED.join(" or ")} (docs/credits.md "Voice-over")`);
  if (voice.length_scale !== undefined && !(voice.length_scale >= 0.5 && voice.length_scale <= 2)) out.push(`${where}: length_scale must be 0.5 to 2`);
  return out;
}

export function piperPath(root) {
  const p = join(TTS_DIR(root), "piper", process.platform === "win32" ? "piper.exe" : "piper");
  return existsSync(p) ? p : null;
}

// Speaks `text` into the wav file `out`. `run` is injectable so the selftest needs no binary.
export function speak(root, voice, text, out, { run = spawnSync } = {}) {
  const bad = voiceProblems(voice);
  if (bad.length) throw new Error(bad[0]);
  const piper = piperPath(root);
  if (!piper) throw new Error("Piper is not installed. Run: tools\\cli film-make --setup");
  const model = join(TTS_DIR(root), `${voice.id}.onnx`);
  if (!existsSync(model)) throw new Error(`Piper voice file missing: ${model}. Run: tools\\cli film-make --setup`);
  mkdirSync(dirname(out), { recursive: true });
  const r = run(piper, ["-m", model, "--length_scale", String(voice.length_scale ?? 1), "-f", out], { input: text + "\n", cwd: dirname(piper), encoding: "utf8", windowsHide: true, timeout: 120000, maxBuffer: 64 * 1024 * 1024 });
  if (r.error || r.status !== 0 || !existsSync(out)) throw new Error(`piper failed on a ${text.length}-character line: ${(r.error?.message || r.stderr || "").trim().slice(-300)}`);
}
