import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, parseState, sessionDir, ticketDeadlines } from "../session.mjs";

// M1-19 on a running `up --clients 2` session (roster: player 2): client 2 clocks in and
// writes client 1 a ticket face to face; client 1 reads it and pays at the desk; client 2
// writes a second one that client 1 leaves unpaid past both deadlines (doubled, then
// wanted +1). Needs short deadlines in LF_Police.conf (each 120 s or less) for the run.
// Screenshots: the ticket pad, the ticket card, the paid receipt, the doubled ticket.
async function runTicketProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const conf = readFileSync(join(ROOT, "addons", "LifeFramework", "Configs", "Police", "LF_Police.conf"), "utf8");
  const { first, second } = ticketDeadlines(conf);
  if (first > 120 || second > 120) {
    console.log(`ticket proof: the deadlines are ${first} s and ${second} s; set both to 120 or less in LF_Police.conf for the run`);
    return 1;
  }
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const waitText = (n, text) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.waitForText(text, 10000);
    console.log(`client ${n}: wait-text ${text} -> ${r.status} ${r.found ? "found" : r.detail}`);
    return r;
  };
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const suspect = `Player${state(1).player}`;

  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  must(on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000), "clock in");
  wait(2500);

  const write = (offenceRow, shot) => {
    on(2, { verb: "stand", entityName: suspect });
    wait(2500);
    on(2, { verb: "look" });
    must(on(2, { verb: "use", className: "LF_PlayerRoleAction", entityName: suspect }, 10000), "Write ticket");
    must(waitText(2, "POLICE TICKET"), "the ticket pad");
    if (shot) on(2, { verb: "capture", mode: "ui", name: shot }, 25000);
    on(2, { verb: "ui", mode: "text" });
    must(on(2, { verb: "click", widget: `TicketOffence${offenceRow}` }), `offence row ${offenceRow}`);
    wait(3000);
  };

  // Ticket 1: written, read, paid at the desk.
  const before = state(1);
  write(0, "m1_19_ticket_pad");
  must(waitText(1, "Issued to"), "the ticket card on the holder's screen");
  on(1, { verb: "capture", mode: "ui", name: "m1_19_ticket_card" }, 25000);
  on(1, { verb: "ui", mode: "text" });
  on(1, { verb: "click", widget: "TicketClose" });
  const written = state(1);
  if (written.tickets !== "1") fails.push(`client 1 carries ${written.tickets} unpaid ticket(s) after the write, expected 1`);
  const owed = Number(written.owed);

  on(1, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(1, { verb: "look" });
  must(on(1, { verb: "use", className: "LF_FineTicketPayAction", entityName: "Roadmap_Police_Desk" }, 10000), "Pay ticket");
  must(waitText(1, "PAID"), "the stamped receipt");
  on(1, { verb: "capture", mode: "ui", name: "m1_19_ticket_receipt" }, 25000);
  on(1, { verb: "ui", mode: "text" });
  on(1, { verb: "click", widget: "TicketClose" });
  const paid = state(1);
  if (Number(paid.cash) !== Number(before.cash) - owed) fails.push(`client 1 cash ${before.cash} -> ${paid.cash}, expected -${owed}`);
  if (paid.tickets !== "0") fails.push(`client 1 still carries ${paid.tickets} unpaid ticket(s) after paying`);

  // Ticket 2: left unpaid past both deadlines.
  write(1, null);
  on(1, { verb: "click", widget: "TicketClose" });
  const second1 = state(1);
  const base = Number(second1.owed);
  console.log(`waiting ${first + 5} s of online time for the first deadline`);
  wait((first + 5) * 1000);
  const late = state(1);
  if (Number(late.owed) !== base * 2) fails.push(`after the first deadline the ticket owes ${late.owed}, expected ${base * 2}`);
  if (late.wanted !== second1.wanted) fails.push(`the first deadline changed wanted ${second1.wanted} -> ${late.wanted}`);
  on(1, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "capture", mode: "ui", name: "m1_19_ticket_doubled_at_desk" }, 25000);
  console.log(`waiting ${second + 5} s of online time for the second deadline`);
  wait((second + 5) * 1000);
  const wanted = state(1);
  if (Number(wanted.wanted) !== Number(second1.wanted) + 1) fails.push(`after the second deadline wanted is ${wanted.wanted}, expected ${Number(second1.wanted) + 1}`);

  console.log("");
  console.log(`before         client 1: ${JSON.stringify(before)}`);
  console.log(`written        client 1: ${JSON.stringify(written)}`);
  console.log(`paid           client 1: ${JSON.stringify(paid)}`);
  console.log(`second written client 1: ${JSON.stringify(second1)}`);
  console.log(`first deadline client 1: ${JSON.stringify(late)}`);
  console.log(`second deadln  client 1: ${JSON.stringify(wanted)}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "ticket proof: FAIL" : "ticket proof: PASS (written face to face, paid at the desk in cash, unpaid one doubled then wanted +1)");
  return fails.length ? 1 : 0;
}

export default {
  name: "ticket-proof",
  usage: "ticket-proof",
  run: (ctx) => runTicketProof(),
};
