import { sessionUid } from "../session.mjs";
import { clientBox, sessionDir } from "../session.mjs";

// `session.mjs staff-approve <client n | account uid> [--client N]` (#1450): the harness verb `staffapprove`
// from one client, which approves that account's open police application through
// LF_PoliceApplicationStaff.ApproveByUid (LF_HarnessStaff). The sender is the caller the staff API checks: a
// client that is not an engine administrator (`up --admin-client N`, harness build) and not on staff.txt gets
// `#LF-PoliceApp_NotAdmin` back. A client number as the target means that client's session identity.
// Exit 0 only when the application was approved.
export function targetUid(arg) {
  if (/^\d{1,2}$/.test(String(arg ?? ""))) return sessionUid(Number(arg));
  return String(arg ?? "");
}

async function run(ctx) {
  const args = ctx.args;
  const ci = args.indexOf("--client");
  const from = ci === -1 ? 1 : Number(args[ci + 1]);
  const target = targetUid(args.find((a, i) => !a.startsWith("--") && args[i - 1] !== "--client"));
  if (!Number.isInteger(from) || from < 1 || !target) {
    console.log("usage: node agent-box/session.mjs staff-approve <client n | account uid> [--client N (the sender, default 1)]");
    return 1;
  }
  const h = await import("../harness.mjs");
  h.setBoxes([clientBox(sessionDir(), from)]);
  const r = h.sendCommand({ verb: "staffapprove", uid: target }, 15000);
  console.log(`client ${from}: staffapprove ${target} -> ${r.status} ${r.detail}`);
  return r.status === "ok" ? 0 : 1;
}

export default {
  name: "staff-approve",
  usage: "staff-approve <client n | account uid> [--client N]",
  noReset: true,
  run,
};
