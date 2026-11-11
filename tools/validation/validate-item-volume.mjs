// An item prefab with an InventoryItemComponent needs `SizeSetupStrategy Volume` in its
// ItemPhysAttributes (own or inherited from an addon prefab). Without it the engine derives the
// volume from the mesh for every instance after the first spawned in a process (a 40 ItemVolume
// keycard reads 440, a seed packet 1073) and the inventory refuses it; which test or player saw the
// bug depended on spawn order (#699, #708). An item that also carries a pick-up/read actions manager
// needs an SCR_GadgetComponent as well (copy the hand pose from SentenceSlip.et), unless the test
// `inventory/paper-items` proves the prefab fits without one and it is allow-listed below.
//
// Fails an addon item prefab with an InventoryItemComponent and no volume strategy, or with an
// ActionsManagerComponent and no gadget component, unless it is allow-listed. An allow-list entry
// needs a reason and a real prefab.
//
// red-proof: with the #708 prefab fixes reverted this printed FAIL lines for LF_Pager.et,
// CabbageSeeds.et, VehicleKey.et and others, and exited 1; with them applied it prints OK.
import { readdirSync, readFileSync, statSync, existsSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const prefabs = join(root, "addons", "LifeFramework", "Prefabs");

const HAND_CARRY = "hand-carry prop, never stored in an inventory";
const BASE_VOLUME = "volume comes from the base game prefab it inherits";
const NO_GADGET_OK = "fits a second spawn without one (inventory/paper-items)";

// Path (relative to Prefabs/) -> reason.
const ALLOW_VOLUME = {
  "Items/Drinks/WaterBottle.et": BASE_VOLUME,
  "Items/Drinks/AppleJuice.et": BASE_VOLUME,
  "Items/Drinks/Drink_Base.et": "base prefab, children set their own",
  "Items/Food/Cabbage.et": BASE_VOLUME,
  "Items/Food/Food_Base.et": "base prefab, children set their own",
  "Items/Core/HandCarryItem_Base.et": "base prefab for hand-carry props",
  "Items/Core/InteractableItem_Base.et": "base prefab, children set their own",
  "Items/Core/RoleplayItem_Base.et": "base prefab, children set their own",
  "Items/Currencies/MoneyStack.et": "cash is a ledger entry, the stack never enters a bag",
  "Items/Roleplay/LF_MoneyBag.et": HAND_CARRY,
  "Items/Roleplay/LF_Parcel.et": HAND_CARRY,
  "Items/Contraband/LF_ContrabandCrate.et": HAND_CARRY,
  "Resources/Mining/CopperBar.et": BASE_VOLUME,
  "Resources/Mining/GoldBar.et": BASE_VOLUME,
  "Resources/Mining/IronBar.et": BASE_VOLUME,
  "Resources/WoodCutting/WoodKindling.et": BASE_VOLUME,
  "Resources/WoodCutting/WoodLog_01_Small.et": HAND_CARRY,
  "Resources/WoodCutting/WoodPlank.et": HAND_CARRY,
};
const ALLOW_GADGET = {
  "Investigation/LF_EvidenceBag.et": NO_GADGET_OK,
  "Investigation/LF_EvidenceCasing.et": NO_GADGET_OK,
  "Investigation/LF_EvidenceMarker.et": NO_GADGET_OK,
  "Items/Fence/LF_Drill.et": NO_GADGET_OK,
  "Items/Fence/LF_Keycard.et": NO_GADGET_OK,
  "Items/Fence/LF_Lockpick.et": NO_GADGET_OK,
  "Items/Fence/LF_Scanner.et": NO_GADGET_OK,
  "Items/Drinks/WaterBottle.et": NO_GADGET_OK,
  "Items/Resources/CementBag.et": "bulk bag, covered by inventory/paper-items",
  "Items/Resources/GravelBag.et": "bulk bag, covered by inventory/paper-items",
};

function walk(dir, out = []) {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (name.endsWith(".et")) out.push(p);
  }
  return out;
}

const rel = (f) => relative(prefabs, f).split("\\").join("/");

// What one prefab text declares itself, and which prefab it inherits.
export function facts(src) {
  const text = src.replace(/\r\n/g, "\n");
  const parent = /^\w+\s*:\s*"\{([0-9A-Fa-f]{16})\}/.exec(text);
  return {
    parent: parent ? parent[1].toUpperCase() : null,
    item: /^\s*InventoryItemComponent\b/m.test(text),
    actions: /^\s*ActionsManagerComponent\b/m.test(text),
    gadget: /^\s*SCR_GadgetComponent\b/m.test(text),
    volume: /^\s*SizeSetupStrategy\s+Volume\b/m.test(text),
  };
}

// Flags for one prefab given its own facts and those of its addon ancestors.
export function flags(self, ancestors) {
  const chain = [self, ...ancestors];
  const out = [];
  if (self.item && !chain.some((c) => c.volume)) out.push("volume");
  if (self.item && chain.some((c) => c.actions) && !chain.some((c) => c.gadget)) out.push("gadget");
  return out;
}

function check() {
  const files = walk(prefabs);
  const byGuid = new Map();
  for (const f of files) {
    const meta = f + ".meta";
    if (!existsSync(meta)) continue;
    const m = /\{([0-9A-Fa-f]{16})\}/.exec(readFileSync(meta, "utf8"));
    if (m) byGuid.set(m[1].toUpperCase(), f);
  }
  const cache = new Map();
  const factsOf = (f) => {
    if (!cache.has(f)) cache.set(f, facts(readFileSync(f, "utf8")));
    return cache.get(f);
  };
  const fails = [];
  for (const f of files) {
    const self = factsOf(f);
    if (!self.item) continue;
    const ancestors = [];
    let next = self.parent;
    for (let i = 0; next && i < 12; i++) {
      const pf = byGuid.get(next);
      if (!pf) break; // a base game prefab: not readable here
      const pfacts = factsOf(pf);
      ancestors.push(pfacts);
      next = pfacts.parent;
    }
    const p = rel(f);
    for (const flag of flags(self, ancestors)) {
      if (flag === "volume" && ALLOW_VOLUME[p]) continue;
      if (flag === "gadget" && ALLOW_GADGET[p]) continue;
      if (flag === "volume") fails.push(`${p} has an InventoryItemComponent but no \`SizeSetupStrategy Volume\`: later spawns read a mesh-derived volume and the inventory refuses them (#708)`);
      else fails.push(`${p} has an actions manager and no SCR_GadgetComponent: only the first spawn can enter an inventory (#708); add one with the hand pose from SentenceSlip.et, or allow-list it with a reason`);
    }
  }
  for (const [k, v] of [...Object.entries(ALLOW_VOLUME), ...Object.entries(ALLOW_GADGET)]) {
    if (!v) fails.push(`allow-list entry ${k} has no reason`);
    if (!existsSync(join(prefabs, k))) fails.push(`allow-list entry ${k} names no prefab`);
  }
  return fails;
}

function selftest() {
  const base = 'GenericEntity : "{8B993DEBBF41116C}x.et" {\n components {\n  InventoryItemComponent "{1}" {\n  }\n';
  const item = (extra) => facts(base + extra + " }\n}\n");
  const none = flags(item(""), []);
  const act = flags(item('  ActionsManagerComponent "{2}" {\n  }\n'), []);
  const ok = flags(item('  SCR_GadgetComponent "{3}" {\n  }\n  ActionsManagerComponent "{2}" {\n  }\n  SizeSetupStrategy Volume\n'), []);
  const inherited = flags(item(""), [facts("GenericEntity {\n SizeSetupStrategy Volume\n}\n")]);
  if (none.join() !== "volume") return "selftest: a bare item should flag volume only";
  if (act.join() !== "volume,gadget") return "selftest: an item with actions should flag volume and gadget";
  if (ok.length) return "selftest: a complete item flagged";
  if (inherited.length) return "selftest: an inherited volume strategy flagged";
  return null;
}

const self = selftest();
if (self) {
  console.log(`FAIL ${self}`);
  process.exit(1);
}
const fails = check();
for (const f of fails) console.log(`FAIL ${f}`);
console.log(fails.length ? `validate-item-volume: ${fails.length} failure(s)` : "validate-item-volume: OK");
process.exit(fails.length ? 1 : 0);
