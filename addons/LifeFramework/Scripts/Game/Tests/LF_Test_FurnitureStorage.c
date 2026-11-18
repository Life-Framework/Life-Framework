// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_FurnitureStorage` (batched run, RED PROOF OK): RED-OK (RedProof() skips the placement record, so the storage key is never attached and "is recorded as a permanent placement" fails for every furniture). Green otherwise.
//! housing/furniture-storage: the seven storage furniture items (wardrobe, dresser, cupboard, chest, crate,
//! fridge, kitchen unit) place a prop with a real inventory. Each placed prefab carries the capacity component
//! with its item-size limit, holds ten apples, registers under `furniture:<placement id>` once recorded, is
//! offered "Open" to the placer only, and a prop with contents is not picked up ("Empty it first").
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FurnitureStorage : LF_Test
{
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const string OWNER = "furniture-storage-owner";
	protected static const vector SPOT = "100 0 66"; // inside the test annex (x/z 18..114), never on the town

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "housing/furniture-storage";
	}

	//------------------------------------------------------------------------------------------------
	protected static void Add(notnull array<ref LF_FurnitureStorageRow> rows, ResourceName placed, float x, float y, float z)
	{
		LF_FurnitureStorageRow row = new LF_FurnitureStorageRow();
		row.m_Placed = placed;
		row.m_vSize = Vector(x, y, z);
		rows.Insert(row);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_FurnitureStorageRow> rows = {};
		Add(rows, "{7416F34B901F7B83}Prefabs/Placeables/Furniture/Furniture_Wardrobe_Placed.et", 70, 140, 70);
		Add(rows, "{1736B87AAEB64AB7}Prefabs/Placeables/Furniture/Furniture_Dresser_Placed.et", 70, 40, 70);
		Add(rows, "{957A81F8C290847D}Prefabs/Placeables/Furniture/Furniture_Cupboard_Placed.et", 70, 100, 70);
		Add(rows, "{BAFB1BD4358EDEFD}Prefabs/Placeables/Furniture/Furniture_Chest_Placed.et", 60, 30, 40);
		Add(rows, "{9A2E2D12C3323874}Prefabs/Placeables/Furniture/Furniture_Crate_Placed.et", 60, 60, 60);
		Add(rows, "{04DEB6AE18777340}Prefabs/Placeables/Furniture/Furniture_Fridge_Placed.et", 60, 60, 60);
		Add(rows, "{8842171499D661BC}Prefabs/Placeables/Furniture/Furniture_KitchenUnit_Placed.et", 70, 70, 70);
		ctx.Equal(7, rows.Count(), "seven storage furniture items are listed");

		int n;
		foreach (LF_FurnitureStorageRow row : rows)
		{
			CheckOne(ctx, row, SPOT + Vector(n * 3, 0, 0));
			n++;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckOne(LF_TestContext ctx, LF_FurnitureStorageRow row, vector at)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity placed = LF_Utils.SpawnEntityPrefab(row.m_Placed, at);
		ctx.True(placed != null, "the placed prefab spawns: " + row.m_Placed);
		if (!placed)
			return;

		string name = LF_Utils.GetPrefabName(placed);
		BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(placed.FindComponent(BaseInventoryStorageComponent));
		ctx.True(storage != null, name + " has an inventory storage");
		LF_PersistentStorageComponent persistent = LF_PlacedStorage.ComponentOf(placed);
		ctx.True(persistent != null, name + " carries LF_PersistentStorageComponent");
		if (storage)
		{
			vector size = storage.GetMaxDimensionCapacity();
			ctx.True(size[0] == row.m_vSize[0] && size[1] == row.m_vSize[1] && size[2] == row.m_vSize[2], string.Format("%1 takes items up to %2 cm (got %3)", name, row.m_vSize, size));
			ctx.Equal(10, LF_InventoryUtils.AddAmount(placed, APPLE, 10), name + " holds ten apples");
		}

		// Recording the placement gives the storage its stable key (red proof: skip the recording).
		string id;
		if (!RedProof())
			id = LF_PlacementSave.Record(OWNER, "Furniture_Test", placed);

		ctx.False(id.IsEmpty(), name + " is recorded as a permanent placement");
		if (!id.IsEmpty())
			ctx.True(LF_PersistentStorageComponent.Resolve(LF_PlacedStorage.KeyFor(id)) == placed, name + " is registered under furniture:<placement id>");

		// "Open" is the placer's alone.
		IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
		IEntity other = LF_TestCharacterFixture.Spawn(ctx);
		if (owner && other)
		{
			LF_Houses.BindCharacter(owner, OWNER);
			LF_Houses.BindCharacter(other, "furniture-storage-other");
			LF_FurnitureStorageOpenAction open = FindOpen(placed);
			ctx.True(open != null, name + " carries an Open action");
			if (open)
			{
				ctx.True(open.CanBeShownScript(owner), name + ": Open is shown to the placer");
				ctx.False(open.CanBeShownScript(other), name + ": Open is not shown to a bystander");
			}

			// Something in it: the placer may not pick it up (and is told); nothing is lost.
			if (!id.IsEmpty())
			{
				owner.SetOrigin(placed.GetOrigin() + "1.5 0 0");
				ctx.False(LF_PlacementPickup.PickUp(owner, placed), name + " holding items is not picked up");
				ctx.True(LF_PlacementPickup.LastRefusal().Contains("empty it first"), "the refusal says to empty it first: " + LF_PlacementPickup.LastRefusal());
				ctx.True(LF_PlacementSave.LiveEntity(id) != null, name + " still stands");
			}
		}

		LF_TestCharacterFixture.Delete(owner);
		LF_TestCharacterFixture.Delete(other);
		if (!id.IsEmpty())
		{
			LF_PlacedStorage.Release(id);
			LF_PlacementSave.Forget(id);
		}

		LF_TestCharacterFixture.Delete(placed);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_FurnitureStorageOpenAction FindOpen(IEntity entity)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			LF_FurnitureStorageOpenAction open = LF_FurnitureStorageOpenAction.Cast(action);
			if (open)
				return open;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
class LF_FurnitureStorageRow
{
	ResourceName m_Placed;
	vector m_vSize;
}
