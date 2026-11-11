import { ROB_TARGET, arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// #127: a jailed client works two jobs in the cell. The state line's `jail=` (remaining seconds)
// must drop by each job's seconds once, and a second scrub inside the cooldown must change nothing.
// Client 1 is made wanted (the rob), client 2 clocks in and arrests. Doubles as the demo of jail work.
// `jail-work-proof [--name <shot prefix>]`
async function runJailWorkProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "m2_127_jailwork");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  const t0 = () => Date.now() / 1000;
  const jail = () => Number(parseState(on(1, { verb: "state" }, 10000).detail).jail ?? 0);

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000), "clock in");
  wait(2000);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  must(on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000), "rob");
  wait(7000);
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(4000);
  // The arrest action shows only once the officer has the suspect in reach; retry a few times.
  let arrest;
  for (let i = 0; i < 4; i++) {
    on(2, { verb: "look" });
    arrest = on(2, { verb: "use", className: "LF_JailArrestAction", entityName: "Player1" }, 10000);
    if (arrest.status === "ok") break;
    wait(2000);
  }
  must(arrest, "arrest");
  wait(4000);

  // The sentence is 60 s (wanted 1) and also ticks one second per second, so the proof reads the clock:
  // drop = seconds off + wall time. The scrub then takes the rest and the prisoner is released.
  const start = jail();
  const t1 = t0();
  if (!(start > 30)) fails.push(`sentence too short to prove work: jail=${start}`);

  const work = (name, spot, seconds) => {
    on(1, { verb: "stand", entityName: spot });
    wait(1500);
    on(1, { verb: "look" });
    must(on(1, { verb: "use", className: "LF_JailWorkAction", entityName: spot }, 10000), name);
    wait(2000);
    on(1, { verb: "capture", mode: "ui", name: `${prefix}_${name}_working` }, 25000);
    wait(seconds * 1000);
  };

  work("laundry", "Roadmap_Jail_LaundryBasket", 5);
  wait(1500);
  const afterLaundry = jail();
  const wall = t0() - t1;
  const taken = start - afterLaundry - wall;
  console.log(`jail after laundry: ${start} -> ${afterLaundry} (wall ${wall.toFixed(1)} s, so about ${taken.toFixed(1)} s taken off)`);
  if (!(taken >= 14 && taken <= 26)) fails.push(`laundry did not take about 20 s off (${start} -> ${afterLaundry}, wall ${wall.toFixed(1)})`);
  on(1, { verb: "capture", mode: "ui", name: `${prefix}_laundry_done` }, 25000);

  // A repeat inside the cooldown is refused with the cooldown toast and takes nothing off.
  on(1, { verb: "use", className: "LF_JailWorkAction", entityName: "Roadmap_Jail_LaundryBasket" }, 10000);
  wait(1500);
  const repeat = on(1, { verb: "state" }, 10000).detail;
  if (!repeat.includes("toast=#LF-JailWork_Cooldown")) fails.push(`a repeat inside the cooldown was not refused (${repeat.split(" ").pop()})`);
  on(1, { verb: "capture", mode: "ui", name: `${prefix}_cooldown` }, 25000);

  work("scrub", "Roadmap_Jail_ScrubBucket", 6);
  wait(2000);
  const afterScrub = jail();
  console.log(`jail after scrub: ${afterLaundry} -> ${afterScrub}`);
  if (afterScrub !== 0) fails.push(`the scrub did not finish the sentence (jail=${afterScrub}, it should have had under 30 s left)`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "jail work proof: FAIL" : "jail work proof: PASS (the laundry took about 20 s off, the cooldown refused a repeat, the scrub ended the sentence)");
  return fails.length ? 1 : 0;
}

export default {
  name: "jail-work-proof",
  usage: "jail-work-proof [--name <shot prefix>]",
  run: (ctx) => runJailWorkProof(),
};
