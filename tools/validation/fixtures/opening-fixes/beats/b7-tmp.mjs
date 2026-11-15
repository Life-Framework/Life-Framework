// opening-run: host=tmp
// Fixture beat for opening-fixes-selftest: a beat that only runs on a folder under <repo>/tmp/ (as B7 and B9 do).
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { isAbsolute, join, relative, resolve } from "node:path";

const i = process.argv.indexOf("--host");
const host = i === -1 ? "" : resolve(process.argv[i + 1]);
const rel = host ? relative(join(process.cwd(), "tmp"), host) : "";
const underTmp = !!rel && !rel.startsWith("..") && !isAbsolute(rel);
const who = host ? join(host, "who") : "";
const shared = !!host && existsSync(who) && readFileSync(who, "utf8") !== "B7";
let why = "";
if (!host) why = "no --host given";
else if (!underTmp) why = `${host} is outside tmp/ (${join(process.cwd(), "tmp")})`;
else if (shared) why = `${host} is a folder another beat used`;
if (!why) writeFileSync(who, "B7");
console.log(`BEAT B7 placement ${why ? "FAIL" : "PASS"} ${why || host}`);
