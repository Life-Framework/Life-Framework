// tools/validation/validate-hud-root-slots.mjs
//
// The HUD root layout the player controller loads (EL_HUDManagerRoot.layout) replaces the vanilla
// HUDManager_Root.layout. Vanilla HUD pieces attach to named slots in it: the vehicle's dashboard
// indicators to Slot_Vehicle, a weapon's ammo to Slot_WeaponInfo. A slot the layout lacks makes the piece
// draw in the middle of the screen (the high-beam icon, #921). Keep the slots this mod relies on.
// red-proof: 2026-11-14, with Slot_Vehicle removed from the layout this printed
// "EL_HUDManagerRoot.layout has no Slot_Vehicle" and FAILED; restored, OK.

import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FILE = "addons/LifeFramework/UI/Layouts/HUD/HUDLayouts/EL_HUDManagerRoot.layout";
const NEEDED = ["Slot_Vehicle", "Slot_WeaponInfo", "Slot_AvailableActions"];

if (!existsSync(join(ROOT, FILE))) {
  console.log(`validate-hud-root-slots: FAILED (${FILE} is missing)`);
  process.exit(1);
}
const text = readFileSync(join(ROOT, FILE), "utf8");
const missing = NEEDED.filter((n) => !text.includes(`Name "${n}"`));
for (const n of missing) console.log(`ERROR EL_HUDManagerRoot.layout has no ${n}: vanilla HUD pieces for it draw mid-screen. Copy it from the vanilla HUDManager_Root.layout.`);
if (missing.length) {
  console.log(`validate-hud-root-slots: FAILED (${missing.length} slot(s) missing)`);
  process.exit(1);
}
console.log(`validate-hud-root-slots: OK (${NEEDED.length} slots present)`);
