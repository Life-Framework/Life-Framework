import { parseState } from "../session.mjs";

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
  return f;
}
