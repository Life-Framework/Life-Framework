// SPIKE (docs/architecture/phone-calls-spike.md): drives the phone-call proof of concept on a
// running `up --clients 2` session with client 2 on the police roster (`session.mjs roster 2`
// before up). It prints what each client reads (`harness phone`); the server side is in the
// server log under [LFDebug:Phone]. It judges nothing: the output is the evidence.
import { arrive, clientBox, sessionDir } from "../session.mjs";

const PAYPHONE = "Roadmap_Fuel_Payphone";
const DISPATCH = "Annex_Dispatch_Console";
const DESK = "Roadmap_Police_Desk";

async function runPhoneProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const step = (t) => console.log(`\n== ${t}`);
  const at = (n, name) => {
    on(n, { verb: "stand", entityName: name });
    wait(2500);
    on(n, { verb: "look" });
  };
  const use = (n, className, entityName) => on(n, { verb: "use", className, entityName }, 10000);
  const phones = () => {
    on(1, { verb: "phone" }, 10000);
    on(2, { verb: "phone" }, 10000);
  };
  const call = (label) => {
    step(`${label}: client 1 dials from ${PAYPHONE}`);
    use(1, "LF_PhoneDialAction", PAYPHONE);
    wait(5000);
    phones();
    step(`${label}: client 2 picks up at ${DISPATCH}`);
    use(2, "LF_PhoneAnswerAction", DISPATCH);
    wait(6000);
    phones();
    wait(5000);
    phones();
    step(`${label}: client 1 hangs up`);
    use(1, "LF_PhoneHangUpAction", PAYPHONE);
    wait(2500);
    phones();
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  step("client 2 clocks in as police, then stands at the dispatch phone; client 1 at the payphone");
  at(2, DESK);
  use(2, "LF_DutyAction", DESK);
  wait(3000);
  at(2, DISPATCH);
  at(1, PAYPHONE);

  call("PROP mode");

  step("switch to CARRIED mode");
  use(2, "LF_PhoneModeAction", DISPATCH);
  call("CARRIED mode");
  use(2, "LF_PhoneModeAction", DISPATCH);

  step("missed call: client 1 dials, nobody picks up for 20 s, then leaves a message");
  use(1, "LF_PhoneDialAction", PAYPHONE);
  wait(24000);
  use(1, "LF_PhoneLeaveMessageAction", PAYPHONE);
  wait(1500);
  step("client 2 reads the messages and calls back; client 1 picks up the ringing payphone");
  use(2, "LF_PhoneMessagesAction", DISPATCH);
  wait(1500);
  use(2, "LF_PhoneCallBackAction", DISPATCH);
  wait(5000);
  phones();
  use(1, "LF_PhoneAnswerAction", PAYPHONE);
  wait(6000);
  phones();
  use(2, "LF_PhoneHangUpAction", DISPATCH);
  wait(2500);

  step("nobody on duty: client 2 clocks out, client 1 dials and leaves a message");
  at(2, DESK);
  use(2, "LF_DutyAction", DESK);
  wait(3000);
  use(1, "LF_PhoneDialAction", PAYPHONE);
  wait(2000);
  use(1, "LF_PhoneLeaveMessageAction", PAYPHONE);
  wait(2000);
  phones();
  console.log("\nphone proof: finished; read the server log for [LFDebug:Phone]");
  return 0;
}

export default {
  name: "phone-proof",
  usage: "phone-proof",
  run: (ctx) => runPhoneProof(),
};
