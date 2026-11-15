import { arrive, clientBox, sessionDir } from "../session.mjs";

// Pictures of the pack supermarket's stocked shelves (#1143), the picture steps of supermarket-proof alone:
// the hall, the fruit and dry-goods gondolas, the cooler and an aisle. For before and after pairs in a PR
// that changes a shelf layout. On a running `up --profile full --mission playtest` session.
const B = [76, 0.1, 96.45];
const at = (x, y, z) => [B[0] + x, B[1] + y, B[2] + z].map((v) => +v.toFixed(3));

async function runStoreShots(args) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const prefix = args.includes("--name") ? args[args.indexOf("--name") + 1] : "store";
  const on = (spec, timeout = 15000) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const stand = (name, target, offset) => on({ verb: "capture", mode: "world", name: `${prefix}_${name}`, x: target[0], y: target[1], z: target[2], ox: offset[0], oy: offset[1], oz: offset[2] }, 30000);
  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(4000);
  // The first frames after a teleport draw the building before it streams in: one throwaway shot.
  stand("warmup", at(0, 3.1, -7), [-8, -1.5, -9]);
  wait(6000);
  // ...and one inside, so the interior has streamed in (a first shot of the hall shows no ceiling and plain walls).
  stand("warmup_hall", at(-1.5, 1.0, 2.5), [1.5, -0.9, -9]);
  wait(8000);
  stand("hall", at(-1.5, 1.0, 2.5), [1.5, -0.9, -9]);
  stand("fruit", at(-5.2, 0.9, -1.5), [1.8, -0.8, -2.2]);
  stand("dry_goods", at(0.8, 0.9, -1.5), [1.6, -0.8, -2.2]);
  stand("cooler", at(-8.0, 0.75, -3.6), [1.6, -0.65, 0.6]);
  stand("aisle", at(-3.0, 1.0, -1.5), [0, -0.8, -7]);
  return 0;
}

export default {
  name: "store-shots",
  usage: "store-shots [--name <shot prefix>]   (up --profile full --mission playtest)",
  run: (ctx) => runStoreShots(ctx?.args || []),
};
