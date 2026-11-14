// Import this first in an entry script (tools/cli.mjs, agent-box/harness.mjs,
// agent-box/session.mjs). It runs the quiet wrapper before the rest of the script loads;
// see quiet-run.mjs. Evaluated once, for whichever entry script started the process.
import { gate } from "./quiet-run.mjs";

await gate();
