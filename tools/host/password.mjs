// The logic of `tools\cli host password rotate|show [<host dir>]` (#1415).
//
// The alpha gate is one server password (docs/community/alpha.md, #1291). It lives only in the
// host's secrets.json (game.password). `rotate` writes a new readable password there, re-applies
// the profile's server.json through `host config apply` (config-apply.mjs, #1322) and returns the
// invite line for the testers; `show` returns the current one. The password never goes into a repo
// file: config-apply refuses a host dir inside the repo and a secret found in server/flagship.
//
// The join password is only enforced by Bohemia's backend (#1520), so a change takes effect on a
// backend-registered server after a restart.
import { existsSync, mkdirSync, readFileSync, renameSync, rmSync, writeFileSync } from "node:fs";
import { randomInt } from "node:crypto";
import { dirname, join, relative, resolve, isAbsolute } from "node:path";
import { fileURLToPath } from "node:url";
import { applyConfig } from "./config-apply.mjs";
import { pidAlive, readHostState } from "./host.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const INVITE_PAGE = "docs/community/alpha.md";

// 64 short, common, easy-to-say words: three of them plus two digits is about 24 bits, enough for
// a gate the backend rate-limits and the maintainer rotates by hand.
const WORDS = [
  "amber", "apple", "arrow", "badge", "baker", "beach", "birch", "bison", "bread", "brook", "cabin", "candy",
  "cedar", "chair", "cliff", "cloud", "coral", "crane", "daisy", "delta", "eagle", "ember", "fable", "fern",
  "field", "flint", "frost", "grove", "hazel", "heron", "honey", "ivory", "jolly", "kayak", "lemon", "lilac",
  "maple", "marsh", "meadow", "mint", "noble", "ocean", "olive", "otter", "pearl", "pine", "plum", "quail",
  "raven", "river", "robin", "sandy", "slate", "sunny", "swift", "tulip", "umber", "valley", "walnut", "willow",
  "wheat", "yarrow", "zebra", "birdie",
];

// A readable join password: three words and two digits, "amber-otter-pine-47".
export function generatePassword() {
  const words = [0, 1, 2].map(() => WORDS[randomInt(WORDS.length)]);
  return `${words.join("-")}-${String(randomInt(100)).padStart(2, "0")}`;
}

const secretsPath = (dir) => join(resolve(dir), "secrets.json");

// secrets.json as an object, or { error }. A missing file is an empty object with missing: true.
function readSecrets(dir) {
  const path = secretsPath(dir);
  if (!existsSync(path)) return { value: {}, missing: true };
  try {
    const v = JSON.parse(readFileSync(path, "utf8"));
    if (!v || typeof v !== "object" || Array.isArray(v)) return { error: `secrets.json is not an object (${path})` };
    if (v.game !== undefined && (typeof v.game !== "object" || v.game === null || Array.isArray(v.game))) return { error: `secrets.json game must be an object (${path})` };
    return { value: v, missing: false };
  } catch {
    return { error: `secrets.json is not valid JSON (${path})` };
  }
}

function writeAtomic(path, text) {
  mkdirSync(dirname(path), { recursive: true });
  const tmp = `${path}.tmp-${process.pid}`;
  writeFileSync(tmp, text);
  renameSync(tmp, path);
}

const refuse = (reason) => ({ ok: false, reason });

// The current join password: { ok, password } or { ok: false, reason }.
export function showPassword(dir) {
  const s = readSecrets(dir);
  if (s.error) return refuse(s.error);
  if (s.missing) return refuse(`no secrets.json in ${resolve(dir)}: rotate writes one`);
  const pw = typeof s.value.game?.password === "string" ? s.value.game.password.trim() : "";
  if (!pw) return refuse(`secrets.json has no game.password (${secretsPath(dir)}): rotate sets one`);
  return { ok: true, password: pw };
}

// The line the maintainer pastes to a tester. The wording of the full invite is `docs/community/alpha.md`.
export function inviteLine(serverName, password) {
  return `Join "${serverName}" in the server browser, password: ${password} (how to join: ${INVITE_PAGE})`;
}

// Writes a new game.password to secrets.json, re-applies the config and returns
// { ok, password, invite, lines, warnings, written } or { ok: false, reason }. When the apply
// refuses, secrets.json goes back to what it was (or is removed when rotate created it).
// `generate` and `repoDir` are for the selftest.
export function rotatePassword(dir, { repoDir = join(ROOT, "server", "flagship"), generate = generatePassword } = {}) {
  const host = resolve(dir);
  const rel = relative(ROOT, host);
  if (rel === "" || (!rel.startsWith("..") && !isAbsolute(rel))) {
    return refuse(`the host dir ${host} is inside the repo checkout; a secret there could be committed. Use the host folder outside the repo (LF_HOST_DIR)`);
  }
  const state = readHostState(host);
  if (state && pidAlive(state.pid)) return refuse(`the host is running (pid ${state.pid}): stop it first (tools\\cli host stop)`);

  const s = readSecrets(host);
  if (s.error) return refuse(s.error);
  const path = secretsPath(host);
  const previous = s.missing ? undefined : readFileSync(path, "utf8");

  const password = generate();
  if (typeof password !== "string" || !password.trim()) return refuse("the password generator returned an empty password");
  const next = { ...s.value, game: { ...(s.value.game ?? {}), password } };
  writeAtomic(path, `${JSON.stringify(next, null, 2)}\n`);

  const r = applyConfig(host, { repoDir });
  if (!r.ok) {
    if (previous === undefined) rmSync(path, { force: true });
    else writeAtomic(path, previous);
    return refuse(`config apply refused, secrets.json left as it was: ${r.reason}`);
  }

  let serverName = "Life Framework";
  try {
    const n = JSON.parse(readFileSync(join(repoDir, "server.json"), "utf8"))?.game?.name;
    if (typeof n === "string" && n.trim()) serverName = n.trim();
  } catch {
    // apply already read it; a name is only for the invite line
  }
  return { ok: true, password, invite: inviteLine(serverName, password), lines: r.lines, warnings: r.warnings, written: [path, ...r.written] };
}
