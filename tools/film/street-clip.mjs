// Records the #393 acceptance clip: scene "street" (a walker uses the store till while a driver drives a car
// down the main street) shot with LF_FilmCamera cuts. Needs a session with the character spawned:
//   node agent-box/session.mjs up --clients 1 --mission playtest --res 1920x1080 --wait
//   node agent-box/harness.mjs --client 1 flow ui-intake
//   node tools/film/street-clip.mjs [--fps 30]
import { join } from "node:path";
import { homedir } from "node:os";
import { basename, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import * as eng from "./engine.mjs";
import * as h from "../../agent-box/harness.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const base = process.env.LF_SESSION_DIR || join(process.env.LOCALAPPDATA || join(homedir(), "AppData", "Local"), "LifeFramework", "session", basename(ROOT));
const box = join(base, "client1", "profile", "agent-box");
const send = (spec, t) => {
  h.setBoxes([box]);
  return h.sendCommand(spec, t);
};
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const i = process.argv.indexOf("--fps");
const fps = i === -1 ? 30 : Number(process.argv[i + 1]);
const name = `street_${Date.now() % 100000000}`;

const start = eng.engineStart(send, name, { fps, seconds: 60 });
if (!start.ok) throw new Error(`film start failed: ${start.error}`);
const t0 = Date.now();
const at = async (sec) => sleep(Math.max(0, t0 + sec * 1000 - Date.now()));
const cmd = (verb, args) => console.log(`${((Date.now() - t0) / 1000).toFixed(1)}s ${verb} ${args} -> ${send({ verb, args }, 8000).detail}`);

cmd("scene", "stop");
cmd("scene", "start street");
await at(2); // let the car and actors stream in before the first shot
cmd("camera", "follow 128 0 250 10 5.5");
await at(8);
cmd("camera", "orbit 180 0 244 8 7");
await at(15.5);
cmd("camera", "follow 225 0 250 10 6");
await at(22);
cmd("scene", "status");
const clip = eng.engineStop(send, (alias) => {
  h.setBoxes([box]);
  return h.resolveCapture(alias);
}, { label: "film-actors-street", fps });
console.log(eng.describe(clip));
