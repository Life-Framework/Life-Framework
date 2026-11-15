// A building's own proof scenario, tools/buildings/<slug>/proof.json, run by `tools\cli building proof <slug>`
// inside a session (agent-box/proofs/building-proof.mjs). Every point is in building space (the prefab's: +Z
// back, -Z the street front, y from the floor origin), so the file survives moving the building. Steps:
//   { "shot": "<name>", "look": [x,y,z], "from": [dx,dy,dz] }  stand at look + from facing look, take a picture
//   { "ui": "<name>" }                                         a picture of the screen as the player sees it
//   { "do": { harness spec }, "expect": "<regex>" }            one harness verb; "$entity" names the placed building
//   { "pickup": "<prefab name>" }                              the F-key pickup (harness `pickup`)
//   { "state": "<as>" }                                        keep the server's `state` fields as <as>.<field>
//   any step may carry "wait": ms (after it), "why" (printed on a failure), "status" (default "ok")
// Then "checks" compare kept values ({ "lt"|"gt"|"eq"|"ne": [a, b] } or { "in": [a, [values]] }, where a
// string naming a kept value is read from it), and "log" reads the server log:
//   { "re": "<regex>", "min": 1 }  or  { "last": "<regex with one group x y z>", "near": [x,y,z], "within": m }
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

//
// A licensed counter (store.counters, an item catalogue outlet behind a licence) has one more step,
// expanded into the plain steps and log lines above before anything runs:
//   { "permit": { "licence": "FIREARM_PERMIT", "look": [x,y,z], "from": [dx,dy,dz], "rows": [0, 3],
//                 "items": ["Handgun_M9", "Magazine_9x19_M9_15rnd_Ball"], "cash": 3000, "refused": "Not without a" } }
// A buyer with cash and no licence stands at the counter (look/from), orders the price-list rows and is
// refused with the slip naming the missing licence; `licence` grants it (the /licence path, dev worlds and
// harness builds only); the same order is then paid and each item is delivered to the inventory.
export const scenarioFile = (root, slug) => join(root, "tools", "buildings", slug, "proof.json");

export function loadScenario(root, slug) {
  const f = scenarioFile(root, slug);
  return existsSync(f) ? expandScenario(JSON.parse(readFileSync(f, "utf8"))) : null;
}

// Pure: one permit step as plain steps and the server-log lines that prove it.
export function permitSteps(p) {
  const tag = String(p.licence || "").toLowerCase();
  const order = (when, expect, why) => [
    { do: { verb: "use", className: "LF_ShopAction" }, wait: 2000, why: `${when}: the F key opens the counter's price list` },
    ...(p.rows || []).map((r) => ({ do: { verb: "click", widget: `Item${r}Row_Action` }, wait: 600, why: `${when}: price-list row ${r} on the order` })),
    { do: { verb: "click", widget: "PayButton" }, wait: 2500, why: `${when}: pay for the order` },
    { do: { verb: "ui", mode: "text" }, ...(expect ? { expect } : {}), why },
    { ui: `permit_${tag}_${when.replace(/\W+/g, "_")}` },
    { do: { verb: "input", action: "MenuBack" }, wait: 1000 },
  ];
  const steps = [
    { do: { verb: "resetplayer", cash: p.cash ?? 3000 }, wait: 1500, why: `a buyer with cash and no ${p.licence}` },
    { shot: `permit_${tag}_counter`, look: p.look, from: p.from, why: "standing at the counter" },
    ...order("without the licence", p.refused ?? "Not without a", "the order slip names the missing licence"),
    { do: { verb: "licence", name: p.licence }, wait: 2000, why: `/licence ${p.licence} grants the licence (dev worlds)` },
    ...order("with the licence", null, "the order slip after paying"),
  ];
  const re = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const log = [
    { re: "trade refused at outlet \\S+: \\S+ lacks NEEDS_LICENCE", why: `the counter refused the order without the ${p.licence}` },
    { re: `/licence: player \\d+ granted ${re(p.licence || "")}`, why: `the ${p.licence} was granted` },
    ...(p.items || []).map((it) => ({ re: `bought item=\\S*/${re(it)}\\.et`, why: `${it} paid for and in the inventory with the ${p.licence}` })),
  ];
  return { steps, log };
}

// Pure: a scenario with every permit step expanded (the file's own log lines first).
export function expandScenario(sc) {
  const steps = [];
  const log = [...(sc.log || [])];
  for (const s of sc.steps || []) {
    if (!("permit" in s)) {
      steps.push(s);
      continue;
    }
    const e = permitSteps(s.permit || {});
    steps.push(...e.steps);
    log.push(...e.log);
  }
  return { ...sc, steps, log, permits: (sc.steps || []).filter((s) => "permit" in s).map((s) => s.permit) };
}

// The problems of a permit step's fields (before it is expanded).
export function permitProblems(p, at = "permit") {
  const out = [];
  const v3 = (a) => Array.isArray(a) && a.length === 3 && a.every((n) => typeof n === "number");
  if (!/^[A-Z][A-Z0-9_]*$/.test(p?.licence || "")) out.push(`${at}: licence must be an LF_ELicenseType name such as FIREARM_PERMIT`);
  if (!v3(p?.look) || !v3(p?.from)) out.push(`${at}: look [x,y,z] and from [dx,dy,dz] put the buyer at the counter`);
  if (!Array.isArray(p?.rows) || !p.rows.length || p.rows.some((r) => !Number.isInteger(r) || r < 0)) out.push(`${at}: rows must list the price-list rows to order (0 is the first)`);
  if (!Array.isArray(p?.items) || !p.items.length || p.items.some((i) => !/^[A-Za-z0-9_]+$/.test(i))) out.push(`${at}: items must name the prefab files (no .et) the order delivers`);
  return out;
}

// Building space -> world, for an entity at origin turned by yaw (Enfusion: +90 turns +Z onto +X).
export function toWorld(place, [x, y, z], offset = false) {
  const r = ((place.yaw || 0) * Math.PI) / 180;
  const c = Math.cos(r);
  const s = Math.sin(r);
  const wx = x * c + z * s;
  const wz = -x * s + z * c;
  if (offset) return [wx, y, wz].map((v) => +v.toFixed(3));
  return [place.x + wx, place.y + y, place.z + wz].map((v) => +v.toFixed(3));
}

// The problems of a scenario file, before a session is spent on it.
export function scenarioProblems(sc) {
  const out = [];
  const v3 = (a) => Array.isArray(a) && a.length === 3 && a.every((n) => typeof n === "number");
  for (const [i, p] of (sc?.permits || []).entries()) out.push(...permitProblems(p, `permit ${i}`));
  for (const [i, s] of (sc?.steps || []).entries()) {
    const at = `steps[${i}]`;
    const kinds = ["shot", "ui", "do", "pickup", "state"].filter((k) => k in s);
    if (kinds.length !== 1) out.push(`${at}: one of shot, ui, do, pickup, state (has ${kinds.join(", ") || "none"})`);
    if ("shot" in s && !(v3(s.look) && v3(s.from))) out.push(`${at}: a shot needs look [x,y,z] and from [dx,dy,dz]`);
    if ("do" in s && !s.do?.verb) out.push(`${at}: do needs a harness spec with a verb`);
    if (s.expect) try { new RegExp(s.expect); } catch { out.push(`${at}: expect is not a regex`); }
  }
  for (const [i, c] of (sc?.checks || []).entries()) {
    const op = ["lt", "gt", "eq", "ne", "in"].find((k) => k in c);
    if (!op || !Array.isArray(c[op]) || c[op].length !== 2) out.push(`checks[${i}]: one of lt, gt, eq, ne, in with two operands`);
  }
  for (const [i, l] of (sc?.log || []).entries()) {
    const re = l.re ?? l.last;
    try { new RegExp(re); } catch { out.push(`log[${i}]: re or last must be a regex`); }
    if (l.last && !v3(l.near)) out.push(`log[${i}]: last needs near [x,y,z]`);
  }
  return out;
}

// A kept value: "s0.cash" reads kept.s0.cash; anything else is itself.
export function valueOf(kept, a) {
  if (typeof a !== "string" || !/^[A-Za-z_]\w*\.\w+$/.test(a)) return a;
  const [k, f] = a.split(".");
  if (!(k in kept)) return a;
  return kept[k][f] ?? "";
}

// One check against the kept values: null when it holds, else what was seen.
export function checkFails(kept, c) {
  const op = ["lt", "gt", "eq", "ne", "in"].find((k) => k in c);
  const [a, b] = c[op].map((x, i) => (op === "in" && i === 1 ? x : valueOf(kept, x)));
  const num = (v) => (v === "" ? 0 : Number(v));
  const numeric = !Number.isNaN(num(a)) && (op === "in" || !Number.isNaN(num(b)));
  let ok;
  if (op === "lt") ok = numeric && num(a) < num(b);
  else if (op === "gt") ok = numeric && num(a) > num(b);
  else if (op === "eq") ok = numeric && typeof b === "number" ? num(a) === b : String(a) === String(b);
  else if (op === "ne") ok = String(a) !== String(b);
  else ok = b.map(String).includes(String(a));
  return ok ? null : `${c.why || `${op} ${JSON.stringify(c[op])}`}: ${c[op][0]}=${JSON.stringify(a)}${op === "in" ? "" : ` ${c[op][1]}=${JSON.stringify(b)}`}`;
}

// Log expectations against the server log text. Points are building space.
export function logFails(log, rules, place) {
  const fails = [];
  for (const l of rules || []) {
    if (l.re) {
      const n = (log.match(new RegExp(l.re, "g")) || []).length;
      if (n < (l.min ?? 1)) fails.push(`${l.why || l.re}: ${n} line(s) /${l.re}/ in the server log, want ${l.min ?? 1}`);
      continue;
    }
    const spot = [...log.matchAll(new RegExp(l.last, "g"))].pop()?.[1];
    const want = toWorld(place, l.near);
    const near = spot && spot.split(/[ ,]+/).map(Number).every((v, i) => Math.abs(v - want[i]) < (l.within ?? 1.5));
    if (!near) fails.push(`${l.why || l.last}: last at ${spot ?? "none"}, want within ${l.within ?? 1.5} m of ${want.join(" ")}`);
  }
  return fails;
}

// Runs the steps. ctx: { on(spec, timeout) -> {status, detail}, wait(ms), parseState, serverLog(), shot(name, result), place, entity, prefix }.
export function runScenario(sc, ctx) {
  const kept = {};
  const fails = [];
  const fill = (v) => (typeof v === "string" ? v.replaceAll("$entity", ctx.entity) : v);
  for (const [i, s] of sc.steps.entries()) {
    let r;
    if ("shot" in s) {
      const [x, y, z] = toWorld(ctx.place, s.look);
      const [ox, oy, oz] = toWorld(ctx.place, s.from, true);
      r = ctx.on({ verb: "capture", mode: "world", name: `${ctx.prefix}_${s.shot}`, x, y, z, ox, oy, oz }, 30000);
      ctx.shot(s.shot, r);
    } else if ("ui" in s) {
      r = ctx.on({ verb: "capture", mode: "ui", name: `${ctx.prefix}_${s.ui}` }, 25000);
      ctx.wait(1500);
      ctx.shot(s.ui, r);
    } else if ("pickup" in s) {
      r = ctx.on({ verb: "pickup", item: s.pickup }, 15000);
    } else if ("state" in s) {
      r = ctx.on({ verb: "state" }, 10000);
      kept[s.state] = ctx.parseState(r.detail);
    } else {
      r = ctx.on(Object.fromEntries(Object.entries(s.do).map(([k, v]) => [k, fill(v)])), s.timeout ?? 15000);
    }
    const what = s.why || s.shot || s.ui || (s.pickup && `pick up ${s.pickup}`) || s.do?.verb || `step ${i}`;
    if (r.status !== (s.status ?? "ok")) fails.push(`${what}: ${r.status} ${r.detail}`);
    else if (s.expect && !new RegExp(s.expect).test(r.detail)) fails.push(`${what}: /${s.expect}/ not in "${r.detail}"`);
    if (s.as) kept[s.as] = { detail: r.detail, status: r.status };
    if (s.wait) ctx.wait(s.wait);
  }
  for (const c of sc.checks || []) {
    const f = checkFails(kept, c);
    if (f) fails.push(f);
  }
  fails.push(...logFails(ctx.serverLog(), sc.log, ctx.place));
  return { fails, kept };
}
