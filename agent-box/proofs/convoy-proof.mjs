import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// Convoy drop (#616, #1152) on a running `up --mission playtest --wait` session, one client.
//   node agent-box/session.mjs up --mission playtest --wait
//   node agent-box/session.mjs convoy-proof
//   node agent-box/session.mjs down
// The client founds an organization at the registry desk and takes the hideout lease (so it is a member of
// the org that holds the hideout), starts the convoy at the barn pallet, and takes the crate. Standing at
// the pub cage (the flagship drop) the Deliver action must pay nothing: it is not the active drop in
// debug mode. Standing at the debug drop 27 m south of the barn, Deliver pays the configured cash once
// (+200) and a second press pays nothing. The truck-boarding part of the convoy stays manual.
const PAYOUT = 200;

async function runConvoyProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const cash = () => parseState(on(1, { verb: "state" }, 10000).detail).cash;
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };

  arrive(on, 1, wait);
  wait(6000);

  // Found the organization and take the hideout lease, as the org proof does.
  on(1, { verb: "stand", entityName: "Roadmap_Org_Desk" });
  wait(2500);
  must(on(1, { verb: "use", className: "LF_OrganizationDeskAction", entityName: "Roadmap_Org_Desk" }, 10000), "org desk");
  wait(1500);
  must(on(1, { verb: "type", widget: "OrgName", text: "ConvoyProofOutfit" }), "org name");
  must(on(1, { verb: "click", widget: "FoundButton" }), "found org");
  wait(3000);
  must(on(1, { verb: "click", widget: "OrgHideoutRow_Action" }), "take the hideout lease");
  wait(2500);
  on(1, { verb: "click", widget: "CloseButton" });
  wait(1000);

  // Start the convoy at the barn pallet: the crate is handed over.
  on(1, { verb: "stand", entityName: "Roadmap_Convoy_Start" });
  wait(2500);
  must(on(1, { verb: "use", className: "LF_ConvoyStartAction", entityName: "Roadmap_Convoy_Start" }, 10000), "start convoy");
  wait(2500);

  // The pub cage is the flagship drop: inert in debug mode, pays nothing.
  const before = cash();
  on(1, { verb: "stand", entityName: "Roadmap_Convoy_Drop" });
  wait(2500);
  on(1, { verb: "use", className: "LF_ConvoyDeliverAction", entityName: "Roadmap_Convoy_Drop" }, 10000);
  wait(2500);
  const afterPub = cash();
  if (afterPub !== before) fails.push(`the pub cage paid ${afterPub - before} in debug mode (expected 0)`);

  // The debug drop beside the barn pays once.
  on(1, { verb: "stand", entityName: "Roadmap_Convoy_Drop_Near" });
  wait(2500);
  must(on(1, { verb: "use", className: "LF_ConvoyDeliverAction", entityName: "Roadmap_Convoy_Drop_Near" }, 10000), "deliver at the debug drop");
  wait(3000);
  const afterNear = cash();
  if (afterNear - afterPub !== PAYOUT) fails.push(`the debug drop paid ${afterNear - afterPub} (expected ${PAYOUT})`);
  on(1, { verb: "use", className: "LF_ConvoyDeliverAction", entityName: "Roadmap_Convoy_Drop_Near" }, 10000);
  wait(3000);
  const again = cash();
  if (again !== afterNear) fails.push(`a second delivery paid ${again - afterNear} (expected 0)`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(
    fails.length
      ? "convoy-proof: FAIL"
      : `convoy-proof: PASS (pub cage inert, debug drop paid +${PAYOUT} once, no repeat; cash ${before} -> ${afterNear})`,
  );
  return fails.length ? 1 : 0;
}

export default { name: "convoy-proof", usage: "convoy-proof", run: () => runConvoyProof() };
