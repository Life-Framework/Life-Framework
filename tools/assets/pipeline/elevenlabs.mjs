// ElevenLabs client for the dialogue stage (docs/asset-pipeline/dialogue.md).
// Endpoints read from elevenlabs.io/docs/api-reference on 2026-11-13. Speech is generated at
// build time only; nothing in the addon calls this service.
// The key is read at call time from a file outside the repo and never leaves this module:
// no function returns it, logs it, or puts it in an error message.
import { appendFileSync, existsSync, mkdirSync, readFileSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join } from "node:path";

export const BASE = "https://api.elevenlabs.io";
export const KEY_FILE = () => process.env.LF_ELEVENLABS_KEY_FILE || join(homedir(), ".life-framework", "elevenlabs-key");
export const DEFAULT_MODEL = "eleven_multilingual_v2";
// Characters billed per character of text. The flash and turbo models bill half.
export const RATE = { eleven_multilingual_v2: 1, eleven_v3: 1, eleven_flash_v2_5: 0.5, eleven_turbo_v2_5: 0.5 };

export const cost = (text, model = DEFAULT_MODEL) => Math.ceil(text.length * (RATE[model] ?? 1));

function key() {
  const file = KEY_FILE();
  if (!existsSync(file)) throw new Error(`no ElevenLabs key file at ${file} (set LF_ELEVENLABS_KEY_FILE)`);
  const k = readFileSync(file, "utf8").trim();
  if (!k) throw new Error(`ElevenLabs key file ${file} is empty`);
  return k;
}

function scrub(text) {
  let k = "";
  try { k = key(); } catch { /* no key: nothing to scrub */ }
  const s = String(text ?? "");
  return k ? s.split(k).join("<elevenlabs-key>") : s;
}

// One HTTP call, bounded. Returns the Response. `fetchImpl` is injectable so the selftest runs offline.
export async function call(method, path, body, { fetchImpl = fetch, timeoutMs = 60000 } = {}) {
  let res;
  try {
    res = await fetchImpl(BASE + path, {
      method,
      headers: { "xi-api-key": key(), ...(body ? { "Content-Type": "application/json" } : {}) },
      body: body ? JSON.stringify(body) : undefined,
      signal: AbortSignal.timeout(timeoutMs),
    });
  } catch (e) {
    throw new Error(scrub(`ElevenLabs ${method} ${path}: ${e.message}`));
  }
  if (!res.ok) {
    let detail = "";
    try { detail = (await res.text()).slice(0, 300); } catch { /* no body */ }
    throw new Error(scrub(`ElevenLabs ${method} ${path}: HTTP ${res.status} ${detail}`));
  }
  return res;
}

// { tier, used, limit, left }: characters this billing period.
export async function balance(opts) {
  const j = await (await call("GET", "/v1/user/subscription", null, opts)).json();
  return { tier: j.tier, used: j.character_count, limit: j.character_limit, left: j.character_limit - j.character_count };
}

// A voice-library voice must be in the account's own voices before it can speak. Premade voices
// always are. Adding one is free and uses a voice slot.
export async function ensureVoice(voice, opts) {
  const mine = await (await call("GET", "/v1/voices", null, opts)).json();
  if ((mine.voices || []).some((v) => v.voice_id === voice.id)) return "present";
  if (!voice.public_owner_id) throw new Error(`voice ${voice.id} (${voice.name}) is not in the account and the spec gives no public_owner_id to add it from the voice library`);
  await call("POST", `/v1/voices/add/${voice.public_owner_id}/${voice.id}`, { new_name: voice.name }, opts);
  return "added";
}

// Text to speech. Returns { audio: Buffer (mp3), requestId, billed } where billed is what the
// service reports for this call, or the estimate when it reports nothing.
export async function speak(voice, text, opts) {
  const model = voice.model || DEFAULT_MODEL;
  const body = { text, model_id: model };
  if (voice.settings) body.voice_settings = voice.settings;
  const res = await call("POST", `/v1/text-to-speech/${voice.id}?output_format=mp3_44100_128`, body, opts);
  const audio = Buffer.from(await res.arrayBuffer());
  if (audio.length < 1000) throw new Error(`ElevenLabs returned ${audio.length} bytes of audio for a ${text.length}-character line`);
  const reported = Number(res.headers?.get?.("character-cost"));
  return { audio, requestId: res.headers?.get?.("request-id") || "", billed: Number.isFinite(reported) && reported > 0 ? reported : cost(text, model) };
}

export function logChars(ledger, row) {
  mkdirSync(dirname(ledger), { recursive: true });
  appendFileSync(ledger, JSON.stringify({ at: new Date().toISOString(), ...row }) + "\n");
}

export function spent(ledger) {
  if (!existsSync(ledger)) return 0;
  return readFileSync(ledger, "utf8").split("\n").filter(Boolean).reduce((n, l) => n + (JSON.parse(l).chars || 0), 0);
}
