// red-proof: observed 2026-11-19 with `toolscli test --no-build --red LF_Test_BinEmptiesOnRestart`: `RED-OK bins/empties-on-restart` (RedProof() skips the world-reset step, so "the bin is empty after the world reset (expected 0, got 2)" fails). Green on the real step.
//! bins/empties-on-restart (#3323): a tagged bin opens for anyone, an item put in it by one character is
//! there for a second, an item taken out first survives, the world-reset step deletes what is left through
//! the bin's storage manager, and no save record names the bin (it has no persistent storage). A restart
//! brings a bin back empty for the same reason: nothing ever stored it.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BinEmptiesOnRestart : LF_Test
{
	protected static const ResourceName BIN = "{C53D93D86C6CD2EE}Prefabs/Props/Bins/LF_StreetBin.et";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName PLUM = "{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et";
	protected static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	protected static const vector SPOT = "100 0 80"; // inside the test annex (x/z 18..114), never on the town

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "bins/empties-on-restart";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		vector at = SPOT;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity binEntity = LF_Utils.SpawnEntityPrefab(BIN, at);
		ctx.NotNull(binEntity, "the street bin spawns");
		if (!binEntity)
			return;

		LF_BinComponent bin = LF_BinComponent.Cast(binEntity.FindComponent(LF_BinComponent));
		BaseInventoryStorageComponent binStorage = BaseInventoryStorageComponent.Cast(binEntity.FindComponent(BaseInventoryStorageComponent));
		ctx.NotNull(bin, "the street bin carries LF_BinComponent");
		ctx.NotNull(binStorage, "the street bin has an inventory storage");
		ctx.True(LF_BinComponent.Count() >= 1, "the bin registered itself");
		ctx.True(LF_Component<LF_PersistentStorageComponent>.Find(binEntity) == null, "no save record names the bin: it has no persistent storage");

		IEntity putter = LF_TestCharacterFixture.Spawn(ctx);
		IEntity taker = LF_TestCharacterFixture.Spawn(ctx);
		if (bin && binStorage && putter && taker)
			CheckBin(ctx, binEntity, bin, binStorage, putter, taker);

		LF_TestCharacterFixture.Delete(putter);
		LF_TestCharacterFixture.Delete(taker);
		LF_TestCharacterFixture.Delete(binEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBin(LF_TestContext ctx, IEntity binEntity, LF_BinComponent bin, BaseInventoryStorageComponent binStorage, IEntity putter, IEntity taker)
	{
		// Anyone may open it.
		SCR_OpenStorageAction open = FindOpen(binEntity);
		ctx.NotNull(open, "the bin carries the vanilla open-storage action");
		if (open)
			ctx.True(open.CanBeShownScript(taker), "Open is shown to any character");

		// One character puts three different things in (one of each, so none stacks into another).
		SCR_InventoryStorageManagerComponent putterManager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(putter);
		SCR_InventoryStorageManagerComponent takerManager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(taker);
		ctx.NotNull(putterManager, "the first character has an inventory manager");
		ctx.NotNull(takerManager, "the second character has an inventory manager");
		if (!putterManager || !takerManager)
			return;

		array<ResourceName> things = {APPLE, PLUM, NUGGET};
		int put;
		foreach (ResourceName thing : things)
		{
			ctx.Equal(1, LF_InventoryUtils.AddAmount(putter, thing, 1), "the first character holds " + thing);
			array<IEntity> held = LF_InventoryUtils.FindItemsByPrefab(putterManager, thing);
			if (held.Count() > 0 && putterManager.TryMoveItemToStorage(held[0], binStorage))
				put++;
		}

		ctx.Equal(3, put, "three things go into the bin");
		ctx.Equal(3, bin.GetItems().Count(), "the bin holds the three things");

		// The second character sees them and takes one out.
		array<IEntity> seen = bin.GetItems();
		ctx.Equal(3, seen.Count(), "a second character sees what the first put in");
		ResourceName taken;
		if (seen.Count() > 0)
		{
			taken = LF_Utils.GetPrefabName(seen[0]);
			BaseInventoryStorageComponent target = takerManager.FindStorageForItem(seen[0]);
			ctx.NotNull(target, "the second character has room for it");
			if (target)
				ctx.True(takerManager.TryMoveItemToStorage(seen[0], target), "the second character takes one thing out of the bin");
		}

		ctx.Equal(2, bin.GetItems().Count(), "two things are left in the bin");
		ctx.True(LF_TestCharacterFixture.HasItem(taker, taken), "the thing taken out is with the second character");

		// The world reset empties what is left, and only through the storage manager.
		ctx.True(HasBinsStep(), "the world reset has a bins step");
		int removed;
		LF_WorldReset_Bins step = new LF_WorldReset_Bins();
		if (!RedProof())
			removed = step.Run();

		ctx.True(removed >= 2, string.Format("the world reset removed the two things left (removed %1)", removed));
		ctx.Equal(0, bin.GetItems().Count(), "the bin is empty after the world reset");
		ctx.True(LF_TestCharacterFixture.HasItem(taker, taken), "the thing taken out first survives the reset");
		ctx.True(GetGame().GetWorld().FindEntityByID(binEntity.GetID()) != null, "the bin itself still stands");
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasBinsStep()
	{
		foreach (LF_WorldResetStep step : LF_WorldReset.Steps())
		{
			if (step.GetName() == "bins")
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_OpenStorageAction FindOpen(IEntity entity)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			SCR_OpenStorageAction open = SCR_OpenStorageAction.Cast(action);
			if (open)
				return open;
		}

		return null;
	}
}
