import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// Shared plumbing of the island flow proofs (wholesale-fallback-proof, island-farm-proof, island-fishing-proof,
// island-impound-proof): one client on `up --mission island --profile full`, a command helper that prints each
// reply, the carried cash and counts, every server console log of the session, and a PASS/FAIL step table.
// No proof verb lives here (a leading underscore keeps the loader off it).

export function toastOf(detail) {
  return /(?:^|\s)toast=(\S*)/.exec(String(detail ?? ""))?.[1] ?? "none";
}

// Every server console log of the session, oldest first (a `session restart` starts a new run folder).
export function allServerLogs(dir = sessionDir()) {
  const logs = join(dir, "server", "logs");
  if (!existsSync(logs)) return "";
  const runs = readdirSync(logs).filter((d) => existsSync(join(logs, d, "console.log")));
  runs.sort((a, b) => statSync(join(logs, a, "console.log")).mtimeMs - statSync(join(logs, b, "console.log")).mtimeMs);
  return runs.map((d) => readFileSync(join(logs, d, "console.log"), "utf8")).join("\n");
}

// The ripe-or-not test of a farm plot's prompt, the stage a plot log line names, and the cash a sale moved are all
// read from text the game prints; these helpers keep that text handling in one place and under selftest.
export function plotStage(log, plotKey) {
  const all = [...String(log).matchAll(new RegExp(`plot ${plotKey} stage -> (\\d+)`, "g"))];
  return all.length ? Number(all[all.length - 1][1]) : -1;
}

export function selftestKit() {
  const f = [];
  if (toastOf("me=CIVILIAN/0/0 toast=They_have_enough dispatches=0") !== "They_have_enough") f.push("toast is read");
  if (toastOf("no toast here") !== "none") f.push("a missing toast reads none");
  if (plotStage("plot sp_farm_plot_1 stage -> 0\nplot sp_farm_plot_1 stage -> 2\nplot sp_farm_plot_2 stage -> 1", "sp_farm_plot_1") !== 2) f.push("the last stage line of the plot");
  if (plotStage("", "sp_farm_plot_1") !== -1) f.push("no stage line reads -1");
  return f;
}

export async function openKit(label, { arriveFirst = true } = {}) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout ?? 10000);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail ?? "").slice(0, 300)}`);
    return r;
  };
  const results = [];
  const step = (name, ok, detail) => {
    results.push({ name, ok });
    console.log(`${ok ? "PASS" : "FAIL"}  ${name}${detail ? `: ${detail}` : ""}`);
    return ok;
  };
  const state = () => {
    let r = on({ verb: "state" });
    for (let i = 0; i < 6 && r.status !== "ok"; i++) {
      wait(3000);
      r = on({ verb: "state" });
    }
    return parseState(r.detail);
  };
  const cash = () => Number(state().cash || 0);
  const carried = (item) => {
    const r = on({ verb: "carries", words: [item] });
    const m = /count=(\d+)/.exec(String(r.detail));
    return r.status === "ok" && m ? Number(m[1]) : 0;
  };
  const toast = () => toastOf(on({ verb: "seen", words: ["4"] }).detail);
  const shot = (name) => on({ verb: "capture", mode: "ui", name }, 30000);
  const serverLog = () => allServerLogs(dir);
  if (arriveFirst) {
    arrive((n, spec, t) => on(spec, t), 1, wait);
    wait(1500);
    if (/WelcomeGuideDismissButton/.test(String(on({ verb: "ui" }).detail))) on({ verb: "click", widget: "WelcomeGuideDismissButton" });
    on({ verb: "guide", words: ["close"] });
    wait(500);
  }
  const finish = () => {
    const failed = results.filter((r) => !r.ok);
    console.log(failed.length ? `${label}: FAIL (${failed.length} of ${results.length}: ${failed.map((r) => r.name).join("; ")})` : `${label}: PASS (${results.length} of ${results.length})`);
    return failed.length ? 1 : 0;
  };
  return { h, dir, wait, on, step, state, cash, carried, toast, shot, serverLog, finish, results };
}
