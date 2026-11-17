// Red-proof for validate-keybinding-menu: a menu missing a bound action fails, the real files pass (#2510).
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { menuProblems } from "./validate-keybinding-menu.mjs";

const input = `Action LF_A {\n}\nAction LF_B {\n}\nAction LF_Keypad1 {\n}\n`;
const row = (a) => `SCR_KeyBindingEntry "{0}" {\n actionName "${a}"\n}\n`;
const fail = (msg) => { console.log(`FAIL keybinding-menu-selftest: ${msg}`); process.exit(1); };
const internal = { LF_Keypad1: "test" };

if (menuProblems(input, row("LF_A") + row("LF_B"), internal).length) fail("complete menu should pass");
const missing = menuProblems(input, row("LF_A"), internal);
if (missing.length !== 1 || !missing[0].includes("LF_B")) fail(`menu missing LF_B should give one problem naming it, got ${JSON.stringify(missing)}`);
if (!menuProblems(input, row("LF_A") + row("LF_B"), { ...internal, LF_Gone: "stale" }).some((p) => p.includes("LF_Gone"))) fail("stale allowlist entry should fail");
if (!menuProblems(input, row("LF_A") + row("LF_B") + row("LF_Keypad1"), internal).some((p) => p.includes("LF_Keypad1"))) fail("allowlisted action with a row should fail");

const dir = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "addons", "LifeFramework", "Configs", "System");
const real = menuProblems(readFileSync(join(dir, "chimeraInputCommon.conf"), "utf8"), readFileSync(join(dir, "keyBindingMenu.conf"), "utf8"));
if (real.length) fail(`real files:\n${real.join("\n")}`);
console.log("keybinding-menu-selftest: OK");
