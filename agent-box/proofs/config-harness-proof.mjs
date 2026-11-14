import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, sessionDir, sessionPaths } from "../session.mjs";
import { loadFixtures, runHarness, setClientDriver, STAGES } from "../../tools/config/harness.mjs";

// #300: the config harness's client stage. For every fixture in tools/test/config-fixtures that has a
// `client` block in its sidecar, the fixture's text becomes the server's owner config.json, the server
// streams it (`configreload`), and client 1 reads the value a player sees: a price on the shop's list
// (a config price, a renamed currency term) or whether an action is offered (a switched-off system).
// Run on `session up --clients 1`. Every fixture is applied to the same running server (#299 boots a
// server per fixture); the shipped config comes back at the end.
//   config-harness-proof [--only <fixture name part>]
function ownerFile() {
  return join(sessionPaths(sessionDir()).server, "profile", "LifeFramework", "config.json");
}

async function runConfigHarnessProof(args) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^[0-9]+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

  const driver = {
    apply(fx) {
      mkdirSync(join(ownerFile(), ".."), { recursive: true });
      writeFileSync(ownerFile(), fx.text);
      const r = on(1, { verb: "configreload" }, 10000);
      if (r.status !== "ok") console.log(`WARN  configreload for ${fx.name}: ${r.status} ${r.detail}`);
      wait(3500);
    },
    read(fx, read) {
      if (read.kind === "shopPrice") {
        on(1, { verb: "stand", entityName: read.shop });
        wait(2000);
        on(1, { verb: "use", className: "LF_ShopAction", entityName: read.shop }, 10000);
        wait(1500);
        on(1, { verb: "type", widget: "Search", text: read.item });
        wait(800);
        const list = on(1, { verb: "ui", mode: "text" }, 10000).detail;
        on(1, { verb: "click", widget: "CloseButton" });
        wait(800);
        return list;
      }
      if (read.kind === "actionShown") {
        on(1, { verb: "stand", entityName: read.entity, side: "s" });
        wait(2500);
        // `look` lists the offered actions within 4 m as Class or Class@EntityName.
        return on(1, { verb: "look" }, 10000).detail.split(",").some((a) => a.split("@")[0] === read.class);
      }
      return "";
    },
  };

  const only = args.includes("--only") ? args[args.indexOf("--only") + 1] : "";
  const fixtures = loadFixtures().filter((f) => f.expect?.client?.length && f.name.includes(only));
  if (!fixtures.length) { console.log("config harness proof: no fixture has client reads"); return 1; }

  arrive(on, 1, wait);
  wait(8000);
  setClientDriver(driver);
  const bad = runHarness(fixtures, [STAGES.find((s) => s.name === "client")]);
  setClientDriver(null);

  rmSync(ownerFile(), { force: true });
  on(1, { verb: "configreload" }, 10000);
  wait(2000);

  for (const b of bad) for (const f of b.fails) console.log(`FAIL  ${b.fixture} [${b.stage}]  ${f}`);
  for (const f of fixtures) if (!bad.some((b) => b.fixture === f.name)) console.log(`PASS  ${f.name}  (${f.expect.client.length} client read(s))`);
  console.log(bad.length ? "config harness proof: FAIL" : `config harness proof: PASS (${fixtures.length} fixture(s): each owner value was visible on the client)`);
  return bad.length ? 1 : 0;
}

export default {
  name: "config-harness-proof",
  usage: "config-harness-proof [--only <name>]   (up --clients 1)",
  run: (ctx) => runConfigHarnessProof(ctx.args ?? []),
};
