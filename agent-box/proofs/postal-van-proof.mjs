import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// The postal van run (#4088), two clients on `up --mission island --profile full`. Client 2 (the postman) gets the
// driving licence and takes the run at the Saint-Philippe post-box stand: a van appears (client 2 sees it, locked for
// client 1, who has no key and is refused the wheel by the server), the postman loads four sacks (`LF_PostalAction`
// rows), and the run is on (`[LFDebug:Jobs] postal run` lines). Late joiner and the drive to Meaux stay manual (the harness
// cannot drive). Every step prints PASS or FAIL.
//   node agent-box/session.mjs postal-van-proof
const DROP = "Fit_saint_philippe_postal_drops_drop_b21";
const DEPOT = "Fit_saint_philippe_parcel_job_depot";

async function runProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const steps = [];
  const step = (name, ok, why) => { steps.push({ name, ok }); console.log(`${ok ? "PASS" : "FAIL"}  ${name}${ok ? "" : `: ${why}`}`); };

  for (const n of [1, 2]) arrive(on, n, wait);
  for (const n of [1, 2]) { wait(2500); on(n, { verb: "click", widget: "WelcomeGuideDismissButton" }, 10000); }
  on(2, { verb: "licence", name: "DRIVING_LICENCE" }, 10000);
  wait(2000);
  on(2, { verb: "stand", entityName: DEPOT, side: "e" });
  wait(2500);
  const start = on(2, { verb: "use", className: "LF_PostalAction", entityName: DROP }, 10000);
  wait(4000);
  const log0 = serverLog();
  const vanLine = (log0.match(/\[LFDebug:Postal\][^\n]*/g) ?? []).join(" | ");
  // The van streams to the client a moment after it is spawned: poll the client's own copy of the world.
  let seen2 = "";
  for (let i = 0; i < 10; i++) { seen2 = String(on(2, { verb: "seen", radius: 30 }).detail); if (/LF_PostalVan/.test(seen2)) break; wait(2500); }
  const van2 = /veh=[^ ]*(PostalVan)[^ ]*/i.exec(seen2);
  step("the postman takes the run and a van appears", start.status === "ok" && /Postal_Started/.test(seen2), `use=${start.status} ${start.detail}; toast/seen: ${seen2.slice(0, 200)}; jobs log: ${vanLine.slice(0, 200)}`);
  step("the postman sees the van", !!van2, `seen: ${seen2.slice(0, 300)}`);
  const waypoint = /waypoint=([^ ]*)/.exec(seen2)?.[1] ?? "-";
  console.log(`waypoint after the start: ${waypoint}`);

  // Client 1 has no key: it stands at the van and is put out of the driver seat.
  on(1, { verb: "stand", entityName: DEPOT, side: "e" });
  wait(5000);
  const seen1 = String(on(1, { verb: "seen", radius: 25 }).detail);
  const before = (serverLog().match(/entry refused/g) ?? []).length;
  const seat = on(1, { verb: "engine", entityName: "seat" }, 10000);
  wait(4000);
  const after = on(1, { verb: "engine" }, 10000);
  step("a second player without the key is put out of the van", after.status !== "ok" && (serverLog().match(/entry refused/g) ?? []).length > before, `seen: ${seen1.slice(0, 200)}; seat=${seat.status} ${seat.detail}; engine=${after.status} ${after.detail}`);
  if (after.status === "ok") on(1, { verb: "engine", entityName: "out" }, 10000);

  // Four sacks: the SACK row of the stand, one toast each, then the waypoint moves to Meaux.
  let sacked = 0;
  for (let i = 0; i < 4; i++) {
    on(2, { verb: "stand", entityName: DEPOT, side: "e" });
    wait(1500);
    const r = on(2, { verb: "use", className: "LF_PostalAction__1", entityName: DROP }, 10000);
    if (r.status === "ok") sacked++;
    wait(1500);
  }
  const log1 = serverLog();
  const toasts = (log1.match(/notify player 2: #LF-Postal_[A-Za-z_]*/g) ?? []).join(" | ");
  const seenLoaded = String(on(2, { verb: "seen", radius: 30 }).detail);
  console.log(`postal toasts: ${toasts}`);
  console.log(`waypoint after loading: ${/waypoint=([^ ]*)/.exec(seenLoaded)?.[1] ?? "-"}`);
  step("four sacks load (one toast each)", sacked === 4 && /Postal_Sack|Postal_Loaded|Postal_/.test(toasts) && !/NoRun/.test(toasts.split("|").slice(-4).join("")), `sacked=${sacked}; toasts: ${toasts.slice(0, 300)}`);

  const wp = /waypoint=([^ ]*)/.exec(seenLoaded)?.[1] ?? "-";
  step("after the fourth sack the waypoint points at the unload stand (Meaux)", /job-stop/.test(wp) && /postal-unload/.test(log1), `waypoint ${wp}`);

  const failed = steps.filter((s) => !s.ok);
  console.log(failed.length ? `postal van proof: FAIL (${failed.length} of ${steps.length})` : `postal van proof: PASS (${steps.length} steps)`);
  return failed.length ? 1 : 0;
}

export default {
  name: "postal-van-proof",
  usage: "postal-van-proof",
  run: () => runProof(),
};
