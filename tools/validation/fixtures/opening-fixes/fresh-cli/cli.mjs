// Stand-in for tools/cli.mjs for the B8 cases in opening-fixes-selftest.
//   default          a host that never reaches its ready line (a first boot with no save)
//   FAKE_MODE=uid    the first boot makes a save point; `host password rotate` is refused because the roster has no uid yet
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
const args = process.argv.slice(2);
const host = process.env.LF_HOST_DIR || "";
if (args[0] === "host" && args[1] === "start") {
  if (process.env.FAKE_MODE === "uid") {
    const sp = join(host, "profile", "profile", ".save", "u", "game", "M", "playthrough000", "savepoint000");
    mkdirSync(sp, { recursive: true });
    writeFileSync(join(sp, "meta-info.json"), "{}");
    console.log("host: up (pid 1)");
    process.exit(0);
  }
  console.log("host start: timed out after 2s waiting for 'Entered online game state.'");
  process.exit(1);
}
if (args[0] === "host" && args[1] === "stop") {
  console.log("host: not running");
  process.exit(0);
}
if (args[0] === "host" && args[1] === "password" && process.env.FAKE_MODE === "uid") {
  console.log("[LFDebug:HostPassword] refused: config apply refused: line 10 (Jasper): the administrator has no uid yet, so game.admins cannot be written");
  process.exit(1);
}
console.log(`fresh-cli: unexpected call ${args.join(" ")}`);
process.exit(3);
