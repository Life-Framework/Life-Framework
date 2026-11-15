// Stand-in server that writes a console.log with the ready line under <profile>/logs (argv[2]),
// as the engine does, then stays up (host-start-nolog-selftest kills it).
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
const dir = join(process.argv[2], "logs", "logs_2099-01-01_00-00-00");
mkdirSync(dir, { recursive: true });
writeFileSync(join(dir, "console.log"), "00:00:01.000 ENGINE : Entered online game state.\n");
setTimeout(() => process.exit(0), 60000);
