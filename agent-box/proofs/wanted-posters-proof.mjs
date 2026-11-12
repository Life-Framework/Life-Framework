import { spawnSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ROB_TARGET, ROOT, arrive, clientBox, clientSees, sessionDir } from "../session.mjs";

// The wanted board's face (docs/architecture/visual-boards-spike.md) on a running `up --clients 1`
// session (`--profile full` for the pictures): client 1 robs the fuel till and its own board shows
// the poster; client 2 then joins late, after the change, and must draw it from RplLoad; client 2
// walks out of the draw radius (the face is released) and back (rebuilt); client 1's wanted level
// is cleared and the poster leaves both boards by the broadcast. The face state comes from the
// harness `seen` (`posters=<count>/<live|frozen|released>`); the client logs give the source.
const BOARD = "Roadmap_Police_WantedBoard";
const BOARD_AT = { x: 235.4, y: 0.555, z: 242.9 };
const FAR = "Annex_Police_WantedBoard";

export function postersOf(seen) {
  const m = /^(\d+)\/(\w+)$/.exec(seen?.posters ?? "");
  return m ? { count: Number(m[1]), face: m[2] } : null;
}

function clientLog(dir, n) {
  const logs = join(dir, `client${n}`, "logs");
  if (!existsSync(logs)) return "";
  return readdirSync(logs).sort().map((d) => join(logs, d, "console.log")).filter(existsSync).map((p) => readFileSync(p, "utf8")).join("\n");
}

async function runWantedPostersProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 220)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const expect = (n, label, judge, budgetMs = 15000) => {
    const r = clientSees(on, wait, n, (s) => {
      const p = postersOf(s);
      return p ? judge(p) : [`no posters field: ${s.posters}`];
    }, { label, budgetMs });
    fails.push(...r.fails);
    if (!r.fails.length) console.log(`ok    ${label} (${r.seconds.toFixed(1)} s)`);
    return r;
  };
  // A fixed film camera in front of the board (the character camera cannot pitch down to it), HUD off.
  const shot = (n, name, back = 1.7, lift = 0.45) => {
    // The character steps behind the board, out of the picture.
    on(n, { verb: "stand", entityName: "Roadmap_Police_EvidenceLocker" });
    wait(1500);
    on(n, { verb: "camera", args: "hud off" });
    on(n, { verb: "camera", args: "stop" });
    on(n, { verb: "camera", args: `static ${BOARD_AT.x} ${BOARD_AT.y + lift} ${BOARD_AT.z + back} ${BOARD_AT.x} ${BOARD_AT.y} ${BOARD_AT.z} 60 50` });
    wait(2500);
    const r = on(n, { verb: "capture", mode: "ui", name }, 25000);
    on(n, { verb: "camera", args: "stop" });
    on(n, { verb: "camera", args: "hud on" });
    return r;
  };

  arrive(on, 1, wait);
  wait(6000);
  on(1, { verb: "stand", entityName: BOARD });
  wait(3000);
  expect(1, "an empty board draws its face near the viewer", (p) => (p.count === 0 && p.face !== "released" ? [] : [`posters=${p.count}/${p.face}`]));

  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  const rob = on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  wait(6000);
  on(1, { verb: "stand", entityName: BOARD });
  expect(1, "the robber's poster reaches the robber's board", (p) => (p.count === 1 ? [] : [`posters=${p.count}/${p.face}`]));
  expect(1, "the face freezes after its live window", (p) => (p.face === "frozen" ? [] : [`face=${p.face}`]), 10000);
  shot(1, "wanted_posters_robbery_client1");
  shot(1, "wanted_posters_street_client1", 6, 1.1);

  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) fails.push("client 2 did not join late");
  else {
    arrive(on, 2, wait);
    wait(6000);
    on(2, { verb: "stand", entityName: BOARD });
    expect(2, "the late joiner draws the poster it got on join", (p) => (p.count === 1 && p.face !== "released" ? [] : [`posters=${p.count}/${p.face}`]), 20000);
    wait(4000);
    shot(2, "wanted_posters_late_client2");

    // Out of the draw radius and back: the face is released and rebuilt.
    on(2, { verb: "stand", entityName: FAR });
    wait(3000);
    on(1, { verb: "resetcrime" }, 10000);
    expect(1, "a cleared wanted level takes the poster down", (p) => (p.count === 0 ? [] : [`posters=${p.count}/${p.face}`]));
    expect(2, "the far client hears the change too", (p) => (p.count === 0 ? [] : [`posters=${p.count}/${p.face}`]));
    on(2, { verb: "stand", entityName: BOARD });
    expect(2, "back in range the face is rebuilt with the current list", (p) => (p.count === 0 && p.face !== "released" ? [] : [`posters=${p.count}/${p.face}`]));
  }

  on(1, { verb: "resetcrime" }, 10000);
  const log1 = clientLog(dir, 1);
  const log2 = clientLog(dir, 2);
  if (!/WantedPosters\] client: 1 poster\(s\) from rpc/.test(log1)) fails.push("client 1 never logged the poster arriving by broadcast");
  if (!/WantedPosters\] client: 1 poster\(s\) from jip/.test(log2)) fails.push("client 2 never logged the poster arriving on join (jip)");
  if (!/WantedPosters\] client: face released at <235\.4/.test(log2)) fails.push("client 2 never released the Roadmap face out of range");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "wanted posters proof: FAIL" : "wanted posters proof: PASS (broadcast, late join, distance gate, poster leaves)");
  return fails.length ? 1 : 0;
}

export default {
  name: "wanted-posters-proof",
  usage: "wanted-posters-proof   (up --clients 1; client 2 joins late)",
  run: () => runWantedPostersProof(),
};
