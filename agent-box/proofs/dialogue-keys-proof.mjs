import { clientBox, sessionDir } from "../session.mjs";

// Landing rate of a digit key in NPC dialogue (#4960 follow-up). On a running session whose client is already
// in a talk (a `flow` that ends on `use LF_QuestAction <clerk>`), it waits for each reply list, waits the given
// delay, presses `input LF_Keypad1` once with no retry and counts a press that leaves the choosing node within
// 3 s as landed. The delays rotate, so one run compares them. Measured 2026-11-25 on a full-profile island
// session: 166 of 166 landed at 0, 250 and 500 ms after the list showed, one client and two.
//   node agent-box/session.mjs up --mission island --profile full --clients 1 --wait
//   node agent-box/harness.mjs --client 1 flow <a flow that registers and ends on the clerk talk>
//   node agent-box/session.mjs dialogue-keys-proof [--client 1] [--delays 0,250,500] [--per 10] [--minutes 9]
// Exit 0 only when every press landed. A dropped press is answered with `dialogue pick 0` so the run goes on.
function numberArg(name, fallback) {
  const at = process.argv.indexOf(name);
  return at >= 0 && process.argv[at + 1] ? Number(process.argv[at + 1]) : fallback;
}

async function runDialogueKeysProof() {
  const h = await import("../harness.mjs");
  const client = numberArg("--client", 1);
  const per = numberArg("--per", 10);
  const budgetMs = numberArg("--minutes", 9) * 60000;
  const at = process.argv.indexOf("--delays");
  const delays = (at >= 0 && process.argv[at + 1] ? process.argv[at + 1] : "0,250,500").split(",").map(Number);
  h.setBoxes([clientBox(sessionDir(), client)]);
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const scene = () => {
    const r = h.sendCommand({ verb: "dialogue" }, 5000);
    const m = /^open node=([\w-]+) phase=(\w+)/.exec(r.detail || "");
    return m ? { node: m[1], phase: m[2] } : { node: "closed", phase: "closed" };
  };
  const tally = Object.fromEntries(delays.map((d) => [d, { landed: 0, dropped: 0 }]));
  const open = (d) => tally[d].landed + tally[d].dropped < per;
  const started = Date.now();
  let turn = 0;
  while (Date.now() - started < budgetMs && delays.some(open)) {
    const before = scene();
    if (before.phase === "closed") {
      console.log("the talk closed before every press was made");
      break;
    }
    if (before.phase !== "choosing") {
      wait(120);
      continue;
    }
    const pending = delays.filter(open);
    const d = pending[turn++ % pending.length];
    wait(d);
    h.sendCommand({ verb: "input", action: "LF_Keypad1" }, 8000);
    let landed = false;
    for (const until = Date.now() + 3000; Date.now() < until && !landed; wait(100)) {
      const now = scene();
      landed = now.node !== before.node || now.phase !== "choosing";
    }
    tally[d][landed ? "landed" : "dropped"]++;
    console.log(`${before.node} +${d} ms: ${landed ? "landed" : "DROPPED"}`);
    if (!landed) h.sendCommand({ verb: "dialogue", arg: "pick", value: "0" }, 10000);
    wait(400);
  }
  let landed = 0;
  let dropped = 0;
  for (const d of delays) {
    landed += tally[d].landed;
    dropped += tally[d].dropped;
    console.log(`delay ${d} ms: ${tally[d].landed} landed, ${tally[d].dropped} dropped`);
  }
  const ok = dropped === 0 && landed > 0;
  console.log(`dialogue-keys-proof: ${ok ? "PASS" : "FAIL"} ${landed}/${landed + dropped} landed`);
  return ok ? 0 : 1;
}

export default {
  name: "dialogue-keys-proof",
  usage: "dialogue-keys-proof [--client N] [--delays 0,250,500] [--per N] [--minutes M]",
  run: () => runDialogueKeysProof(),
};
