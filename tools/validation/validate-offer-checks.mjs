// Offer checks run on the player's machine. CanBeShownScript and
// inputs: addons/**
// CanBePerformedScript are evaluated on the client to build the interact list,
// and LF_PlayerAccountManager lives on the server only: on a dedicated client
// every account lookup returns null, so an offer gated on it is never shown.
// That hid arrest and escort from on-duty police (playtest QA, 2026-11-08).
// Offer checks read LF_AccountReplication.GetState(entity) instead; the server
// re-checks the real account in PerformAction.
//
// Fails a CanBeShownScript or CanBePerformedScript body that calls
// LF_PoliceUtils.GetAccountForEntity or LF_PlayerAccountManager.GetInstance.
//
// red-proof: on main 2026-11-08 this printed 5 FAIL lines (LF_JailArrestAction,
// LF_JailEscortAction, LF_JailEscortArrestAction, LF_JailPutInVehicleAction,
// LF_JailTakeOutOfVehicleAction) and exited 1; after the fix it prints OK.
import { readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const scripts = join(root, "addons", "LifeFramework", "Scripts", "Game");
const FORBIDDEN = /\bLF_PoliceUtils\s*\.\s*GetAccountForEntity\s*\(|\bLF_PlayerAccountManager\s*\.\s*GetInstance\s*\(/;

function walk(dir, out = []) {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (name.endsWith(".c") && !name.startsWith("LF_Test")) out.push(p);
  }
  return out;
}

// The body of each `override bool CanBe(Shown|Performed)Script(...)`, comments stripped.
export function offerBodies(src) {
  const code = src.replace(/\r\n/g, "\n").replace(/\/\/[^\n]*/g, (m) => " ".repeat(m.length));
  const out = [];
  const head = /bool\s+(CanBeShownScript|CanBePerformedScript)\s*\([^)]*\)\s*\{/g;
  let m;
  while ((m = head.exec(code))) {
    let depth = 1;
    let i = head.lastIndex;
    while (i < code.length && depth > 0) {
      if (code[i] === "{") depth++;
      else if (code[i] === "}") depth--;
      i++;
    }
    out.push({ name: m[1], line: code.slice(0, m.index).split("\n").length, body: code.slice(head.lastIndex, i) });
  }
  return out;
}

export function check(files) {
  const fails = [];
  for (const f of files) {
    for (const b of offerBodies(readFileSync(f, "utf8"))) {
      if (FORBIDDEN.test(b.body)) fails.push(`${relative(root, f).split("\\").join("/")}:${b.line} ${b.name} reads the server-only account; use LF_AccountReplication.GetState`);
    }
  }
  return fails;
}

function selftest() {
  const bad = "class A\n{\n\toverride bool CanBeShownScript(IEntity user)\n\t{\n\t\treturn LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(user));\n\t}\n}\n";
  const good = "class A\n{\n\toverride bool CanBeShownScript(IEntity user)\n\t{\n\t\t// not LF_PoliceUtils.GetAccountForEntity(user)\n\t\treturn LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));\n\t}\n\tvoid PerformAction(IEntity o, IEntity u)\n\t{\n\t\tLF_PoliceUtils.GetAccountForEntity(u);\n\t}\n}\n";
  const hit = (s) => offerBodies(s).some((b) => FORBIDDEN.test(b.body));
  if (!hit(bad)) return "selftest: the bad fixture passed";
  if (hit(good)) return "selftest: the good fixture failed";
  return null;
}

const self = selftest();
if (self) {
  console.log(`FAIL ${self}`);
  process.exit(1);
}
const fails = check(walk(scripts));
for (const f of fails) console.log(`FAIL ${f}`);
console.log(fails.length ? `validate-offer-checks: ${fails.length} failure(s)` : "validate-offer-checks: OK");
process.exit(fails.length ? 1 : 0);
