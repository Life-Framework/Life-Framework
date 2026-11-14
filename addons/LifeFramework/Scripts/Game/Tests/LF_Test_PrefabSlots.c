// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_PrefabSlots`: skipping the FindComponentSource check in LF_PrefabSlots.Unusable failed `- a nugget is no card (expected 'has no LF_IdCardComponent', got '')` and `- pack: an ID card without LF_IdCardComponent is rejected; core default (expected '{75459D9B0B635800}Prefabs/Items/Roleplay/IDCard.et', got '{C9D661E5B0714711}Prefabs/Items/Food/Apple.et')`. Restored; green.

//------------------------------------------------------------------------------------------------
//! A stand-in pack source that overlays the prefab slots core file with a test fixture.
class LF_Test_PrefabSlotsPackStub : LF_ConfigPackSource
{
	static const ResourceName OVERLAY = "{787EB61DCF471E20}Configs/Tests/LF_Test_PrefabSlotsOverlay.conf";

	override void GetOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
		if (coreConf == LF_PrefabSlotsConfig.CONFIG_PATH)
			overlays.Insert(OVERLAY);
	}

	override string GetName()
	{
		return "prefab-slots-test-stub";
	}
}

//------------------------------------------------------------------------------------------------
//! Prefab slots (#964) on the real core file. Core alone: every migrated system gets its shipped
//! prefab. With the fixture pack: cash swaps to a prefab that has what cash needs; the ID card
//! slot pointed at an apple (no LF_IdCardComponent) and the money bag at a missing file fall back
//! to the core default; a pack removing a slot keeps the core default; an unknown slot is empty.
// tier: WORLD
class LF_Test_PrefabSlots : LF_Test
{
	static const ResourceName CASH = "{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et";
	static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	static const ResourceName ID_CARD = "{75459D9B0B635800}Prefabs/Items/Roleplay/IDCard.et";
	static const ResourceName BAG = "{65DE48DCC1D4DD01}Prefabs/Items/Roleplay/LF_MoneyBag.et";
	static const ResourceName MARKER = "{4C81F2A6D9E37B05}Prefabs/Investigation/LF_EvidenceMarker.et";
	static const ResourceName WORN = "{7BD876D8BA3FF91E}Prefabs/Items/Roleplay/Handcuffs_Worn.et";

	override string GetName()
	{
		return "prefabs/slots";
	}

	override void Run(LF_TestContext ctx)
	{
		// Core only: an empty pack source.
		LF_ConfigService.Reset();
		LF_ConfigService.SetPackSource(new LF_ConfigPackSource());
		LF_PrefabSlots.ResetStatics();
		ctx.EqualStr(CASH, LF_MoneyUtils.CashPrefab(), "core: cash is the MoneyStack");
		ctx.EqualStr(BAG, LF_Bank.BagPrefab(), "core: the money bag");
		ctx.EqualStr(ID_CARD, LF_IdCard.Prefab(), "core: the ID card");
		ctx.EqualStr(MARKER, LF_Evidence.MarkerPrefab(), "core: the evidence marker");
		ctx.EqualStr(WORN, LF_Restraint.WornPrefab(), "core: the worn handcuffs");
		ctx.True(LF_PrefabSlots.Get("no_such_slot").IsEmpty(), "an unknown slot is empty, so that one feature is off");

		// The requirement check itself.
		array<string> needsCard = {"LF_IdCardComponent"};
		ctx.EqualStr("", LF_PrefabSlots.Unusable(ID_CARD, needsCard), "the card prefab has a card component");
		ctx.EqualStr("has no LF_IdCardComponent", LF_PrefabSlots.Unusable(NUGGET, needsCard), "a nugget is no card");
		array<string> bogus = {"LF_NoSuchComponentClass"};
		ctx.EqualStr("requires unknown class LF_NoSuchComponentClass", LF_PrefabSlots.Unusable(CASH, bogus), "an unknown required class is named");

		// The fixture pack over core.
		LF_ConfigService.SetPackSource(new LF_Test_PrefabSlotsPackStub());
		LF_PrefabSlots.ResetStatics();
		ctx.EqualStr(NUGGET, LF_MoneyUtils.CashPrefab(), "pack: cash swaps to a prefab with an inventory item and a quantity");
		ctx.EqualStr(ID_CARD, LF_IdCard.Prefab(), "pack: an ID card without LF_IdCardComponent is rejected; core default");
		ctx.EqualStr(BAG, LF_Bank.BagPrefab(), "pack: a money bag that does not load is rejected; core default");
		ctx.EqualStr(MARKER, LF_Evidence.MarkerPrefab(), "pack: a removed slot keeps the core default");
		ctx.EqualStr(WORN, LF_Restraint.WornPrefab(), "pack: a slot the pack leaves alone stays core");

		LF_ConfigService.Reset();
		LF_PrefabSlots.ResetStatics();
	}
}
