// red-proof: observed 2026-11-24 `tools/cli.mjs test --no-build --red LF_Test_ToolItemWear,LF_Test_ToolBench,LF_Test_GatherWear,LF_Test_ToolConditionPersist` (with the pack leg test): RED-OK tool/item-wear, "the used pickaxe lost four uses (expected 96, got 100)", "the pickaxe in the pack stays worn". Before #4821 the tool had no item state at all (one store per player and kind). Green with `--only`.
//! tool/item-wear (#4821): condition lives on the tool item. Two pickaxes keep separate conditions, a
//! use with no tool named wears the one in the hands (not the one in the pack), the bench repairs the
//! held tool only, and a tool keeps its condition when it is put away, dropped, and carried through a
//! snapshot onto a fresh body (the restart and reconnect path).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ToolItemWear : LF_Test
{
	static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	static const ResourceName SCRAP_STAND_IN = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const string CHARACTER_ID = "tool-item-wear-character";

	override string GetName()
	{
		return "tool/item-wear";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		IEntity first = LF_Utils.SpawnEntityPrefab(PICKAXE, character.GetOrigin() + "3 0.5 0");
		IEntity second = LF_Utils.SpawnEntityPrefab(PICKAXE, character.GetOrigin() + "3 0.5 1");
		LF_ToolConditionComponent a = LF_ToolConditionComponent.Of(first);
		LF_ToolConditionComponent b = LF_ToolConditionComponent.Of(second);
		ctx.True(a && b, "both pickaxes carry a tool condition");
		if (!a || !b)
		{
			Cleanup(character, first, second);
			return;
		}

		float rate = LF_ToolCondition.WearPerUse(a.GetTier());
		ctx.EqualFloat(LF_ToolCondition.FULL, a.GetCondition(), 0.001, "a pickaxe as spawned is at full condition");

		// Two pickaxes keep separate conditions: wearing the first leaves the second as sold.
		for (int i = 0; i < 4; i++)
		{
			if (!RedProof())
				LF_ToolWear.OnUse(character, LF_ToolBench.KIND_PICKAXE, first);
		}
		ctx.EqualFloat(100.0 - 4 * rate, a.GetCondition(), 0.001, "the used pickaxe lost four uses");
		ctx.EqualFloat(LF_ToolCondition.FULL, b.GetCondition(), 0.001, "the other pickaxe is untouched");

		// The held one: a use with no tool named wears what is in the hands, not the one in the pack.
		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(character);
		ctx.True(manager && manager.TryInsertItem(first), "the worn pickaxe goes into the character's gear");
		LF_ToolConditionComponent.SetHeldForTests(character, second);
		ctx.True(LF_ToolConditionComponent.Held(character, LF_ToolBench.KIND_PICKAXE) == b, "the second pickaxe is the held one");
		ctx.True(LF_ToolConditionComponent.Held(character, LF_ToolBench.KIND_AXE) == null, "a held pickaxe is no axe");
		LF_ToolWear.OnUse(character, LF_ToolBench.KIND_PICKAXE);
		ctx.EqualFloat(100.0 - rate, b.GetCondition(), 0.001, "the held pickaxe took the swing");
		ctx.EqualFloat(100.0 - 4 * rate, a.GetCondition(), 0.001, "the carried pickaxe did not");

		// Repair restores the held item only.
		LF_MoneyUtils.AddCash(character, 50);
		LF_InventoryUtils.AddAmount(character, SCRAP_STAND_IN, 1);
		LF_EToolBenchResult result = LF_ToolBench.Repair(character, LF_ToolConditionComponent.Held(character, LF_ToolBench.KIND_PICKAXE), SCRAP_STAND_IN, LF_ToolBench.FEE);
		ctx.Equal(LF_EToolBenchResult.DONE, result, "the bench repairs the held pickaxe");
		ctx.EqualFloat(LF_ToolCondition.FULL, b.GetCondition(), 0.001, "the held pickaxe is back to full");
		ctx.EqualFloat(100.0 - 4 * rate, a.GetCondition(), 0.001, "the pickaxe in the pack stays worn");
		LF_ToolConditionComponent.SetHeldForTests(null, null);
		ctx.True(LF_ToolConditionComponent.Held(character, LF_ToolBench.KIND_PICKAXE) == null, "with empty hands nothing is held");
		ctx.True(LF_ToolConditionComponent.InUse(character, LF_ToolBench.KIND_PICKAXE) == a, "a gather from the pack uses the carried pickaxe");

		// Put away, then dropped: the same item, the same condition.
		ctx.EqualFloat(100.0 - 4 * rate, a.GetCondition(), 0.001, "the carried pickaxe keeps its condition");
		ctx.True(LF_StorageSnapshot.CaptureCarried(CHARACTER_ID, character), "the character is captured with the worn pickaxe");
		ctx.True(LF_InventoryUtils.DropItem(character, first), "the worn pickaxe is dropped");
		ctx.EqualFloat(100.0 - 4 * rate, a.GetCondition(), 0.001, "the dropped pickaxe keeps its condition");

		// The snapshot carries it onto a fresh body (what a restart or a reconnect does).
		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Get(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(record, "the carried snapshot is stored");
		IEntity restored = LF_TestCharacterFixture.Spawn(ctx);
		if (record && restored)
		{
			LF_StorageSnapshot.Apply(restored, record);
			LF_ToolConditionComponent back = LF_ToolConditionComponent.InUse(restored, LF_ToolBench.KIND_PICKAXE);
			ctx.NotNull(back, "the pickaxe is back on the fresh body");
			if (back)
				ctx.EqualFloat(100.0 - 4 * rate, back.GetCondition(), 0.001, "the restored pickaxe keeps its condition");
		}

		LF_StorageSnapshot.Clear(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		LF_TestCharacterFixture.Delete(restored);
		Cleanup(character, first, second);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Cleanup(IEntity character, IEntity first, IEntity second)
	{
		LF_ToolConditionComponent.SetHeldForTests(null, null);
		LF_TestCharacterFixture.Delete(first);
		LF_TestCharacterFixture.Delete(second);
		LF_TestCharacterFixture.Delete(character);
	}
}
