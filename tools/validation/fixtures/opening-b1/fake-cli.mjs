// Stand-in for tools/cli.mjs in tools/validation/opening-b1-selftest.mjs: it answers the host commands that are
// not on main yet (`host config apply`, `host ready`) so the beat script's PASS and FAIL paths run without the game.
// The case is picked by env: FAKE_HELP (a help text file), FAKE_CONFIG ok|nopass|fail, FAKE_READY go|nogo,
// FAKE_PRUNE tagged|clean. It writes only under LF_HOST_DIR.
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const args = process.argv.slice(2);
const host = process.env.LF_HOST_DIR;
const sub = args.slice(0, 3).join(" ");

if (!host) {
  console.log("fake-cli: LF_HOST_DIR is not set");
  process.exit(3);
}

if (args[0] === "host" && args[1] === "help") {
  process.stdout.write(readFileSync(process.env.FAKE_HELP, "utf8"));
  process.exit(0);
}

if (sub === "host config apply") {
  const mode = process.env.FAKE_CONFIG || "ok";
  if (mode === "fail") {
    console.log("host config apply: secrets.json is missing");
    process.exit(1);
  }
  const profile = join(host, "profile");
  mkdirSync(profile, { recursive: true });
  writeFileSync(join(profile, "config.json"), JSON.stringify({ dayCycle: { dayLengthMinutes: 120 } }));
  const password = mode === "nopass" ? "" : "fixture-join-password";
  writeFileSync(join(profile, "server.json"), JSON.stringify({ game: { name: "Life Framework | Everon 1989 (Alpha)", password } }));
  console.log("host config apply: wrote config.json and server.json");
  process.exit(0);
}

if (args[0] === "host" && args[1] === "ready") {
  if (process.env.FAKE_READY === "nogo") {
    console.log("host ready: no-go (disk: 3 GB free, limit 5 GB)");
    process.exit(1);
  }
  console.log("host ready: go");
  process.exit(0);
}

if (sub === "host prune --dry-run") {
  if (process.env.FAKE_PRUNE === "tagged") console.log("host prune: would delete 2026-11-01_10-00-00-preflight");
  console.log("host prune: would delete 0, kept 20 plain (tagged backups are never deleted)");
  process.exit(0);
}

console.log(`fake-cli: no answer for ${args.join(" ")}`);
process.exit(2);
