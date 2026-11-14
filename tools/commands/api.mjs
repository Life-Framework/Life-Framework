// tools\cli api: engine API answers from the installed game, stamped with the build.
import { buildStamp, dumpDate, findMcpDir, findClass, grepScripts, openVfs, staleNote } from "../lib/install-api.mjs";

export const help = "api class <Name> | api grep <regex> [--path <prefix>] [--limit N] | api stamp   the real declaration from the installed game's paks, stamped with the build (api_search is the fallback)";

export async function main(args) {
  const sub = args[0];
  const stamp = buildStamp();
  const note = staleNote(stamp, dumpDate(findMcpDir() || ""));
  if (sub === "stamp") {
    console.log(`${stamp.text}\n${note}`);
    return 0;
  }
  if (sub !== "class" && sub !== "grep") {
    console.error("usage: tools\\cli api class <Name> | grep <regex> [--path prefix] [--limit N] | stamp");
    return 2;
  }
  const arg = args[1];
  if (!arg) { console.error("missing argument"); return 2; }
  let vfs;
  try { vfs = await openVfs(); } catch (e) { console.error(`api: ${e.message}`); return 1; }
  console.log(`# source: installed game paks, ${stamp.text}\n# ${note}`);
  if (sub === "class") {
    const hits = findClass(vfs, arg);
    if (!hits.length) { console.log(`not found in installed scripts: ${arg} (try the base class, or api_search as a fallback)`); return 1; }
    for (const h of hits) console.log(`\n## ${h.path}:${h.line}\n${h.text.split("\n").slice(0, 80).join("\n")}`);
    return 0;
  }
  const opt = (k) => { const i = args.indexOf(k); return i > 0 ? args[i + 1] : undefined; };
  for (const h of grepScripts(vfs, arg, { prefix: opt("--path") || "", limit: Number(opt("--limit")) || 40 })) console.log(`${h.path}:${h.line}: ${h.text}`);
  return 0;
}
