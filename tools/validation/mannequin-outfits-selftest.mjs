// Selftest for validate-mannequin-outfits.mjs (revert audit 2026-11-27, #5096): a mannequin that wears a piece its
// outlet does not sell must fail, whatever way the piece went missing, and the real tree must pass.
// inputs: tools/validation/validate-mannequin-outfits.mjs, tools/validation/validate-item-catalogue.mjs, addons/**/*.conf, addons/**/LF_Mannequin*.et
// red-proof: observed 2026-11-27 on the real tree with the raincoat mannequin set back to `jacket_raincoat_green` (the
// offer #5096 dropped; the state the main guard reverted): validate-mannequin-outfits.mjs printed "FAIL ...
// LF_Mannequin_Raincoat.et wears jacket_raincoat_green, which clothing_rack does not sell" and exited 1; restored, it passes.
import { checkMannequins, mergeCatalogue, parseMannequin, run } from "./validate-mannequin-outfits.mjs";

const fails = [];
const expect = (cond, msg) => { if (!cond) fails.push(msg); };

const item = (id, price, removed) => `  LF_CatalogueItem "{AAAAAAAAAAAAAAA1}" {
   m_sId "${id}"
${price === null ? "" : `   m_iPrice ${price}\n`}${removed === undefined ? "" : `   m_bRemoved ${removed}\n`}  }`;
const offer = (outlet, id, extra = "") => `  LF_OutletOffer "{AAAAAAAAAAAAAAA2}" {
   m_sOutlet "${outlet}"
   m_sItem "${id}"
${extra}  }`;
const conf = (items, offers) => ({ file: "addons/LF_Everon1989/Configs/Catalogue/x.conf", text: `LF_ItemCatalogueConfig {\n m_aItems {\n${items.join("\n")}\n }\n m_aOffers {\n${offers.join("\n")}\n }\n}\n` });
const core = (items, offers) => ({ ...conf(items, offers), file: "addons/LifeFramework/Configs/Catalogue/LF_ItemCatalogue.conf" });
const prefab = (outlet, ids) => ({ file: "addons/LF_Everon1989/Prefabs/Props/Clothing/LF_Mannequin_Test.et", text: `GenericEntity : "{451F65968BA97575}Prefabs/Props/Commercial/LF_MannequinDisplay.et" {
 components {
  LF_ShopComponent "{B9707321705D85F1}" {
   m_sOutletId "${outlet}"
  }
  LF_MannequinOutfitComponent "{DF845760343DB74B}" {
   m_aItemIds {
${ids.map((i) => `    "${i}"`).join("\n")}
   }
  }
 }
}
` });

const base = [core([item("coat", 60), item("hat", 20), item("boots", 40)], [offer("rack", "coat"), offer("rack", "hat"), offer("rack", "boots")])];
const man = prefab("rack", ["coat", "hat", "boots"]);
const out = (files, p = man) => checkMannequins(files, [p]).join("\n");

expect(parseMannequin("GenericEntity {}") === null, "a prefab with no outfit component is not a mannequin");
expect(JSON.stringify(parseMannequin(man.text)) === JSON.stringify({ outlet: "rack", ids: ["coat", "hat", "boots"] }), "the outlet and the item ids are read");
expect(out(base) === "", `a clean outfit passes (got: ${out(base)})`);

// The #5096 shape: the pack drops the offer (a removed row) while the prefab still wears the piece.
const dropped = [...base, conf([], [offer("rack", "coat", "   m_bRemoved 1\n")])];
expect(/coat, which rack does not sell/.test(out(dropped)), "a dropped offer fails");
// The same piece wanted under another outlet's name.
expect(/hat, which shop does not sell/.test(out(base, prefab("shop", ["coat", "hat", "boots"]))), "an offer under another outlet fails");
// A piece that is not a catalogue item at all, or is removed, or has no price.
expect(/ghost, which is not a live priced/.test(out(base, prefab("rack", ["coat", "hat", "ghost"]))), "an unknown id fails");
expect(/hat, which is not a live priced/.test(out([...base, conf([item("hat", null, 1)], [])])), "a removed item fails");
expect(/hat, which is not a live priced/.test(out([...base, conf([item("hat", 0)], [])])), "a price of 0 fails");
// A licence row is not a piece.
expect(/coat, which rack does not sell/.test(out([core([item("coat", 60), item("hat", 20), item("boots", 40)], [offer("rack", "coat", '   m_sGrantsLicence "FIREARM_PERMIT"\n'), offer("rack", "hat"), offer("rack", "boots")])])), "a licence row fails");
// Two pieces make an outfit; one does not.
expect(/needs two/.test(out(base, prefab("rack", ["coat"]))), "a one-piece mannequin fails");
// A later layer that writes only a price keeps the offer; a later layer that restores a removed row passes.
expect(out([...base, conf([item("coat", 75)], [])]) === "", "a price patch keeps the offer");
expect(out([...dropped, conf([], [offer("rack", "coat", "   m_bRemoved 0\n")])]) === "", "a later layer can restore a removed offer");
expect(mergeCatalogue(base).items.get("coat").price === 60, "the merge keeps the core price");
expect(out(base, { file: "addons/LF_Everon1989/Prefabs/Props/Clothing/LF_Mannequin_NoShop.et", text: man.text.replace(/\s*m_sOutletId "rack"/, "") }) !== "", "an outfit with no outlet fails");

// The real tree passes.
const real = run();
expect(real.prefabs >= 1, "the real tree has mannequin prefabs to check");
expect(real.fails.length === 0, `the real tree passes (got: ${real.fails.join("; ")})`);

for (const f of fails) console.log(`FAIL  ${f}`);
if (fails.length) process.exit(1);
console.log("mannequin-outfits-selftest: ok");
