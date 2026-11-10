import { arrive, clientBox, sessionDir } from "../session.mjs";

// M2 #276 on a running `up --clients 2 --wait` session with client 2 on the roster: the investigation
// chain in the village. Client 2 clocks in and gets a backpack (a bagged drill does not fit a vest). Client 1 is turned away
// by the fence in his yard (a fresh record), is handed a drill (dev gate: a real sale costs $2,500,
// more than a new character carries), is refused then opens the bank vault, and uses a second drill
// on the cash van box. The drill is left at the door; client 2 reads its serial, bags it and logs it
// at the locker, which names the buyer. World shots: the fence, the van stops, the village layout.
// Confirm `[LFDebug:Fence] refused` and `[LFDebug:Serials] ... bought by` in the server log.
async function runVillageProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
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
  const world = (n, name, x, y, z, ox, oy, oz) => on(n, { verb: "capture", mode: "world", name, x, y, z, ox, oy, oz }, 25000);
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000), "clock in");
  wait(2000);

  // The fence in his yard: where he stands, then what a fresh record gets.
  world(1, "m2_271_fence_yard_client1", 186, 1, 311, 6, 1.8, -9);
  on(1, { verb: "stand", entityName: "FenceNpc" });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "capture", mode: "ui", name: "m2_271_fence_village_client1" }, 25000);
  must(on(1, { verb: "use", className: "LF_ShopAction", entityName: "FenceNpc" }, 10000), "fence shop");
  wait(1500);
  on(1, { verb: "capture", mode: "ui", name: "m2_271_fence_list_client1" }, 25000);
  on(1, { verb: "click", widget: "CloseButton" });

  // The vault refuses a bare-handed visitor, then opens to the drill.
  on(1, { verb: "stand", entityName: "Roadmap_Bank_Vault" });
  wait(2500);
  const noTool = on(1, { verb: "use", className: "LF_BankRobAction", entityName: "Roadmap_Bank_Vault" }, 10000);
  wait(700);
  on(1, { verb: "capture", mode: "ui", name: "m2_273_vault_refused_client1" }, 25000);
  wait(5000);
  const give = on(1, { verb: "drill" }, 10000);
  wait(1500);
  const vault = on(1, { verb: "use", className: "LF_BankRobAction", entityName: "Roadmap_Bank_Vault" }, 10000);
  wait(700);
  on(1, { verb: "capture", mode: "ui", name: "m2_273_vault_opened_client1" }, 25000);

  // The cash van on the main street, then its box.
  world(1, "m2_274_van_street_client1", 192, 1.2, 247.5, -9, 1.7, 1.5);
  on(1, { verb: "stand", entityName: "Roadmap_Cash_Van_Box" });
  wait(2500);
  must(on(1, { verb: "drill" }, 10000), "second drill");
  wait(1500);
  const rob = on(1, { verb: "use", className: "LF_HeistRobAction", entityName: "Roadmap_Cash_Van_Box" }, 10000);
  wait(3000);
  on(1, { verb: "capture", mode: "ui", name: "m2_274_van_opened_client1" }, 25000);

  // The officer reads the serial, bags the drill and logs it.
  on(2, { verb: "stand", entityName: "Roadmap_Cash_Van_Box" });
  wait(2500);
  on(2, { verb: "capture", mode: "ui", name: "m2_269_evidence_marker_client2" }, 25000);
  must(on(2, { verb: "use", className: "LF_ReadSerialAction" }, 10000), "read serial");
  wait(1000);
  on(2, { verb: "capture", mode: "ui", name: "m2_272_serial_client2" }, 25000);
  const bag = on(2, { verb: "use", className: "LF_BagEvidenceAction" }, 10000);
  wait(1500);
  on(2, { verb: "stand", entityName: "Roadmap_Police_EvidenceLocker" });
  wait(2500);
  const log = on(2, { verb: "use", className: "LF_LogEvidenceAction", entityName: "Roadmap_Police_EvidenceLocker" }, 10000);
  wait(1500);
  on(2, { verb: "capture", mode: "ui", name: "m2_269_locker_client2" }, 25000);

  // The layout: the fence, the main street and the station from above.
  world(1, "m2_276_layout_street_client1", 192, 0, 247.5, -30, 30, 14);
  if (noTool.status === "ok") fails.push(`the vault opened without a tool: ${noTool.detail}`);
  if (give.status !== "ok") fails.push(`no drill: ${give.status} ${give.detail}`);
  if (vault.status !== "ok") fails.push(`vault returned ${vault.status} ${vault.detail}`);
  if (rob.status !== "ok") fails.push(`van returned ${rob.status} ${rob.detail}`);
  if (bag.status !== "ok") fails.push(`bagging returned ${bag.status} ${bag.detail}`);
  if (log.status !== "ok") fails.push(`logging returned ${log.status} ${log.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "village proof: FAIL" : "village proof: PASS (fence refused, vault refused then opened, van robbed, serial read, drill bagged and logged; confirm the server log)");
  return fails.length ? 1 : 0;
}

export default {
  name: "village-proof",
  usage: "village-proof",
  run: (ctx) => runVillageProof(),
};
