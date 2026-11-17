// Proves the flagship's two live files (#1321): server/flagship/config.json passes the owner-config check and
// carries the decided values (2-hour in-game day, restarts at 06:00 and 18:00 with 15/5/1 minute warnings, rejoin at the saved spot, 30 minutes of new-character grace);
// server/flagship/server.json has the listing name and the same scenario and mods as server/example; neither
// file holds a secret (passwords, admin uids, a webhook URL). Secrets live only in the host's secrets.json.
// red-proof: adding game.password, a webhook URL or a changed name to a fixture copy below prints a FAIL line.
import { cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { checkText } from "../config/check.mjs";

const NAME = "Life Framework | Everon 1989 (Alpha)";
const SECRET_KEY = /password|passwd|secret|token|webhook|apikey|api_key|credential/i;

export function flagshipProblems(root = ".") {
  const out = [];
  const dir = join(root, "server", "flagship");
  const load = (f) => {
    const p = join(dir, f);
    if (!existsSync(p)) { out.push(`server/flagship/${f} is missing`); return null; }
    try { return { text: readFileSync(p, "utf8"), json: JSON.parse(readFileSync(p, "utf8")) }; } catch (e) { out.push(`server/flagship/${f} is not JSON: ${e.message}`); return null; }
  };
  const cfg = load("config.json");
  const srv = load("server.json");
  const ex = JSON.parse(readFileSync(join(root, "server", "example", "server.json"), "utf8"));

  // A secret anywhere in either file: a non-empty value under a secret-looking key, or any URL value.
  const scan = (v, path, file) => {
    if (Array.isArray(v)) v.forEach((x, i) => scan(x, `${path}[${i}]`, file));
    else if (v && typeof v === "object") for (const [k, x] of Object.entries(v)) {
      if (SECRET_KEY.test(k) && x !== "" && x !== null && !(Array.isArray(x) && x.length === 0)) out.push(`${file} ${path}.${k} holds a value; secrets go only in the host's secrets.json`);
      scan(x, `${path}.${k}`, file);
    } else if (typeof v === "string" && /https?:\/\//i.test(v)) out.push(`${file} ${path} holds a URL; webhook URLs are host-only`);
  };

  if (cfg) {
    try {
      for (const p of checkText(cfg.text, { source: "config.json" }).problems ?? []) if (p.level === "error") out.push(`config.json ${p.line}:${p.col} ${p.msg}`);
    } catch (e) { out.push(`config.json could not be checked: ${e.message}`); }
    const c = cfg.json;
    if (c.dayCycle?.mode !== "accelerated" || c.dayCycle?.dayLengthMinutes !== 120) out.push("config.json dayCycle must be accelerated with dayLengthMinutes 120 (a 2 hour day)");
    if (JSON.stringify(c.restarts?.times) !== JSON.stringify(["06:00", "18:00"])) out.push("config.json restarts.times must be [\"06:00\", \"18:00\"]");
    if (JSON.stringify(c.restarts?.warningMinutes) !== JSON.stringify([15, 5, 1])) out.push("config.json restarts.warningMinutes must be [15, 5, 1]");
    // #1779 / #2075: returning players rejoin at the saved spot; a new character has 30 minutes of grace.
    if (c.respawn?.rejoinAtLastPosition !== true) out.push("config.json respawn.rejoinAtLastPosition must be true (rejoin at the saved spot, #1779)");
    if (c.grace?.minutes !== 30) out.push("config.json grace.minutes must be 30 (the new-character grace, #2075)");
    scan(c, "$", "config.json");
  }
  if (srv) {
    const g = srv.json.game ?? {};
    if (g.name !== NAME) out.push(`server.json game.name must be "${NAME}", got "${g.name}"`);
    if (g.scenarioId !== ex.game.scenarioId) out.push("server.json game.scenarioId differs from server/example");
    if (JSON.stringify(g.mods) !== JSON.stringify(ex.game.mods)) out.push("server.json game.mods differs from server/example");
    if (g.password !== "") out.push("server.json game.password must be empty (the alpha password is host-only)");
    if (g.passwordAdmin !== "") out.push("server.json game.passwordAdmin must be empty (host-only)");
    if (!Array.isArray(g.admins) || g.admins.length) out.push("server.json game.admins must be empty (host-only)");
    scan(srv.json, "$", "server.json");
  }
  return out;
}

if (import.meta.url === `file:///${process.argv[1].replace(/\\/g, "/")}` || process.argv[1]?.endsWith("flagship-config-selftest.mjs")) {
  const fails = [];
  const expect = (ok, msg) => { if (!ok) fails.push(msg); };
  const real = flagshipProblems();
  expect(real.length === 0, `the committed flagship files have problems: ${real.join(" | ")}`);

  const copy = () => {
    const root = mkdtempSync(join(tmpdir(), "lf-flagship-"));
    mkdirSync(join(root, "server"), { recursive: true });
    cpSync("server/flagship", join(root, "server", "flagship"), { recursive: true });
    cpSync("server/example", join(root, "server", "example"), { recursive: true });
    return root;
  };
  const mutate = (file, fn, want) => {
    const root = copy();
    const p = join(root, "server", "flagship", file);
    if (existsSync(p)) writeFileSync(p, fn(readFileSync(p, "utf8")));
    const got = flagshipProblems(root).join(" | ");
    expect(want.test(got), `${file}: expected ${want}, got "${got}"`);
  };
  const json = (fn) => (t) => { const o = JSON.parse(t); fn(o); return JSON.stringify(o, null, 2) + "\n"; };
  mutate("server.json", json((o) => { o.game.password = "hunter2"; }), /game\.password must be empty/);
  mutate("server.json", json((o) => { o.game.passwordAdmin = "CHANGE-ME"; }), /passwordAdmin must be empty/);
  mutate("server.json", json((o) => { o.game.admins = ["0123456789abcdef"]; }), /game\.admins must be empty/);
  mutate("server.json", json((o) => { o.game.name = "My server"; }), /game\.name must be/);
  mutate("server.json", json((o) => { o.game.mods.pop(); }), /game\.mods differs/);
  mutate("server.json", json((o) => { o.game.scenarioId = "{0000000000000000}Missions/X.conf"; }), /scenarioId differs/);
  mutate("config.json", json((o) => { o.staff = { webhookUrl: "https://discord.com/api/webhooks/1/x" }; }), /webhookUrl holds a value|holds a URL/);
  mutate("config.json", json((o) => { o.shops.note = "https://example.org/x"; }), /holds a URL/);
  mutate("config.json", json((o) => { o.dayCycle.dayLengthMinutes = 1440; }), /dayCycle must be/);
  mutate("config.json", json((o) => { delete o.restarts; }), /restarts\.times/);
  mutate("config.json", json((o) => { o.respawn.rejoinAtLastPosition = false; }), /rejoinAtLastPosition must be true/);
  mutate("config.json", json((o) => { delete o.grace; }), /grace\.minutes must be 30/);
  mutate("config.json", json((o) => { o.shops.defaultBuyPrice = "cheap"; }), /config\.json \d+:\d+/);
  mutate("server.json", () => "{", /is not JSON/);

  if (fails.length) {
    for (const f of fails) console.log(`FAIL ${f}`);
    process.exit(1);
  }
  console.log("flagship-config-selftest: OK");
}
