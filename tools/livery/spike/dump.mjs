// tools/livery/spike/dump.mjs - print vanilla .emat/.et/.conf files from the game paks (livery spike #4226).
// Usage: node tools/livery/spike/dump.mjs <path> [<path>...]
import { gameVfs } from "../../lib/game-prefab.mjs";

const vfs = await gameVfs();
for (const p of process.argv.slice(2)) {
  console.log(`=== ${p}`);
  if (!vfs.exists(p)) { console.log("(missing)"); continue; }
  const b = vfs.readFile(p);
  console.log(b.toString("utf8"));
}
