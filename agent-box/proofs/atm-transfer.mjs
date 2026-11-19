import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// #3320 on a running `up --clients 2 --profile full` session: the ATM's transfer page. Client 2 opens the page and
// reads its own number ("YOUR NUMBER: ..."); client 1 opens the page, types that number and 50 and sends. The page
// says it was sent to the last four characters, client 2's bank balance rises by 50 and client 1's falls by 50 plus the
// fee. Client 1 then types a number nobody holds and the page says so in words; no money moves. Both pages are
// captured for the PR (--profile full). The toast shows on client 1's HUD at the same moment as the status line.
async function runAtmTransferProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const bank = (n) => Number(parseState(on(n, { verb: "state" }, 10000).detail).bank);
  const uiText = (n) => on(n, { verb: "ui", mode: "text" }, 10000).detail || "";
  const field = (text, kind, name) => new RegExp(`${kind}:${name}=([^|]*)`).exec(text)?.[1]?.trim() ?? "";
  const fails = [];

  const openPage = (n) => {
    on(n, { verb: "stand", entityName: "Bank_Teller", side: "s" }); // town-layout-ok: the teller is the one ATM action a session can reach; the proof only stands at it
    wait(2500);
    on(n, { verb: "look" });
    const use = on(n, { verb: "use", className: "LF_ATMAction", entityName: "Bank_Teller" } /* town-layout-ok: same teller */, 10000);
    if (use.status !== "ok") fails.push(`client ${n}: the teller returned ${use.status} ${use.detail}`);
    wait(1500);
    on(n, { verb: "click", widget: "TransferButton" });
    wait(800);
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  // Client 2's number, as the page prints it.
  openPage(2);
  const own = field(uiText(2), "line", "YourNumber").replace(/^YOUR NUMBER:\s*/i, "");
  if (!own) fails.push("client 2's page does not show its own number");
  console.log(`client 2 number: ${own}`);
  on(2, { verb: "click", widget: "CloseButton" });
  wait(500);

  const senderBefore = bank(1);
  const receiverBefore = bank(2);

  // The good transfer: 50 to client 2's number.
  openPage(1);
  on(1, { verb: "type", widget: "Target", text: own });
  on(1, { verb: "click", widget: "Key5Button" });
  on(1, { verb: "click", widget: "Key0Button" });
  wait(300);
  on(1, { verb: "capture", mode: "ui", name: "atm_transfer_page" }, 25000);
  on(1, { verb: "click", widget: "SendButton" });
  wait(2500);
  const sentText = uiText(1);
  const status = field(sentText, "line", "Status");
  const last4 = own.slice(-4);
  if (!status.includes(last4) || !/SENT/i.test(status)) fails.push(`client 1's page does not report the transfer to ...${last4}: "${status}"`);
  on(1, { verb: "capture", mode: "ui", name: "atm_transfer_sent" }, 25000);
  const senderAfter = bank(1);
  const receiverAfter = bank(2);
  console.log(`sender ${senderBefore} -> ${senderAfter}, receiver ${receiverBefore} -> ${receiverAfter}`);
  if (receiverAfter !== receiverBefore + 50) fails.push(`client 2's bank rose by ${receiverAfter - receiverBefore}, not 50`);
  if (senderAfter > senderBefore - 50) fails.push(`client 1's bank fell by ${senderBefore - senderAfter}, not at least 50`);

  // A number nobody holds: refused in words, nothing moves.
  on(1, { verb: "click", widget: "KeyClearButton" });
  on(1, { verb: "type", widget: "Target", text: "0000" });
  on(1, { verb: "click", widget: "Key1Button" });
  on(1, { verb: "click", widget: "Key0Button" });
  on(1, { verb: "click", widget: "SendButton" });
  wait(2000);
  const refused = field(uiText(1), "line", "Status");
  if (!/no account/i.test(refused)) fails.push(`a bad number does not show the refusal: "${refused}"`);
  on(1, { verb: "capture", mode: "ui", name: "atm_transfer_refused" }, 25000);
  if (bank(1) !== senderAfter || bank(2) !== receiverAfter) fails.push("a refused transfer moved money");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "atm transfer proof: FAIL" : "atm transfer proof: PASS (confirm the server log carries [LFDebug:BankTransfer] transfer ... sent 50)");
  return fails.length ? 1 : 0;
}

export default {
  name: "atm-transfer",
  usage: "atm-transfer",
  run: () => runAtmTransferProof(),
};
