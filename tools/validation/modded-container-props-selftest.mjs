// Selftest for validate-modded-container-props: the missing-attribute logic on fixture text.
// red-proof: 2026-11-14, with the `[BaseContainerProps()]` line removed from the OK fixture this failed
// "copy with the attribute is clean".
import assert from "node:assert/strict";
import { missingContainerAttrs } from "./validate-modded-container-props.mjs";

const base = `[BaseContainerProps()]\nclass SCR_X : Y\n{\n}\n`;
const baseCustom = `[BaseContainerProps(), BaseContainerCustomTitleField("m_a")]\nclass SCR_Z : Y\n{\n}\n`;
assert.deepEqual(missingContainerAttrs(base, `modded class SCR_X\n{\n}\n`, "SCR_X"), ["BaseContainerProps()"], "copy without it is flagged");
assert.deepEqual(missingContainerAttrs(base, `[BaseContainerProps()]\nmodded class SCR_X\n{\n}\n`, "SCR_X"), [], "copy with the attribute is clean");
assert.deepEqual(missingContainerAttrs(baseCustom, `[BaseContainerProps()]\nmodded class SCR_Z\n{\n}\n`, "SCR_Z"), ['BaseContainerCustomTitleField("m_a")'], "custom title missing");
assert.deepEqual(missingContainerAttrs(`class SCR_Q\n{\n}\n`, `modded class SCR_Q\n{\n}\n`, "SCR_Q"), [], "base without attributes needs none");
console.log("modded-container-props-selftest: OK");
