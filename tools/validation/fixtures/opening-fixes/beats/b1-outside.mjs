// Fixture beat for opening-fixes-selftest: the default placement (a folder outside the repo, as `host config apply` needs).
// FAILs when its --host is missing, is not LF_HOST_DIR, is inside the runner's root (cwd), or is a folder another beat used.
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { isAbsolute, join, relative, resolve } from "node:path";

const i = process.argv.indexOf("--host");
const host = i === -1 ? "" : resolve(process.argv[i + 1]);
const rel = host ? relative(process.cwd(), host) : "";
const inside = !!rel && !rel.startsWith("..") && !isAbsolute(rel);
const who = host ? join(host, "who") : "";
const shared = !!host && existsSync(who) && readFileSync(who, "utf8") !== "B1";
let why = "";
if (!host) why = "no --host given";
else if (resolve(process.env.LF_HOST_DIR || "") !== host) why = `LF_HOST_DIR ${process.env.LF_HOST_DIR} is not --host`;
else if (inside) why = `${host} is inside the repo (${process.cwd()})`;
else if (shared) why = `${host} is a folder another beat used`;
if (!why) writeFileSync(who, "B1");
console.log(`BEAT B1 placement ${why ? "FAIL" : "PASS"} ${why || host}`);
