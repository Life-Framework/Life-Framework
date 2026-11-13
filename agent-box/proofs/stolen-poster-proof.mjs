import { arrive, clientBox, clientSees, sessionDir } from "../session.mjs";

// Stolen cars on the wanted board's face (docs/architecture/visual-boards.md) on a running
// `up --clients 2 --mission playtest` session (`--profile full` for the pictures): client 1 owns
// the starter S105 and takes it out of the garage, client 2 hotwires it (the car is stolen),
// client 1 reports it; both clients' boards then carry a STOLEN poster with the plate and the car
// (read from `seen`, `posters=<count>/<face>`, and the client logs), and client 1 photographs it.
const BOARD = "Roadmap_Police_WantedBoard";
const BOARD_AT = { x: 235.4, y: 0.555, z: 242.9 };

async function runStolenPosterProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  const postersOf = (s) => {
    const m = /^(\d+)\/(\w+)$/.exec(s?.posters ?? "");
    return m ? { count: Number(m[1]), face: m[2] } : null;
  };
  const expect = (n, label, judge, budgetMs = 15000) => {
    const r = clientSees(on, wait, n, (s) => {
      const p = postersOf(s);
      return p ? judge(p) : [`no posters field: ${s.posters}`];
    }, { label, budgetMs });
    fails.push(...r.fails);
    if (!r.fails.length) console.log(`ok    ${label} (${r.seconds.toFixed(1)} s)`);
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: "Roadmap_Garage" });
  wait(2500);
  must(on(1, { verb: "use", className: "LF_VehicleRetrieveAction", entityName: "Roadmap_Garage" }, 10000), "retrieve the car");
  wait(4000);
  on(2, { verb: "stand", entityName: "Roadmap_Garage" });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "engine", entityName: "car" }, 10000), "seat the thief");
  wait(4000);
  let started;
  for (let attempt = 1; attempt <= 5; attempt++) {
    must(on(2, { verb: "use", className: "LF_HotwireAction" }, 10000), "hotwire");
    wait(17000);
    on(2, { verb: "engine" }, 10000);
    wait(4000);
    started = on(2, { verb: "engine" }, 10000);
    if (started.status === "ok" && started.detail.includes("on=1")) break;
  }
  if (!started || !started.detail.includes("on=1")) fails.push(`the stolen car did not run: ${started && started.detail}`);

  on(1, { verb: "look" });
  must(on(1, { verb: "use", className: "LF_ReportStolenAction" }, 10000), "report the car stolen");
  wait(3000);
  on(2, { verb: "engine", entityName: "out" }, 10000);
  for (const n of [1, 2]) on(n, { verb: "stand", entityName: BOARD });
  // The thief was wanted for the theft: one person and the car.
  for (const n of [1, 2]) expect(n, `client ${n}'s board carries the stolen car`, (p) => (p.count >= 1 ? [] : [`posters=${p.count}/${p.face}`]), 20000);

  // Both characters step out of the picture.
  on(2, { verb: "stand", entityName: "Roadmap_Garage" });
  on(1, { verb: "stand", entityName: "Roadmap_Police_EvidenceLocker" });
  wait(1500);
  on(1, { verb: "camera", args: "hud off" });
  on(1, { verb: "camera", args: "stop" });
  on(1, { verb: "camera", args: `static ${BOARD_AT.x} ${BOARD_AT.y + 0.45} ${BOARD_AT.z + 1.7} ${BOARD_AT.x} ${BOARD_AT.y} ${BOARD_AT.z} 60 50` });
  wait(3000);
  on(1, { verb: "capture", mode: "ui", name: "stolen_poster_client1" }, 25000);
  on(1, { verb: "camera", args: "stop" });
  on(1, { verb: "camera", args: "hud on" });

  const { readdirSync, readFileSync, existsSync } = await import("node:fs");
  const { join } = await import("node:path");
  for (const n of [1, 2]) {
    const logs = join(dir, `client${n}`, "logs");
    const text = existsSync(logs) ? readdirSync(logs).sort().map((d) => join(logs, d, "console.log")).filter(existsSync).map((p) => readFileSync(p, "utf8")).join("\n") : "";
    if (!/WantedPosters\] stolen car poster: picture of [0-9A-F]{16}/.test(text)) fails.push(`client ${n} never drew the stolen car's picture`);
  }
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "stolen poster proof: FAIL" : "stolen poster proof: PASS (a real report puts the car on both boards)");
  return fails.length ? 1 : 0;
}

export default {
  name: "stolen-poster-proof",
  usage: "stolen-poster-proof   (up --clients 2 --mission playtest)",
  run: () => runStolenPosterProof(),
};
