// red-proof: observed on the unfixed code (plain UIInfo on the key prefabs, UpdateDisplayName calling UIInfo.SetName on the
// shared prefab UIInfo): `- the house key's UIInfo is the inventory kind`, and the prefab name no longer read
// '#EL-ItemName_HouseKey'. Green with LF_KeyItemUIInfo per item (#2470).

// tier: WORLD
//! houses/key-item-name (#2470): every house and car key names its own house or vehicle in the inventory, read
//! from the replicated identifier, and the UIInfo every key of the prefab shares keeps its prefab name.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_KeyItemName : LF_Test
{
	protected static const ResourceName HOUSE_KEY = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";
	protected static const ResourceName VEHICLE_KEY = "{9D94B834AF514B4A}Prefabs/Items/Roleplay/VehicleKey.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "houses/key-item-name";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The name the inventory shows for this item, empty when it has no inventory UIInfo.
	protected static string InventoryName(IEntity item)
	{
		InventoryItemComponent inventoryItem = LF_Component<InventoryItemComponent>.Find(item);
		if (!inventoryItem)
			return "";

		SCR_InventoryUIInfo uiInfo = SCR_InventoryUIInfo.Cast(inventoryItem.GetUIInfo());
		if (!uiInfo)
			return "";

		return uiInfo.GetInventoryItemName(inventoryItem);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The UIInfo name every item of the prefab shares (the prefab data, not a per-item read).
	protected static string SharedName(IEntity item)
	{
		InventoryItemComponent inventoryItem = LF_Component<InventoryItemComponent>.Find(item);
		if (!inventoryItem || !inventoryItem.GetUIInfo())
			return "";

		return inventoryItem.GetUIInfo().GetName();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity keyA = LF_Utils.SpawnEntityPrefab(HOUSE_KEY, "68 0 68");
		IEntity keyB = LF_Utils.SpawnEntityPrefab(HOUSE_KEY, "69 0 68");
		IEntity car = LF_Utils.SpawnEntityPrefab(VEHICLE_KEY, "70 0 68");
		ctx.NotNull(keyA, "the first house key spawns");
		ctx.NotNull(keyB, "the second house key spawns");
		ctx.NotNull(car, "the vehicle key spawns");
		if (!keyA || !keyB || !car)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(keyA);
			SCR_EntityHelper.DeleteEntityAndChildren(keyB);
			SCR_EntityHelper.DeleteEntityAndChildren(car);
			return;
		}

		LF_KeyComponent componentA = LF_Component<LF_KeyComponent>.Find(keyA);
		LF_KeyComponent componentB = LF_Component<LF_KeyComponent>.Find(keyB);
		LF_VehicleKeyComponent carComponent = LF_Component<LF_VehicleKeyComponent>.Find(car);
		ctx.NotNull(componentA, "the house key carries LF_KeyComponent");
		ctx.NotNull(carComponent, "the vehicle key carries LF_VehicleKeyComponent");
		if (!componentA || !componentB || !carComponent)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(keyA);
			SCR_EntityHelper.DeleteEntityAndChildren(keyB);
			SCR_EntityHelper.DeleteEntityAndChildren(car);
			return;
		}

		string sharedBefore = SharedName(keyA);
		string unbound = InventoryName(keyA);
		ctx.True(unbound.Length() > 0, "an unbound house key still has an inventory name (the plain prefab name)");

		componentA.SetHouseIdentifier("key-name-house-a");
		componentB.SetHouseIdentifier("key-name-house-b");
		carComponent.SetVehicleIdentifier("key-name-vehicle-c");

		string nameA = InventoryName(keyA);
		string nameB = InventoryName(keyB);
		string nameCar = InventoryName(car);
		ctx.True(nameA.Contains("key-name-house-a"), "house key A names its own house (" + nameA + ")");
		ctx.True(nameB.Contains("key-name-house-b"), "house key B names its own house (" + nameB + ")");
		ctx.False(nameA.Contains("key-name-house-b"), "house key A does not show house B's name");
		ctx.False(nameB.Contains("key-name-house-a"), "house key B does not show house A's name");
		ctx.False(nameA == nameB, "two keys for two houses show two names");
		ctx.True(nameCar.Contains("key-name-vehicle-c"), "the vehicle key names its own vehicle (" + nameCar + ")");

		ctx.EqualStr(sharedBefore, SharedName(keyA), "the shared house key UIInfo name is unchanged by binding");
		ctx.EqualStr(sharedBefore, SharedName(keyB), "the shared UIInfo reads the same on the other key");

		SCR_EntityHelper.DeleteEntityAndChildren(keyA);
		SCR_EntityHelper.DeleteEntityAndChildren(keyB);
		SCR_EntityHelper.DeleteEntityAndChildren(car);
	}
};
