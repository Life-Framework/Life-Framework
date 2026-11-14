// red-proof: Observed 2026-11-11 with `tools\cli test --no-build --only LF_Test_ItemVolume`: removing the SCR_GadgetComponent from FineTicket.et failed `[LFTEST] FAIL inventory/paper-items`, `- a bare fixture character has room for the ticket`. Restored; green. #708 extension: Reverted `SizeSetupStrategy Volume` from the pager, seed packet, keycard, lockpick, evidence bag and marker, vehicle and house keys, ID card: failed `the keycard volume is its ItemVolume (expected 40, got 440)`, `the seed packet volume ... (expected 60, got 1073)`, and no room for the pager and seed packet until they got an SCR_GadgetComponent too. Restored; green.

// tier: WORLD
//! inventory/paper-items: the paper items (receipt, fine ticket, hospital bill) and every other small
//! mod item (evidence, fence tools, pager, keys, ID card, seed packet, ...) read their ItemVolume and
//! fit a bare fixture character on a second spawn, whichever test spawned them first in the run.
//! Without an SCR_GadgetComponent and `SizeSetupStrategy Volume` the engine let only the first
//! instance spawned in a process be stored (later ones read a mesh-derived volume of 1073 and the
//! jacket refused them), so a test passed or failed on which earlier test had spawned the item
//! (#699, #708). Hand-carry items (logs, planks, crate, parcel) are never stored and are not here.
class LF_Test_ItemVolume : LF_Test
{
	protected static const ResourceName RECEIPT = "{3E8A5C1D7B2F4096}Prefabs/Items/Roleplay/LF_Receipt.et";
	protected static const ResourceName TICKET = "{3D6B0E8A41C7F259}Prefabs/Items/Roleplay/FineTicket.et";
	protected static const ResourceName BILL = "{75055A5BFE4372D5}Prefabs/Items/Roleplay/HospitalBill.et";
	protected static const ResourceName SLIP = "{4C1A9E7B52D83F60}Prefabs/Items/Roleplay/SentenceSlip.et";
	protected static const ResourceName EVIDENCE_BAG = "{A2E64B90F1C8D357}Prefabs/Investigation/LF_EvidenceBag.et";
	protected static const ResourceName DRILL = "{4FF102B4CE9B0ED3}Prefabs/Items/Fence/LF_Drill.et";
	protected static const ResourceName KEYCARD = "{F341AA386FA695E4}Prefabs/Items/Fence/LF_Keycard.et";
	protected static const ResourceName LOCKPICK = "{6B1D3F5A92C47E08}Prefabs/Items/Fence/LF_Lockpick.et";
	protected static const ResourceName SCANNER = "{D763DE7A23C5D1FB}Prefabs/Items/Fence/LF_Scanner.et";
	protected static const ResourceName PAGER = "{747E7E1669676AA1}Prefabs/Items/Roleplay/LF_Pager.et";
	protected static const ResourceName SEEDS = "{F77BE55D8D3AEBA3}Prefabs/Items/Seeds/CabbageSeeds.et";
	protected static const ResourceName VEHICLE_KEY = "{9D94B834AF514B4A}Prefabs/Items/Roleplay/VehicleKey.et";
	protected static const ResourceName HOUSE_KEY = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";
	protected static const ResourceName ID_CARD = "{75459D9B0B635800}Prefabs/Items/Roleplay/IDCard.et";
	protected static const ResourceName WATER_BOTTLE = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";
	protected static const ResourceName CEMENT = "{D58674633332DC86}Prefabs/Items/Resources/CementBag.et";
	protected static const ResourceName GRAVEL = "{344090890C63F4CA}Prefabs/Items/Resources/GravelBag.et";

	override string GetName()
	{
		return "inventory/paper-items";
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn and delete one instance, so the checked one is a later spawn (the engine's first
	//! instance per process was always fine).
	protected static void WarmUp(ResourceName prefab, vector pos)
	{
		IEntity item = LF_Utils.SpawnEntityPrefab(prefab, pos);
		if (item)
			SCR_EntityHelper.DeleteEntityAndChildren(item);
	}

	//------------------------------------------------------------------------------------------------
	//! volume < 0 skips the volume check (the prefab's volume comes from the base game); fits false
	//! skips the room check (a bag of cement does not belong in a jacket).
	protected static void CheckItem(LF_TestContext ctx, ResourceName prefab, float volume, bool fits, string label)
	{
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		WarmUp(prefab, body.GetOrigin());
		IEntity item = LF_Utils.SpawnEntityPrefab(prefab, body.GetOrigin());
		if (!item)
		{
			ctx.Fail("the " + label + " prefab does not spawn");
			LF_TestCharacterFixture.Delete(body);
			return;
		}

		InventoryItemComponent inventoryItem = LF_Component<InventoryItemComponent>.Find(item);
		ctx.NotNull(inventoryItem, "the " + label + " has an inventory item component");
		if (inventoryItem && volume >= 0)
			ctx.Equal(volume, inventoryItem.GetTotalVolume(), "the " + label + " volume is its ItemVolume");

		if (fits)
		{
			InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(body);
			ctx.True(manager && manager.CanInsertItem(item), "a bare fixture character has room for the " + label);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(item);
		LF_TestCharacterFixture.Delete(body);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Each in turn: a prefab spawned later in the run is the one that used to be refused.
		CheckItem(ctx, RECEIPT, 60, true, "receipt");
		CheckItem(ctx, TICKET, 113, true, "ticket");
		CheckItem(ctx, BILL, 113, true, "hospital bill");
		CheckItem(ctx, SLIP, 113, true, "sentence slip");
		CheckItem(ctx, TICKET, 113, true, "ticket on a third spawn");

		CheckItem(ctx, EVIDENCE_BAG, 20, true, "evidence bag");
		// The casing and the marker are scene evidence, not items, since #148: only the bag action takes
		// them (investigation/scene-pieces-world asserts they have no inventory item component).
		CheckItem(ctx, DRILL, 4000, false, "drill");
		CheckItem(ctx, KEYCARD, 40, true, "keycard");
		CheckItem(ctx, LOCKPICK, 40, true, "lockpick");
		CheckItem(ctx, SCANNER, 120, true, "scanner");
		CheckItem(ctx, PAGER, 40, true, "pager");
		CheckItem(ctx, SEEDS, 60, true, "seed packet");
		CheckItem(ctx, VEHICLE_KEY, 113, true, "vehicle key");
		CheckItem(ctx, HOUSE_KEY, 113, true, "house key");
		CheckItem(ctx, ID_CARD, 113, true, "ID card");
		CheckItem(ctx, WATER_BOTTLE, -1, true, "water bottle");
		CheckItem(ctx, CEMENT, 3300, false, "cement bag");
		CheckItem(ctx, GRAVEL, 3000, false, "gravel bag");
	}
}
