// tools\cli server-template check [--engine]   the example server folder (server/example/, #295)
//
// Static: server.json, config.json, the start scripts and the README, no game (also run by `validate`).
// --engine: starts the installed dedicated server with only `-config server/example/server.json` and reads
// its "JSON Schema Validation" verdict from the console log (about 10 s; it stops itself at the mission
// header because the local addons cannot be loaded with -config, see server/example/README.md).
import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, readdirSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { templateProblems } from "../server-template/template.mjs";

export const help = "server-template check [--engine]   the example server folder (server/example): static checks, or let the real server read server.json";

function serverExe() {
  const c = process.env.ENFUSION_SERVER_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Server";
  return /\.exe$/i.test(c) ? c : join(c, "ArmaReforgerServer.exe");
}

export function engineVerdict(log) {
  if (/JSON is Valid/.test(log)) return { ok: true, reason: "the engine says: JSON is Valid" };
  const bad = log.split(/\r?\n/).filter((l) => /BACKEND\s+\(E\)/.test(l)).map((l) => l.replace(/^\S+\s+BACKEND\s+\(E\):\s*/, "")).join(" | ");
  return { ok: false, reason: bad || "no JSON Schema Validation verdict in the log" };
}

export async function main(args, { root }) {
  const [sub] = args;
  if (sub !== "check") {
    console.log("usage: tools\cli server-template check [--engine]");
    return 1;
  }
  const fails = templateProblems(root);
  for (const f of fails) console.log(`FAIL  ${f}`);
  if (fails.length) return 1;
  console.log("server-template: static checks pass (server/example is a ready folder)");
  if (!args.includes("--engine")) return 0;

  const exe = serverExe();
  if (!existsSync(exe)) {
    console.log(`FAIL  dedicated server not found: ${exe}`);
    return 1;
  }
  const profile = mkdtempSync(join(tmpdir(), "lf-template-"));
  spawnSync(exe, ["-config", join(root, "server", "example", "server.json"), "-profile", profile, "-maxFPS", "30", "-disableCrashReporter", "-noBackend"], { cwd: dirname(exe), timeout: 90000, windowsHide: true });
  const logs = existsSync(join(profile, "logs")) ? readdirSync(join(profile, "logs")).sort() : [];
  const file = logs.length ? join(profile, "logs", logs[logs.length - 1], "console.log") : "";
  if (!file || !existsSync(file)) {
    console.log("FAIL  the server wrote no console.log");
    return 1;
  }
  const v = engineVerdict(readFileSync(file, "utf8"));
  console.log(`${v.ok ? "server-template" : "FAIL "} engine: ${v.reason}`);
  return v.ok ? 0 : 1;
}
