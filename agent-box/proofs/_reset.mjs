// Per-proof reset (#496). A proof leaves duty, wanted, faction and a downed or dead body behind;
// the next proof in the same session then fails from the first step (a clock-in toggles off,
// a revive finds a healthy patient). `resetClients` brings every client back to a plain living
// civilian off duty. session.mjs runs it before every proof verb (skip with --no-reset) and
// as `session.mjs reset`.
import { parseState } from "../session.mjs";

const DESKS = { POLICE: "Roadmap_Police_Desk", MEDIC: "Roadmap_Hospital_Duty_Desk" };

// Decides what a client needs from its `state` line. Pure, so selftest covers it.
export function planReset(st) {
  const steps = [];
  if (st.life && st.life !== "ALIVE") steps.push("revive");
  else if (st.downed === "1") steps.push("revive");
  if (st.duty === "1") steps.push("clockout");
  if (Number(st.wanted || 0) > 0) steps.push("wanted");
  if (st.faction && st.faction !== "CIVILIAN" && st.faction !== "none") steps.push("faction");
  return steps;
}

export function selftestReset() {
  const f = [];
  const eq = (a, b) => JSON.stringify(a) === JSON.stringify(b);
  if (!eq(planReset({ life: "ALIVE", downed: "0", duty: "0", wanted: "0", faction: "CIVILIAN" }), [])) f.push("clean state planned work");
  if (!eq(planReset({ life: "INCAPACITATED", downed: "1", duty: "0", wanted: "0", faction: "CIVILIAN" }), ["revive"])) f.push("downed body not revived");
  if (!eq(planReset({ life: "ALIVE", downed: "0", duty: "1", wanted: "3", faction: "POLICE" }), ["clockout", "wanted", "faction"])) f.push("duty, wanted and faction not all planned");
  f.push(...selftestDowner());
  return f;
}

// An earlier proof left client 2 downed by client 1. The reset's give-up is a real death, and the
// murder goes to the downer, client 1 (downed-proof then read wanted=5 = 3 + 3 capped, not 3).
// Client 1 must still end clean, so its wanted is cleared after every body is settled.
function selftestDowner() {
  const st = { 1: { life: "ALIVE", downed: "0", wanted: "0", faction: "CIVILIAN", duty: "0" }, 2: { life: "INCAPACITATED", downed: "1", wanted: "0", faction: "CIVILIAN", duty: "0" } };
  const line = (s) => Object.entries(s).map(([k, v]) => `${k}=${v}`).join(" ");
  let menuFor = 0;
  const on = (n, spec) => {
    if (spec.verb === "state") return { status: "ok", detail: line(st[n]) };
    if (spec.verb === "giveup") {
      st[n].life = "ALIVE";
      st[n].downed = "0";
      st[1].wanted = "3";
    }
    if (spec.verb === "open" && spec.mode === "show") menuFor = n;
    if (spec.verb === "click" && spec.widget === "Wanted0Button") st[menuFor].wanted = "0";
    return { status: "ok", detail: "" };
  };
  resetClients(on, () => {}, 2);
  return st[1].wanted === "0" ? [] : [`client 1 left wanted=${st[1].wanted} by the murder client 2's give-up filed`];
}

// on(n, spec, timeoutMs) -> { status, detail }; wait(ms) blocks. Returns one note per action taken.
// phase "body" only settles a downed or dead body; "rest" only clears duty, wanted and faction.
export function resetClient(on, wait, n, phase = "all") {
  const notes = [];
  const read = () => {
    const r = on(n, { verb: "state" }, 10000);
    return r.status === "ok" ? parseState(r.detail) : null;
  };
  let st = read();
  if (!st) {
    // A death screen has no character to read; its button is the way out.
    on(n, { verb: "click", widget: "RespawnButton" });
    wait(4000);
    notes.push("respawn from the death screen");
    st = read();
    if (!st) return [...notes, "no character after respawn, skipped"];
  }

  for (const step of planReset(st)) {
    if (phase === "body" && step !== "revive") continue;
    if (phase === "rest" && step === "revive") continue;
    if (step === "revive") {
      for (let i = 0; i < 12; i++) {
        st = read();
        if (st && st.life === "ALIVE" && st.downed !== "1") break;
        if (st && st.life === "INCAPACITATED") on(n, { verb: "giveup" }, 10000);
        else on(n, { verb: "click", widget: "RespawnButton" });
        wait(3000);
      }
      notes.push("respawned a downed or dead body");
    } else if (step === "clockout") {
      const desk = DESKS[st.faction];
      if (!desk) {
        notes.push(`on duty as ${st.faction}: no desk known`);
        continue;
      }
      on(n, { verb: "stand", entityName: desk });
      wait(2500);
      on(n, { verb: "look" });
      on(n, { verb: "use", className: "LF_DutyAction", entityName: desk }, 10000);
      wait(2500);
      notes.push(`clocked out of ${st.faction}`);
    } else if (step === "wanted" || step === "faction") {
      // The debug menu is the only way to clear wanted or leave a faction; the playtest mission closes it.
      const open = on(n, { verb: "open", menu: "DebugMenu", mode: "show" });
      if (open.status !== "ok") {
        notes.push(`${step} not reset: the debug menu is closed on this mission`);
        continue;
      }
      wait(800);
      on(n, { verb: "click", widget: step === "wanted" ? "Wanted0Button" : "FactionCivilianButton" });
      on(n, { verb: "open", menu: "DebugMenu", mode: "hide" });
      wait(1200);
      notes.push(step === "wanted" ? "wanted cleared" : "faction set to civilian");
    }
  }

  const after = read();
  if (phase !== "body" && after && planReset(after).length) notes.push(`still dirty: ${planReset(after).join(", ")}`);
  return notes;
}

// Bodies first, on every client, then the rest. Giving up a downed body is a real death and files
// the murder on whoever downed it, often a client with a lower number that a single pass had
// already cleaned (downed-proof read the shooter at wanted=3 before it fired a shot).
export function resetClients(on, wait, count) {
  const out = [];
  for (let n = 1; n <= count; n++) out.push({ n, notes: resetClient(on, wait, n, "body") });
  for (const r of out) r.notes.push(...resetClient(on, wait, r.n, "rest"));
  return out;
}
