// Flow steps and asserts (#2935): a flow line `step <id> <title...> [pending #N]` opens a judged step, and
// `expect <field> <want>` checks the player's `state` line against the one taken when the step opened.
// runFlow (harness.mjs) feeds the results in; flowTable prints the PASS/FAIL table. Pure: no game here.
//
//   expect cash|bank|money +N|-N   changed by exactly N since the step opened (money = cash + bank)
//   expect cash|bank|money up|down|same
//   expect cash|bank|money =N      is exactly N now
//   expect job <NAME>              job= is that LF_EJobType name (PARCEL, UNEMPLOYED, ...)
//   expect duty police|medic|off   on duty in that faction, or off duty
//   expect item <prefab text>      a carried item's prefab path contains the text (the `carries` verb)
//   expect state <key> set|unset|=V|~T   a state-line field (idcard, parked, quests, ...): set is anything but
//                                  none/0/empty, unset is none/0/empty, =V is exactly V, ~T contains T

const MONEY_FIELDS = new Set(["cash", "bank", "money"]);

// "faction=CIVILIAN duty=0 cash=100 ..." -> { faction: "CIVILIAN", duty: "0", cash: "100", ... }
export function parseState(text) {
  const out = {};
  for (const m of String(text ?? "").matchAll(/(?:^|\s)([a-z]+)=(\S*)/g)) if (!(m[1] in out)) out[m[1]] = m[2];
  return out;
}

function moneyOf(state, field) {
  if (field === "money") return Number(state.cash) + Number(state.bank);
  return Number(state[field]);
}

// `expect ...` words -> { field, want } or throws with the line's problem.
export function parseExpect(words) {
  const [field, ...rest] = words;
  const want = rest.join(" ");
  if (!want) throw new Error(`expect needs a field and a value: expect ${words.join(" ")}`);
  if (MONEY_FIELDS.has(field)) {
    if (!/^([+-]\d+|=\d+|up|down|same)$/.test(want)) throw new Error(`expect ${field} takes +N, -N, =N, up, down or same: ${want}`);
    return { field, want };
  }
  if (field === "job") {
    if (!/^[A-Z][A-Z_]*$/.test(want)) throw new Error(`expect job takes an LF_EJobType name such as PARCEL: ${want}`);
    return { field, want };
  }
  if (field === "duty") {
    if (!/^(police|medic|off)$/.test(want)) throw new Error(`expect duty takes police, medic or off: ${want}`);
    return { field, want };
  }
  if (field === "item") {
    if (!/^[A-Za-z0-9_./-]+$/.test(want)) throw new Error(`expect item takes prefab text (letters, digits, _ . / -): ${want}`);
    return { field, want };
  }
  if (field === "state") {
    const [key, ...tail] = rest;
    const rule = tail.join(" ");
    if (!/^[a-z]+$/.test(key ?? "") || !/^(set|unset|=\S+|~\S+)$/.test(rule)) throw new Error(`expect state takes <key> set|unset|=V|~T: expect ${words.join(" ")}`);
    return { field, key, want: rule };
  }
  throw new Error(`expect field must be cash, bank, money, job, duty, item or state: ${field}`);
}

// One state assert. before/after are parseState objects. "" when it holds, else why not.
export function checkExpect(spec, before, after) {
  const { field, want } = spec;
  if (MONEY_FIELDS.has(field)) {
    const now = moneyOf(after, field);
    if (!Number.isFinite(now)) return `expect ${field} ${want}: no ${field} in the state line`;
    if (want.startsWith("=")) return now === Number(want.slice(1)) ? "" : `expect ${field} ${want}: it is ${now}`;
    const then = moneyOf(before ?? {}, field);
    if (!Number.isFinite(then)) return `expect ${field} ${want}: no ${field} when the step opened`;
    const delta = now - then;
    const shown = `${field} ${then} -> ${now} (${delta >= 0 ? "+" : ""}${delta})`;
    if (want === "up") return delta > 0 ? "" : `expect ${field} up: ${shown}`;
    if (want === "down") return delta < 0 ? "" : `expect ${field} down: ${shown}`;
    if (want === "same") return delta === 0 ? "" : `expect ${field} same: ${shown}`;
    return delta === Number(want) ? "" : `expect ${field} ${want}: ${shown}`;
  }
  if (field === "state") {
    const have = after[spec.key];
    const empty = have === undefined || have === "" || have === "none" || have === "0";
    const shown = `expect state ${spec.key} ${want}: ${spec.key}=${have ?? "?"}`;
    if (want === "set") return empty ? shown : "";
    if (want === "unset") return empty ? "" : shown;
    if (want.startsWith("=")) return have === want.slice(1) ? "" : shown;
    return String(have ?? "").includes(want.slice(1)) ? "" : shown;
  }
  if (field === "job") return after.job === want ? "" : `expect job ${want}: job=${after.job ?? "?"}`;
  if (field === "duty") {
    const on = after.duty === "1";
    if (want === "off") return on ? `expect duty off: on duty as ${after.faction}` : "";
    if (on && after.faction === want.toUpperCase()) return "";
    return `expect duty ${want}: duty=${after.duty ?? "?"} faction=${after.faction ?? "?"}`;
  }
  throw new Error(`checkExpect cannot judge ${field} from the state line`);
}

// `step 6b Buy the firearm permit pending #2932` -> { id: "6b", title: "Buy the firearm permit", pending: "#2932" }
export function parseStep(words) {
  const [id, ...rest] = words;
  if (!id || !/^[0-9A-Za-z_-]{1,12}$/.test(id)) throw new Error(`step needs an id (1-12 of letters, digits, _ -): step ${words.join(" ")}`);
  let title = rest.join(" ");
  let pending = "";
  const m = title.match(/\s*pending\s+(#\d+)\s*$/);
  if (m) {
    pending = m[1];
    title = title.slice(0, m.index);
  }
  return { id, title, pending, fails: [] };
}

// The table a judged flow ends with; code 1 when any step failed.
export function flowTable(name, steps) {
  if (!steps.length) return { code: 0, lines: [] };
  const lines = ["", `${name}: step results`];
  const w = Math.max(4, ...steps.map((s) => s.id.length));
  for (const s of steps) {
    const verdict = s.fails.length ? "FAIL" : "PASS";
    const note = s.fails.length ? `  <- ${s.fails[0]}${s.fails.length > 1 ? ` (+${s.fails.length - 1} more)` : ""}` : "";
    const pending = s.pending ? `  [pending ${s.pending}]` : "";
    // "step FAIL" rows are what the compact runner (tools/lib/quiet-run.mjs) shows as failures; the
    // lower-case tally below is its kept line and names the passing steps.
    lines.push(`step ${verdict}  ${s.id.padEnd(w)}  ${s.title}${pending}${note}`);
  }
  const passed = steps.filter((s) => !s.fails.length).map((s) => s.id);
  const failed = steps.length - passed.length;
  lines.push(`flow ${name}: ${passed.length} pass${passed.length ? ` (${passed.join(", ")})` : ""}, ${failed} fail of ${steps.length} steps`);
  return { code: failed ? 1 : 0, lines };
}

export function selftest() {
  const fail = (msg) => {
    throw new Error(`flow-asserts selftest: ${msg}`);
  };
  const s0 = parseState("faction=CIVILIAN duty=0 wanted=0 rank=0 cash=100 bank=1000 job=UNEMPLOYED org=none toast=none");
  if (s0.cash !== "100" || s0.job !== "UNEMPLOYED" || s0.faction !== "CIVILIAN") fail(`parseState read ${JSON.stringify(s0)}`);
  const paid = parseState("faction=CIVILIAN duty=0 cash=140 bank=1000 job=PARCEL");
  const at = (words, before, after) => checkExpect(parseExpect(words.split(" ")), before, after);
  if (at("cash +40", s0, paid) !== "") fail("cash +40 after a $40 run did not pass");
  if (!/cash 100 -> 140 \(\+40\)/.test(at("cash +20", s0, paid))) fail("cash +20 after +40 did not fail with the numbers");
  if (at("cash up", s0, paid) !== "" || at("cash down", s0, paid) === "") fail("cash up/down");
  if (at("bank same", s0, paid) !== "" || at("money +40", s0, paid) !== "") fail("bank same / money +40");
  if (at("cash =140", s0, paid) !== "" || at("cash =100", s0, paid) === "") fail("cash =N");
  if (at("job PARCEL", s0, paid) !== "" || at("job PARCEL", s0, s0) === "") fail("job PARCEL");
  const police = parseState("faction=POLICE duty=1 cash=0 bank=0 job=POLICE");
  if (at("duty police", s0, police) !== "" || at("duty medic", s0, police) === "" || at("duty off", s0, police) === "") fail("duty on as police");
  if (at("duty off", police, s0) !== "" || at("duty police", police, s0) === "") fail("duty off");
  for (const bad of ["cash 40", "cash", "job parcel", "duty sheriff", "item a b!", "mood happy", "state idcard", "state idcard maybe"]) {
    let threw = false;
    try {
      parseExpect(bad.split(" "));
    } catch {
      threw = true;
    }
    if (!threw) fail(`expect ${bad} was accepted`);
  }
  const arrived = parseState("cash=200 bank=2000 idcard=Ada_Lovelace quests=arrival_register parked=none");
  const onState = (w) => at(`state ${w}`, s0, arrived);
  if (onState("idcard set") !== "" || onState("parked set") === "" || onState("parked unset") !== "" || onState("idcard unset") === "") fail("expect state set/unset");
  if (onState("idcard =Ada_Lovelace") !== "" || onState("idcard =Ada") === "" || onState("quests ~arrival_register") !== "" || onState("quests ~arrival_garage") === "") fail("expect state =V/~T");
  if (!/idcard=Ada_Lovelace/.test(onState("idcard =Bob"))) fail("expect state names the value it read");
  if (parseExpect(["item", "Shirt"]).field !== "item") fail("expect item Shirt");
  const st = parseStep(["6b", "Buy", "the", "permit", "pending", "#2932"]);
  if (st.id !== "6b" || st.title !== "Buy the permit" || st.pending !== "#2932") fail(`parseStep read ${JSON.stringify(st)}`);
  const ok = { ...parseStep(["1", "Spawn"]) };
  const bad = { ...parseStep(["2", "Clerk", "pending", "#2932"]), fails: ["stand: entity_not_found"] };
  const t = flowTable("island-first-hour", [ok, bad]);
  if (t.code !== 1 || !t.lines.some((l) => /^step PASS +1 +Spawn$/.test(l)) || !t.lines.some((l) => /^step FAIL +2 +Clerk +\[pending #2932\] +<- stand: entity_not_found/.test(l))) fail(`table:\n${t.lines.join("\n")}`);
  if (t.lines[t.lines.length - 1] !== "flow island-first-hour: 1 pass (1), 1 fail of 2 steps") fail(`tally: ${t.lines[t.lines.length - 1]}`);
  if (flowTable("x", [ok]).code !== 0 || flowTable("x", []).lines.length !== 0) fail("an all-pass or step-less flow is not exit 0");
  return "flow-asserts selftest ok";
}
