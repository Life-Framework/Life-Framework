import { parseState } from "../session.mjs";
import { resetClient } from "./_reset.mjs";

// Preconditions a proof sets up for itself, so the order of Saturday's run does not matter (rc-proof-order, 2026-11-27).
// Dry run 4 (docs/playtest/rc-dryrun-4-2026-11-27.md) lost `starter-gates-proof` after the first-two-hours flow on the
// same character, and the booking desk proof once met an officer with no handcuffs. Pure helpers first, so the selftest
// (`session.mjs selftest`) covers them; the two `ensure`/`reset` functions only send verbs.

// ---------- Papers: the Next tab's starters open once the clerk's arrival quest is turned in ----------

// The card quest of the arrival chain. The Next tab's gate (`LF_ActivityGuide.Registered`) reads this turned-in quest,
// not the card in the pack: `resetplayer`, which every proof verb starts with, empties the pack and the card goes with
// it, while the quest stays turned in and the clerk will not issue a second card.
export const PAPERS_QUEST = "arrival_register";

// Pure: whether a `state` line says the character already has its papers (the quest is in `quests=`).
export function hasPapers(stateDetail) {
  const quests = parseState(stateDetail).quests ?? "none";
  return quests.split(",").includes(PAPERS_QUEST);
}

// Pure: what the starter-gates proof must do about the clerk for this `state` line.
export function papersMove(stateDetail) {
  return hasPapers(stateDetail) ? "skip" : "talk";
}

// Every trade with a level track the Next tab reads: Hauling gates the truck check, the best of any trade gates the perk
// pick. CIVILIAN has no levels worth setting. A trade left at level 5 or 10 by an earlier proof opens a starter early.
export const TRADES = ["DELIVERY", "FARMER", "MINER", "LUMBERJACK", "FISHER", "MEDIC", "SMELTING", "SALVAGE", "POLICE"];

// Pure: the `levelset` calls that bring every trade to level 1, as { verb, words } specs.
export function tradeResetSpecs() {
  return TRADES.map((job) => ({ verb: "levelset", words: [job, "1"] }));
}

// Puts every trade back to level 1 (the harness `levelset`, which skips the daily XP cap) so "a new civilian" is true
// whatever ran before. on(n, spec, timeoutMs) -> { status, detail }; wait(ms) blocks. Returns the failed specs.
export function resetTrades(on, wait, n = 1) {
  const failed = [];
  for (const spec of tradeResetSpecs()) {
    const r = on(n, spec, 10000);
    if (r.status !== "ok") failed.push(`${spec.words[0]}: ${r.status} ${r.detail}`);
  }
  wait(2000);
  return failed;
}

// ---------- Handcuffs: an officer who clocks in again gets no second duty kit ----------

export const HANDCUFFS = "{DE0B6FEBDCECC6FF}Prefabs/Items/Roleplay/Handcuffs.et";
export const CUFFS_ATTEMPTS = 4;

// Pure: the next move after `hold Handcuffs` answered `held`, on attempt number `attempt` (0 first). The duty kit may
// still be arriving (wait), a roster account that already had its first-shift kit never gets one again (grant the
// cuffs through the harness), and the proof gives up after CUFFS_ATTEMPTS answers.
export function nextCuffsMove(held, attempt) {
  if (held.status === "ok") return "ready";
  if (attempt + 1 >= CUFFS_ATTEMPTS) return "fail";
  if (/not_carried/.test(String(held.detail)) && attempt === 1) return "grant";
  return "wait";
}

// Makes client n hold handcuffs: holds them, waits for a late kit, grants them through `itemgive` when the pack has
// none. Returns { ok, moves, last }; `moves` is the trail ("wait", "grant", ...) the proof prints on a failure.
export function ensureCuffs(on, wait, n) {
  const moves = [];
  let last;
  for (let attempt = 0; attempt < CUFFS_ATTEMPTS; attempt++) {
    last = on(n, { verb: "hold", item: "Handcuffs" }, 10000);
    const move = nextCuffsMove(last, attempt);
    moves.push(move);
    if (move === "ready") return { ok: true, moves, last };
    if (move === "fail") break;
    if (move === "grant") {
      const give = on(n, { verb: "itemgive", prefab: HANDCUFFS }, 10000);
      moves.push(`give=${give.status}`);
    }
    wait(2500);
  }
  return { ok: false, moves, last };
}

// ---------- Body: jail, cuffs, downed, duty, camera, stance, seat, weight (proof-preconditions-2, 2026-11-28) ----------
// The RC runs failed proofs on what an earlier proof left behind: `jip-sweep-proof` found client 2 still serving the
// 900 s sentence `booking-desk-proof` gave it, and once found client 1's camera on `SCR_ManualCamera` (docs/releases/
// 0.3.0-rc.md, docs/playtest/proof-health-2026-11-27.md). A proof calls `ensureBody(on, wait, n, want)` at its start; it
// reads the character, repairs what it can and answers `{ ok, failed, notes }`, so a leftover is fixed or named, never a
// silent cascade. Reads: `state` (server: jail, cuffed, downed, life, duty, cash) and `bodycheck` (this client: camera,
// stance, seat, weight, maxload). Repairs: `giveup`/respawn (body), `bodycheck fix leave|stand|camera`, `clean`, and
// `resetplayer` (serves or releases a sentence, unlocks cuffs and an escort, clocks out, empties an overweight pack).

export const PLAYER_CAMERA = "PlayerCamera";

// Pure: the move a body that fails a check needs, in the order they are run (a seat before the stance, a reset last of
// the body steps, the camera last). `st` is parseState(state), `body` is parseState(bodycheck), `want` is
// { duty: "off" | "on" | "any" (default "off"), dutyDesk: <entity name that offers LF_DutyAction> }.
export function planBody(st, body, want = {}) {
  const duty = want.duty ?? "off";
  const steps = [];
  const downed = st.downed === "1" || (st.life && st.life !== "ALIVE");
  if (downed) steps.push("revive");
  if (body.seat === "1") steps.push("leave");
  if (body.stance === "PRONE") steps.push("stand");
  const jailed = Number(st.jail || 0) > 0;
  const cuffed = st.cuffed === "1"; // an escort holds the character cuffed (the detain is the restraint), so this covers it
  const dutyWrong = (duty === "off" && st.duty === "1") || (duty === "on" && st.duty !== "1");
  const heavy = isOverweight(body);
  // A reset is the one repair for a sentence, cuffs and an escort; duty off and an overweight pack go with it. Duty on
  // after a reset (or on its own) is the desk's job.
  if (jailed || cuffed || heavy || (duty === "off" && st.duty === "1")) steps.push("reset");
  if (duty === "on" && (dutyWrong || steps.includes("reset"))) steps.push("clockin");
  if (body.camera && body.camera !== PLAYER_CAMERA) steps.push("camera");
  return steps;
}

// Pure: whether a `bodycheck` read says the pack weighs more than the character can carry. No limit read (0) is not heavy.
export function isOverweight(body) {
  const weight = Number(body.weight);
  const max = Number(body.maxload);
  return Number.isFinite(weight) && Number.isFinite(max) && max > 0 && weight > max;
}

// Pure: the failures a character that went through the repairs still shows (`want` as in planBody). Empty = ready.
export function bodyProblems(st, body, want = {}) {
  const duty = want.duty ?? "off";
  const out = [];
  if (st.downed === "1" || (st.life && st.life !== "ALIVE")) out.push(`downed (life=${st.life} downed=${st.downed})`);
  if (Number(st.jail || 0) > 0) out.push(`serving a sentence (jail=${st.jail} s)`);
  if (st.cuffed === "1") out.push("cuffed or escorted");
  if (duty === "off" && st.duty === "1") out.push("on duty, the proof needs off duty");
  if (duty === "on" && st.duty !== "1") out.push("off duty, the proof needs on duty");
  if (body.camera !== undefined && body.camera !== PLAYER_CAMERA) out.push(`camera is ${body.camera}, not ${PLAYER_CAMERA}`);
  if (body.stance === "PRONE") out.push("prone");
  if (body.seat === "1") out.push("seated in a vehicle");
  if (isOverweight(body)) out.push(`overweight (${body.weight} of ${body.maxload} kg)`);
  return out;
}

// Reads the character: `state` then `bodycheck`. { st, body, ok } — `ok` false when either read is refused (no
// character, a build without the verb), with `why`.
export function readBody(on, n) {
  const s = on(n, { verb: "state" }, 10000);
  if (s.status !== "ok") return { ok: false, why: `state: ${s.status} ${s.detail}`, st: {}, body: {} };
  const b = on(n, { verb: "bodycheck", words: [] }, 10000);
  // A build without the verb answers an error: the state-only checks still run, the body checks are skipped and noted.
  const body = b.status === "ok" ? parseState(b.detail) : {};
  return { ok: true, st: parseState(s.detail), body, bodyNote: b.status === "ok" ? "" : `bodycheck: ${b.status} ${b.detail}` };
}

// Makes client n a character the proof can start from. want: { duty: "off"|"on"|"any", dutyDesk }. Returns
// { ok, failed: [named problems left], notes: [what was done] }; the proof stops on `ok: false` and prints `failed`.
export function ensureBody(on, wait, n, want = {}) {
  const notes = [];
  let r = readBody(on, n);
  if (!r.ok) return { ok: false, failed: [r.why], notes };
  if (r.bodyNote) notes.push(r.bodyNote);
  const steps = planBody(r.st, r.body, want);
  if (steps.includes("revive")) {
    // Bodies first: a downed or dead character is settled the way the per-proof reset settles it.
    for (const note of resetClient(on, wait, n, "body")) notes.push(note);
    r = readBody(on, n);
  }
  const act = (step) => steps.includes(step);
  if (act("leave")) { const x = on(n, { verb: "bodycheck", words: ["fix", "leave"] }, 10000); notes.push(`left the seat: ${x.status} ${x.detail}`); wait(1500); }
  if (act("stand")) {
    for (let i = 0; i < 4; i++) {
      const x = on(n, { verb: "bodycheck", words: ["fix", "stand"] }, 10000);
      if (i === 0) notes.push(`stand up: ${x.status} ${x.detail}`);
      wait(2000);
      const again = readBody(on, n);
      if (again.body.stance !== "PRONE") break;
    }
  }
  let reset = false;
  if (act("reset")) {
    // The sentence, cuffs, escort and duty go with a reset; the pack stays unless it is too heavy (then it is emptied and
    // the cash carried stays).
    const spec = { verb: "resetplayer", keepInventory: !isOverweight(r.body) };
    if (!spec.keepInventory) spec.cash = Number(r.st.cash) || 0;
    const x = on(n, spec, 10000);
    notes.push(`reset player (${spec.keepInventory ? "pack kept" : "pack emptied, overweight"}): ${x.status} ${x.detail}`);
    reset = x.status === "ok";
    wait(2000);
  }
  if (act("clockin")) {
    const desk = want.dutyDesk;
    if (!desk) notes.push("clock in wanted, no dutyDesk given: the proof clocks in itself");
    else {
      on(n, { verb: "stand", entityName: desk });
      wait(2500);
      on(n, { verb: "look" });
      const x = on(n, { verb: "use", className: "LF_DutyAction", entityName: desk }, 10000);
      notes.push(`clocked in at ${desk}: ${x.status} ${x.detail}`);
      wait(2500);
    }
  }
  if (act("camera")) {
    on(n, { verb: "clean" }, 10000);
    const x = on(n, { verb: "bodycheck", words: ["fix", "camera"] }, 10000);
    notes.push(`camera back: ${x.status} ${x.detail}`);
    wait(1000);
  }
  r = readBody(on, n);
  let failed = r.ok ? bodyProblems(r.st, r.body, want) : [r.why];
  // A character who ought to be clocked in by the proof itself (no dutyDesk) is not a precondition failure.
  if (want.duty === "on" && !want.dutyDesk) failed = failed.filter((p) => !p.startsWith("off duty"));
  return { ok: failed.length === 0, failed, notes, reset };
}

// Console text for a proof that stops on `ensureBody`.
export function bodyReport(n, result) {
  const lines = [`client ${n}: ${result.ok ? "ready" : "NOT ready"}`];
  for (const note of result.notes) lines.push(`  did: ${note}`);
  for (const bad of result.failed) lines.push(`  left: ${bad}`);
  return lines.join("\n");
}

// What a proof calls at its start: every listed client must be a character it can start from. Prints what was done and
// what is left, and answers false when any client is not ready (the proof then ends with exit code 1 and the named
// leftover instead of a cascade of failed rows). `wants` is one { duty, dutyDesk } for all clients or { [n]: want }.
export function requireBodies(on, wait, clients, wants = {}) {
  let allReady = true;
  for (const n of clients) {
    const want = wants[n] ?? ("duty" in wants || "dutyDesk" in wants ? wants : {});
    const r = ensureBody(on, wait, n, want);
    console.log(bodyReport(n, r));
    if (!r.ok) allReady = false;
  }
  if (!allReady) console.log("FAIL  precondition: a client is not in a state the proof can start from (see the lines above)");
  return allReady;
}

export function selftestPreconditions() {
  const f = [];
  const eq = (a, b, what) => { if (JSON.stringify(a) !== JSON.stringify(b)) f.push(`${what}: got ${JSON.stringify(a)}, want ${JSON.stringify(b)}`); };

  // Papers: read from the quest list, not the card.
  eq(hasPapers("cash=200 idcard=none quests=arrival_register,arrival_garage"), true, "papers turned in, card wiped");
  eq(hasPapers("cash=200 idcard=Ada_Lovelace quests=arrival_register"), true, "papers with the card");
  eq(hasPapers("cash=200 idcard=none quests=none"), false, "fresh character");
  eq(hasPapers("cash=200 idcard=none quests=arrival_register_x"), false, "a quest id that only starts like the papers quest");
  eq(hasPapers("cash=200 idcard=Ada_Lovelace"), false, "a state line with no quests field");
  eq(hasPapers(""), false, "an empty state line");
  eq(papersMove("quests=arrival_done,arrival_register"), "skip", "registered character skips the clerk");
  eq(papersMove("quests=none"), "talk", "fresh character talks to the clerk");

  // Trades.
  const specs = tradeResetSpecs();
  eq(specs.length, TRADES.length, "one levelset per trade");
  eq(specs.every((s) => s.verb === "levelset" && s.words[1] === "1"), true, "every levelset goes to level 1");
  eq(specs.some((s) => s.words[0] === "DELIVERY"), true, "Hauling is reset");
  eq(TRADES.includes("CIVILIAN"), false, "CIVILIAN has no trade to reset");
  {
    const sent = [];
    const bad = resetTrades((n, spec) => { sent.push(spec.words[0]); return spec.words[0] === "MINER" ? { status: "error", detail: "no_session" } : { status: "ok", detail: "" }; }, () => {});
    eq(sent.length, TRADES.length, "resetTrades asks for every trade");
    eq(bad.length, 1, "resetTrades reports the refused trade");
  }

  // Handcuffs: the move after each answer.
  eq(nextCuffsMove({ status: "ok", detail: "held Handcuffs" }, 0), "ready", "cuffs in the pack are held at once");
  eq(nextCuffsMove({ status: "error", detail: "not_carried" }, 0), "wait", "a first miss waits for a late kit");
  eq(nextCuffsMove({ status: "error", detail: "not_carried" }, 1), "grant", "a second miss grants the cuffs");
  eq(nextCuffsMove({ status: "error", detail: "no_character" }, 1), "wait", "no character is not a missing pair of cuffs");
  eq(nextCuffsMove({ status: "error", detail: "not_carried" }, CUFFS_ATTEMPTS - 1), "fail", "the last miss fails");
  eq(nextCuffsMove({ status: "ok", detail: "held Handcuffs" }, CUFFS_ATTEMPTS - 1), "ready", "the last attempt can still succeed");

  // ensureCuffs end to end against a pack that gets its cuffs from the grant.
  {
    let carried = false;
    const sent = [];
    const on = (n, spec) => {
      sent.push(spec.verb);
      if (spec.verb === "itemgive") { carried = true; return { status: "ok", detail: "" }; }
      return carried ? { status: "ok", detail: "held Handcuffs" } : { status: "error", detail: "not_carried" };
    };
    const r = ensureCuffs(on, () => {}, 1);
    eq(r.ok, true, "a pack with no cuffs ends up holding the granted pair");
    eq(sent, ["hold", "hold", "itemgive", "hold"], "hold, hold again, grant, hold");
  }
  {
    const r = ensureCuffs(() => ({ status: "error", detail: "not_carried" }), () => {}, 1);
    eq(r.ok, false, "cuffs that never arrive fail the proof step");
    eq(r.moves.includes("grant"), true, "the grant was tried before giving up");
  }
  {
    let holds = 0;
    const r = ensureCuffs((n, spec) => { holds++; return { status: "ok", detail: "held Handcuffs" }; }, () => {}, 1);
    eq([r.ok, holds], [true, 1], "an officer with cuffs costs one hold and no grant");
  }

  // Body: the plan for each leftover, one at a time and together.
  const clean = { st: { life: "ALIVE", downed: "0", jail: "0", cuffed: "0", duty: "0" }, body: { camera: "PlayerCamera", stance: "STAND", seat: "0", weight: "12", maxload: "40" } };
  const plan = (st, body, want) => planBody({ ...clean.st, ...st }, { ...clean.body, ...body }, want);
  eq(plan({}, {}), [], "a clean character needs nothing");
  eq(plan({ jail: "781" }, {}), ["reset"], "a running sentence is released");
  eq(plan({ cuffed: "1" }, {}), ["reset"], "cuffs (and an escort) are taken off");
  eq(plan({ downed: "1", life: "INCAPACITATED" }, {}), ["revive"], "a downed body is revived");
  eq(plan({ life: "DEAD" }, {}), ["revive"], "a dead body is respawned");
  eq(plan({ duty: "1" }, {}), ["reset"], "on duty with the proof wanting off duty clocks out");
  eq(plan({ duty: "1" }, {}, { duty: "any" }), [], "duty is left alone when the proof does not care");
  eq(plan({ duty: "0" }, {}, { duty: "on" }), ["clockin"], "off duty with the proof wanting on duty clocks in");
  eq(plan({ duty: "1" }, {}, { duty: "on" }), [], "on duty already is not touched when on duty is wanted");
  eq(plan({ jail: "40", duty: "1" }, {}, { duty: "on" }), ["reset", "clockin"], "a reset that clocks out is followed by a clock in");
  eq(plan({}, { camera: "SCR_ManualCamera" }), ["camera"], "a manual camera is switched back");
  eq(plan({}, { camera: "PlayerCamera" }), [], "the player camera stays");
  eq(plan({}, { stance: "PRONE" }), ["stand"], "a prone body stands up");
  eq(plan({}, { stance: "CROUCH" }), [], "crouching is not prone");
  eq(plan({}, { seat: "1" }), ["leave"], "a seated body gets out");
  eq(plan({}, { weight: "55.5", maxload: "40" }), ["reset"], "an overweight pack is emptied");
  eq(plan({}, { weight: "40", maxload: "40" }), [], "exactly at the limit is under it");
  eq(plan({}, { weight: "99", maxload: "0" }), [], "no limit read is not heavy");
  eq(plan({ downed: "1", life: "INCAPACITATED", jail: "5" }, { seat: "1", stance: "PRONE", camera: "SCR_ManualCamera" }), ["revive", "leave", "stand", "reset", "camera"], "everything at once runs in order");
  eq(bodyProblems(clean.st, clean.body), [], "a clean character has no problems");
  eq(bodyProblems({ ...clean.st, jail: "9", cuffed: "1" }, { ...clean.body, camera: "SCR_ManualCamera" }).length, 3, "three leftovers name three problems");
  eq(bodyProblems({ ...clean.st, duty: "0" }, clean.body, { duty: "on" })[0], "off duty, the proof needs on duty", "a missing duty is named");
  eq(bodyProblems(clean.st, {}), [], "a build with no bodycheck verb only checks the state line");

  // ensureBody against a fake character that carries every leftover at once.
  const world = (over = {}) => {
    const w = { st: { life: "ALIVE", downed: "0", jail: "781", cuffed: "1", duty: "1", cash: "250" }, body: { camera: "SCR_ManualCamera", stance: "PRONE", seat: "1", weight: "55", maxload: "40" }, sent: [], ...over };
    const on = (n, spec) => {
      w.sent.push(spec.words ? `${spec.verb} ${spec.words.join(" ")}`.trim() : spec.verb);
      const line = (o) => Object.entries(o).map(([k, v]) => `${k}=${v}`).join(" ");
      if (spec.verb === "state") return { status: "ok", detail: line(w.st) };
      if (spec.verb === "bodycheck" && spec.words.length === 0) return { status: "ok", detail: line(w.body) };
      if (spec.verb === "bodycheck" && spec.words[1] === "leave") { w.body.seat = "0"; return { status: "ok", detail: "left the vehicle" }; }
      if (spec.verb === "bodycheck" && spec.words[1] === "stand") { w.body.stance = "STAND"; return { status: "ok", detail: "asked" }; }
      if (spec.verb === "bodycheck" && spec.words[1] === "camera") { w.body.camera = "PlayerCamera"; return { status: "ok", detail: "now PlayerCamera" }; }
      if (spec.verb === "resetplayer") { Object.assign(w.st, { jail: "0", cuffed: "0", duty: "0" }); if (!spec.keepInventory) w.body.weight = "3"; w.resetSpec = spec; return { status: "ok", detail: "reset" }; }
      return { status: "ok", detail: "" };
    };
    return { w, on };
  };
  {
    const { w, on } = world();
    const r = ensureBody(on, () => {}, 2);
    eq(r.ok, true, "every leftover is repaired");
    eq([w.st.jail, w.st.cuffed, w.st.duty, w.body.camera, w.body.stance, w.body.seat], ["0", "0", "0", "PlayerCamera", "STAND", "0"], "the fake character ends clean");
    eq([w.resetSpec.keepInventory, w.resetSpec.cash], [false, 250], "an overweight pack is emptied and the cash carried stays");
    eq(w.sent.includes("clean"), true, "the clean-shot camera is ended before the player camera is set");
  }
  {
    const { w, on } = world({ body: { camera: "PlayerCamera", stance: "STAND", seat: "0", weight: "5", maxload: "40" } });
    const r = ensureBody(on, () => {}, 1);
    eq(r.ok, true, "a sentence alone is repaired");
    eq(w.resetSpec.keepInventory, true, "a sentence reset keeps the pack");
    eq(w.sent.includes("bodycheck fix camera"), false, "no camera fix when the camera is right");
  }
  {
    // A reset that does not take (the verb is refused): the problems are named, not hidden.
    const on = (n, spec) => {
      if (spec.verb === "state") return { status: "ok", detail: "life=ALIVE downed=0 jail=600 cuffed=0 duty=0" };
      if (spec.verb === "bodycheck") return { status: "ok", detail: "camera=PlayerCamera stance=STAND seat=0 weight=1 maxload=40" };
      if (spec.verb === "resetplayer") return { status: "error", detail: "dev_gate_closed" };
      return { status: "ok", detail: "" };
    };
    const r = ensureBody(on, () => {}, 2);
    eq(r.ok, false, "a refused reset leaves the proof not ready");
    eq(r.failed[0], "serving a sentence (jail=600 s)", "the failure names the sentence");
  }
  {
    // Duty on with a desk: after a release the officer clocks in again.
    const { w, on } = world({ st: { life: "ALIVE", downed: "0", jail: "300", cuffed: "0", duty: "0", cash: "5" }, body: { camera: "PlayerCamera", stance: "STAND", seat: "0", weight: "1", maxload: "40" } });
    const used = [];
    const wrapped = (n, spec, t) => { used.push(spec.className ?? spec.verb); if (spec.className === "LF_DutyAction") w.st.duty = "1"; return on(n, spec, t); };
    const r = ensureBody(wrapped, () => {}, 2, { duty: "on", dutyDesk: "Desk_X" });
    eq(r.ok, true, "a released officer is on duty again");
    eq(used.includes("LF_DutyAction"), true, "the duty desk was used");
  }
  {
    // Duty on with no desk named: not a failure, the proof clocks in itself.
    const { on } = world({ st: { life: "ALIVE", downed: "0", jail: "0", cuffed: "0", duty: "0", cash: "5" }, body: { camera: "PlayerCamera", stance: "STAND", seat: "0", weight: "1", maxload: "40" } });
    eq(ensureBody(on, () => {}, 2, { duty: "on" }).ok, true, "duty on without a desk is the proof's own step");
  }
  {
    // A downed body goes through the per-proof reset's body phase first.
    let life = "INCAPACITATED";
    const on = (n, spec) => {
      if (spec.verb === "state") return { status: "ok", detail: `life=${life} downed=${life === "ALIVE" ? 0 : 1} jail=0 cuffed=0 duty=0` };
      if (spec.verb === "giveup") { life = "ALIVE"; return { status: "ok", detail: "" }; }
      if (spec.verb === "bodycheck") return { status: "ok", detail: "camera=PlayerCamera stance=STAND seat=0 weight=1 maxload=40" };
      return { status: "ok", detail: "" };
    };
    eq(ensureBody(on, () => {}, 1).ok, true, "a downed body is revived by the body phase");
  }
  return f;
}
