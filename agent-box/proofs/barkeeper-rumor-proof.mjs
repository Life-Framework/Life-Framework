import { arrive, clientBox, sessionDir } from "../session.mjs";

// #4595: "Heard anything?" at the Chotain bar, one client on `up --mission island --profile full`. The client stands
// by the placed barkeeper (Josselin Vidal), talks, the numbered replies show (the second is "Heard anything?"), and the
// reply is picked with the real menu keys. A neutral stranger is told a plain rumor (the crime rows wait for a friendly
// regular, covered by the `everon/rumor-standing` test). The proof fails unless the barkeeper offers the replies, and it
// captures the reply list and the answer.
//   node agent-box/session.mjs barkeeper-rumor-proof
const BARKEEPER = "Fit_chotain_pub_bar_barkeeper";

async function run() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const scene = (arg, value) => on({ verb: "dialogue", arg, value }, 10000).detail;
  const shot = (name) => on({ verb: "capture", mode: "ui", name: `barkeeper_rumor_${name}` }, 25000);
  const fails = [];

  arrive((n, spec, t) => on(spec, t), 1, wait);
  // Stand ends the hospital wake-up and answers once the barkeeper has streamed in (#4611), so one talk is enough.
  const stood = on({ verb: "stand", entityName: BARKEEPER, side: "w" }, 20000);
  if (stood.status !== "ok") {
    console.log(`FAIL  stand at the barkeeper: ${stood.status} ${stood.detail}`);
    return 1;
  }

  const use = on({ verb: "use", className: "LF_DialogueTalkAction", entityName: BARKEEPER }, 10000);
  if (use.status !== "ok") fails.push(`the talk returned ${use.status} ${use.detail}`);
  // Wait out the greeting (it is spoken before the replies show); a closed scene means the talk missed.
  let replies = "";
  for (let waited = 0; waited < 30000 && !/phase=choosing|^closed/.test(replies); waited += 2000) {
    wait(2000);
    replies = String(scene());
  }

  if (!/phase=choosing replies=2/.test(replies)) fails.push(`the barkeeper offers two replies and Goodbye (got: ${replies})`);
  shot("1_replies");

  // The replies are "Do you buy cartons and vodka?", "Heard anything?" and the Goodbye row: down once, confirm.
  on({ verb: "input", action: "MenuDown" });
  wait(800);
  shot("2_heard_focused");
  on({ verb: "input", action: "MenuSelect" });
  wait(3500);
  const after = String(scene());
  console.log(`after the pick: ${after}`);
  // The hook said nothing wrong: the scene left the reply list (a rumor clip or the nothing-new line plays).
  if (/phase=choosing/.test(after)) fails.push(`the pick did not leave the reply list (got: ${after})`);
  shot("3_rumor_told");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "barkeeper-rumor-proof: FAIL" : "barkeeper-rumor-proof: PASS (read the shots; grep the server log for '[LFDebug:Residents] heard anything')");
  return fails.length ? 1 : 0;
}

export default {
  name: "barkeeper-rumor-proof",
  usage: "barkeeper-rumor-proof   (run in a --mission island --profile full session)",
  run,
};
