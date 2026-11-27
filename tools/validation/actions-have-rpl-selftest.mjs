// tools/validation/actions-have-rpl-selftest.mjs - validate-actions-have-rpl (#5110).
// inputs: tools/validation/validate-actions-have-rpl.mjs, tools/lib/rpl-actions.mjs, tools/validation/actions-have-rpl-selftest.mjs
//
// Runs the validator over a throwaway tree of prefabs:
//   - a rootless prop with a vanilla-class action and no RplComponent fails, naming the file and the action class;
//   - the same prop with an RplComponent passes;
//   - an action class that overrides HasLocalEffectOnlyScript (a local mesh, as the plant stages) passes;
//   - an addon child of the failing prop fails too, and passes once the base gets the RplComponent;
//   - a child of a base-game prefab (GUID not in the addons) is assumed to inherit the base's RplComponent;
//   - a prop with an ActionsManagerComponent and no actions passes.
//
// red-proof: 2026-11-27, with findUnreachableActions returning [] the first case printed
// `FAIL a rootless prop with a vanilla action must fail` and exited 1; restored, all cases pass.
import { spawnSync } from "node:child_process";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { releaseScratch, scratchDir } from "../lib/scratch.mjs";
import { parsePrefab } from "../lib/rpl-actions.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const here = dirname(fileURLToPath(import.meta.url));
const validator = join(here, "validate-actions-have-rpl.mjs");
const dir = scratchDir("lf-actions-rpl-");
const prefabs = join(dir, "addons", "LF_Test", "Prefabs");
const scripts = join(dir, "addons", "LF_Test", "Scripts", "Game");
mkdirSync(prefabs, { recursive: true });
mkdirSync(scripts, { recursive: true });

const meta = (guid, name) => `MetaFileClass {\n Name "{${guid}}Prefabs/${name}"\n}\n`;
const actions = (cls) => ` ActionsManagerComponent "{AAAAAAAAAAAAAA01}" {
   ActionContexts {
    UserActionContext "{AAAAAAAAAAAAAA02}" {
     ContextName "ctx"
    }
   }
   additionalActions {
    ${cls} "{AAAAAAAAAAAAAA03}" {
     ParentContextList {
      "ctx"
     }
    }
   }
  }`;
const rpl = `  RplComponent "{BBBBBBBBBBBBBB01}" {\n   Enabled 1\n  }\n`;
const root = (cls, withRpl, parent) => `${parent ? `GenericEntity : "{${parent}}Prefabs/x.et"` : "GenericEntity"} {
 ID "1111111111111111"
 components {
 ${actions(cls)}
${withRpl ? rpl : ""} }
}
`;
const put = (name, guid, text) => {
  writeFileSync(join(prefabs, name), text);
  writeFileSync(join(prefabs, name + ".meta"), meta(guid, name));
};
const run = () => spawnSync(process.execPath, [validator], { env: { ...process.env, LF_RPLACTIONS_ROOT: dir }, encoding: "utf8" });

try {
  const p = parsePrefab(root("SCR_Foo", true, null));
  check(p.parent === null && p.components.has("RplComponent") && p.components.has("ActionsManagerComponent") && p.actions.join() === "SCR_Foo", "parsePrefab reads the root components and the action class");
  const child = parsePrefab(root("SCR_Foo", false, "CCCCCCCCCCCCCC01"));
  check(child.parent === "CCCCCCCCCCCCCC01", "parsePrefab reads the parent GUID");

  put("Bad.et", "1000000000000001", root("SCR_Foo", false, null));
  let r = run();
  check(r.status === 1 && /FAIL .*Bad\.et.*SCR_Foo/.test(r.stdout), "a rootless prop with a vanilla action must fail, naming file and class");

  put("Bad.et", "1000000000000001", root("SCR_Foo", true, null));
  r = run();
  check(r.status === 0, "the same prop with an RplComponent passes");

  put("Bad.et", "1000000000000001", root("LF_LocalAction", false, null));
  writeFileSync(join(scripts, "LF_LocalAction.c"), "class LF_LocalAction : ScriptedUserAction\n{\n\toverride bool HasLocalEffectOnlyScript()\n\t{\n\t\treturn true;\n\t}\n}\n");
  r = run();
  check(r.status === 0, "an action class that overrides HasLocalEffectOnlyScript passes");

  put("Bad.et", "1000000000000001", root("SCR_Foo", false, null));
  put("Child.et", "1000000000000002", root("SCR_Bar", false, "1000000000000001"));
  r = run();
  check(r.status === 1 && /Child\.et/.test(r.stdout) && /Bad\.et/.test(r.stdout), "an addon child of a failing prop fails too");

  put("Bad.et", "1000000000000001", root("SCR_Foo", true, null));
  r = run();
  check(r.status === 0, "the child passes once the base has the RplComponent");

  put("Vanilla.et", "1000000000000003", root("SCR_Baz", false, "9999999999999999"));
  r = run();
  check(r.status === 0, "a child of a base-game prefab is assumed to inherit its RplComponent");

  put("Empty.et", "1000000000000004", "GenericEntity {\n ID \"2222222222222222\"\n components {\n  ActionsManagerComponent \"{AAAAAAAAAAAAAA09}\" {\n  }\n }\n}\n");
  r = run();
  check(r.status === 0, "an ActionsManagerComponent with no actions passes");
} finally {
  releaseScratch(dir);
}

if (failed) {
  console.log(`actions-have-rpl-selftest: FAIL (${failed})`);
  process.exit(1);
}
console.log("actions-have-rpl-selftest: PASS");
