import { arrive, clientBox, sessionDir } from "../session.mjs";

// #481 on a running `up --profile full --clients 1` session: opens the credits screen (what /credits
// opens) and takes one picture of the sheet. Confirm `[LFDebug:Credits] credits screen open with N rows`
// in the client log and no `string key missing` line.
async function runCreditsProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const at = process.argv.indexOf("--name");
  const name = at >= 0 && process.argv[at + 1] ? process.argv[at + 1] : "credits";
  const fails = [];
  arrive(on, 1, wait);
  wait(6000);
  const open = on(1, { verb: "kit", step: "credits" }, 10000);
  wait(2000);
  on(1, { verb: "capture", mode: "ui", name }, 25000);
  if (open.status !== "ok") fails.push(`credits returned ${open.status} ${open.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "credits proof: FAIL" : "credits proof: PASS (confirm the client log for the row count and no missing key)");
  return fails.length ? 1 : 0;
}

export default {
  name: "credits-proof",
  usage: "credits-proof [--name <shot>]",
  run: () => runCreditsProof(),
};
