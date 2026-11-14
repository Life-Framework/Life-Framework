// Config harness (#298): runs owner-config fixtures through staged checks, offline.
//
//   tools\cli config-harness [--stage parse|validate|all] [--dir <fixtures>] [--json]
//
// Fixtures live in tools/test/config-fixtures/: presets/<name>.json (a legal file, no problems) and
// broken/<reject-class>.json (one reject class each). Every fixture has a sidecar <name>.expect.json:
//   { "exit": 0|1, "parse": "ok"|"fail", "problems": [ { "level", "line", "col", "msg" } ] }
// `msg` is the exact line the server logs (the file is named config.json in every message), so a
// reworded message or a moved position fails here. Exit 0 when every fixture matches.
//
// Stages are an ordered registry. A stage gets one fixture and returns failure strings. The later
// issues add theirs without touching these two:
//   #299  "boot":   a fixture whose sidecar says `"boot": true` gets its own fresh dedicated server
//   #300  "client": a client joins, the config is served to it, and a value it can see is read back
// A stage with `needs: "server"` or `"client"` is skipped here unless the caller passes
// `--stage <name>` explicitly, because it needs a game. Red-proof: tools/validation/config-harness-selftest.mjs.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { checkText, parseJson, JsonSyntaxError } from "./check.mjs";
import { DEFAULT_SOURCES } from "../economy/profit-loops.mjs";

const here = dirname(fileURLToPath(import.meta.url));
export const FIXTURES = join(here, "..", "test", "config-fixtures");
const BASELINE = join(here, "..", "validation", "profit-loop-baseline.json");
export const SOURCE = "config.json";

/** Every fixture: { name, kind: "preset"|"broken", file, text, expect }. */
export function loadFixtures(dir = FIXTURES) {
  const out = [];
  for (const [sub, kind] of [["presets", "preset"], ["broken", "broken"]]) {
    const d = join(dir, sub);
    if (!existsSync(d)) continue;
    for (const f of readdirSync(d).filter((n) => n.endsWith(".json") && !n.endsWith(".expect.json")).sort()) {
      const name = f.slice(0, -5);
      const exp = join(d, `${name}.expect.json`);
      out.push({
        name: `${sub}/${name}`, kind, file: join(d, f), text: readFileSync(join(d, f), "utf8"),
        expect: existsSync(exp) ? JSON.parse(readFileSync(exp, "utf8")) : null,
      });
    }
  }
  return out;
}

const baseline = () => (existsSync(BASELINE) ? JSON.parse(readFileSync(BASELINE, "utf8")) : []);

// The client stage (#300). A fixture's sidecar may carry `"client": [ read, ... ]`; each read names a
// value a player can see and what it must be:
//   { "kind": "shopPrice",   "shop": "Shop_GeneralStore_Debug", "item": "Water", "expect": "$17" }
//       the shop's price list, searched for `item`, shows `expect`
//   { "kind": "actionShown", "entity": "Bank_Teller", "class": "LF_ATMAction", "expect": true|false }
//       standing at `entity`, the player is (not) offered an action of `class` within reach
// The stage does not touch a game itself: `driver.read(fx, read)` returns what the client showed, and the
// driver that boots nothing but talks to a running session is agent-box/proofs/config-harness-proof.mjs.
// Without a driver every fixture that has reads fails, so the stage never passes by being skipped.
let clientDriver = null;
export function setClientDriver(driver) { clientDriver = driver; }
let bootDriver = null;
export function setBootDriver(driver) { bootDriver = driver; }

/** Judges one read against what the client showed. `got` is the text (shopPrice) or a boolean (actionShown). */
export function judgeRead(read, got) {
  if (read.kind === "shopPrice") return String(got).includes(read.expect) ? "" : `${read.item} in ${read.shop}: expected ${read.expect}, the client showed ${JSON.stringify(String(got))}`;
  if (read.kind === "actionShown") return got === read.expect ? "" : `${read.class} near ${read.entity}: expected ${read.expect ? "offered" : "hidden"}, the client ${got ? "offers it" : "does not offer it"}`;
  return `unknown read kind ${read.kind}`;
}

// The boot stage (#299). Some values a reload cannot apply: the autosave timer is scheduled once at game
// start, a system the world switches off at boot, the pack overlays from the mission header, persistence
// switches. A fixture whose sidecar has `"boot": true` is booted on a fresh dedicated server instead of
// being streamed to the shared one (the client stage, #300, which is faster and stays for the reloadable
// ones). The sidecar also carries the server log it must produce:
//   "bootLog": { "has": [ "text a line of the log contains", ... ], "absent": [ "text no line may contain" ] }
// Run by `tools\cli config-harness --stage boot` (tools/config/boot-run.mjs): per fixture `session up
// --clients 0 --seed config.json` on whichever pool machine has room, `session config-boot-proof` reads the
// server log there, `session down`.
export const isBoot = (fx) => fx.expect?.boot === true;

/** Judges a server log against a boot fixture's `bootLog`; returns failure strings. */
export function judgeBootLog(boot, logText) {
  const fails = [];
  const text = String(logText ?? "");
  for (const want of boot?.has ?? []) if (!text.includes(want)) fails.push(`boot: the server log has no line with "${want}"`);
  for (const no of boot?.absent ?? []) if (text.includes(no)) fails.push(`boot: the server log has a line with "${no}" and must not`);
  if (!(boot?.has?.length || boot?.absent?.length)) fails.push("boot: the sidecar has no bootLog (has/absent)");
  return fails;
}

export const STAGES = [
  {
    name: "parse", needs: null,
    // JSON syntax only: the same reader the owner check uses.
    run(fx) {
      let ok = true;
      try { parseJson(fx.text); } catch (e) { if (!(e instanceof JsonSyntaxError)) throw e; ok = false; }
      const want = fx.expect.parse ?? "ok";
      return (ok ? "ok" : "fail") === want ? [] : [`parse: expected ${want}, got ${ok ? "ok" : "fail"}`];
    },
  },
  {
    name: "validate", needs: null,
    // config-check over the fixture: the problem list must equal the expected one, in order.
    run(fx) {
      const { problems } = checkText(fx.text, { source: SOURCE, sources: DEFAULT_SOURCES, baseline: baseline() });
      const fails = [];
      const want = fx.expect.problems ?? [];
      const key = (p) => `${p.level} ${p.line}:${p.col} ${p.msg}`;
      const got = problems.map(key), exp = want.map(key);
      for (const g of got) if (!exp.includes(g)) fails.push(`validate: unexpected  ${g}`);
      for (const e of exp) if (!got.includes(e)) fails.push(`validate: missing     ${e}`);
      const code = problems.some((p) => p.level === "error") ? 1 : 0;
      if (code !== (fx.expect.exit ?? 0)) fails.push(`validate: exit ${code}, expected ${fx.expect.exit ?? 0}`);
      return fails;
    },
  },
  {
    name: "boot", needs: "server",
    // The server log of a server booted on this fixture; the driver (boot-run.mjs) boots, reads and stops.
    run(fx) {
      if (!isBoot(fx)) return [];
      if (!bootDriver) return ["boot: no server driver (run `tools\cli config-harness --stage boot`)"];
      return judgeBootLog(fx.expect.bootLog, bootDriver.log(fx));
    },
  },
  {
    name: "client", needs: "client",
    run(fx) {
      if (isBoot(fx)) return [];
      const reads = fx.expect.client ?? [];
      if (!reads.length) return [];
      if (!clientDriver) return ["client: no client attached (run agent-box/session.mjs config-harness-proof on `session up --clients 1`)"];
      const fails = [];
      clientDriver.apply(fx);
      for (const r of reads) {
        const why = judgeRead(r, clientDriver.read(fx, r));
        if (why) fails.push(`client: ${why}`);
      }
      return fails;
    },
  },
];

/** Runs `stages` over `fixtures`; returns [{ fixture, stage, fails }] for every failing pair. */
export function runHarness(fixtures, stages) {
  const out = [];
  for (const fx of fixtures) {
    if (!fx.expect) { out.push({ fixture: fx.name, stage: "load", fails: [`no ${fx.name}.expect.json`] }); continue; }
    for (const st of stages) {
      const fails = st.run(fx);
      if (fails.length) out.push({ fixture: fx.name, stage: st.name, fails });
    }
  }
  return out;
}

export function main(args) {
  const val = (f) => { const i = args.indexOf(f); return i >= 0 ? args[i + 1] : undefined; };
  const want = val("--stage") ?? "all";
  const known = STAGES.map((s) => s.name);
  if (want !== "all" && !known.includes(want)) {
    console.log(`config-harness: unknown stage '${want}' (${known.join(", ")}, all)`);
    return 2;
  }
  if (want === "boot") {
    return import("./boot-run.mjs").then((m) => m.runBootStage(val("--dir"), val("--only") ?? ""));
  }
  if (want === "client") {
    console.log("config-harness: the client stage needs a running game: `tools\cli session up --clients 1`, then `tools\cli session config-harness-proof`, then `session down`");
    return 2;
  }
  const stages = want === "all" ? STAGES.filter((s) => !s.needs) : STAGES.filter((s) => s.name === want);
  const fixtures = loadFixtures(val("--dir"));
  if (!fixtures.length) { console.log("config-harness: no fixtures found"); return 2; }
  const bad = runHarness(fixtures, stages);
  const broken = fixtures.filter((f) => f.kind === "broken").length;
  if (args.includes("--json")) console.log(JSON.stringify({ stages: stages.map((s) => s.name), fixtures: fixtures.length, failures: bad }, null, 2));
  else {
    for (const b of bad) for (const f of b.fails) console.log(`FAIL  ${b.fixture} [${b.stage}]  ${f}`);
    console.log(`config-harness: ${fixtures.length} fixture(s) (${broken} broken), stage ${stages.map((s) => s.name).join("+")}: ${bad.length ? `${bad.length} failing` : "OK"}`);
  }
  return bad.length ? 1 : 0;
}

if (process.argv[1] && /[\\/]harness\.mjs$/.test(process.argv[1])) process.exit(main(process.argv.slice(2)));
