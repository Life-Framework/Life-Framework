import { arrive, clientBox, sessionDir } from "../session.mjs";

// "Find a place" on the open map (#5127 card, playtest round 2): types each word into the search blank through the
// harness and reads the rows the list shows, so a place players are sent to that the search misses fails the run.
//
//   map-search-proof [--words hostel,clerk,market]
//
//   node agent-box/session.mjs up --mission island --profile lowspec --wait
//   node agent-box/session.mjs map-search-proof
//   node agent-box/session.mjs down
// Prints one line per word (`search hostel: 1 -> Hostel, Saint-Philippe`); exit 1 when a word finds nothing.
const OPEN_TRIES = 10;
const DEFAULT_WORDS = "hostel,clerk,market";

function opt(args, name) {
  const i = args.indexOf(`--${name}`);
  return i < 0 ? undefined : args[i + 1];
}

function parseDetail(detail) {
  const out = {};
  for (const m of String(detail).matchAll(/(\w+)=([^\s]*)/g)) out[m[1]] = m[2];
  return out;
}

async function run(args) {
  const words = (opt(args, "words") || DEFAULT_WORDS).split(",").map((w) => w.trim()).filter(Boolean);
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (spec, timeout = 15000) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fail = (why) => {
    console.log(`SUMMARY map-search-proof fail ${why}`);
    return 1;
  };

  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(6000);

  let opened = on({ verb: "map", words: ["open"] });
  for (let i = 1; i < OPEN_TRIES && opened.status !== "ok" && /not_carried|no_character/.test(opened.detail); i++) {
    wait(2000);
    opened = on({ verb: "map", words: ["open"] });
  }
  if (opened.status !== "ok") return fail(`map open: ${opened.detail}`);

  let status = {};
  for (let i = 0; i < 60; i++) {
    wait(1000);
    status = parseDetail(on({ verb: "map", words: ["status"] }).detail);
    if (status.open === "1" && Number(status.view_m) > 0) break;
  }
  if (status.open !== "1") return fail("the map did not open after map open");

  const known = parseDetail(on({ verb: "maplegend", words: ["status"] }).detail);
  console.log(`known places on this map: ${known.known}`);

  const missed = [];
  for (const word of words) {
    const r = on({ verb: "maplegend", words: ["search", word] });
    if (r.status !== "ok") return fail(`maplegend search ${word}: ${r.detail}`);
    const count = Number(parseDetail(r.detail).results);
    const names = String(r.detail).replace(/^.*names=/, "");
    console.log(`search ${word}: ${count} -> ${names}`);
    if (!(count > 0)) missed.push(word);
  }

  on({ verb: "maplegend", words: ["close"] });
  on({ verb: "map", words: ["close"] });
  if (missed.length) return fail(`no result for: ${missed.join(", ")} (known=${known.known})`);
  console.log(`SUMMARY map-search-proof ok ${words.length} word(s) found: ${words.join(", ")}`);
  return 0;
}

export default {
  name: "map-search-proof",
  usage: "map-search-proof [--words hostel,clerk,market]   (up --mission island --profile lowspec; reads the Find a place results)",
  run: (ctx) => run(ctx?.args || []),
};
