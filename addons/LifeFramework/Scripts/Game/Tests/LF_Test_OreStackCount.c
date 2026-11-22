// red-proof: 2026-11-22 `node tools/cli.mjs test --no-build --red LF_Test_OreStackCount,LF_Test_TillSellKey,LF_Test_ProcessRunClock`: RED PROOF OK (all 3 fail with the hook on). RedProof() picks one nugget, so the count checks read 1. Before the fix the harness counted entities (1) and pickup reported not_inserted alive=0 for every merged nugget (session repro, DebugWorld).
//! mining/ore-stack-count (first-2h review B9): iron nuggets are stackable (LF_QuantityComponent from
//! Item_Base), so every pickup after the first merges into one stack and the picked entity is deleted.
//! Three picked nuggets are one entity holding 3, the harness count reads 3, and the mining run's ore
//! step ("carry 3 iron ore", LF_ActivityLiveFacts.Carried) sees all 3.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_OreStackCount : LF_Test
{
	protected const ResourceName IRON_NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";

	override string GetName()
	{
		return "mining/ore-stack-count";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity miner = LF_TestCharacterFixture.Spawn(ctx);
		if (!miner)
			return;

		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(miner);
		if (!inventory)
		{
			ctx.Fail("the miner has no inventory manager");
			LF_TestCharacterFixture.Delete(miner);
			return;
		}

		// Three pickups, one at a time, the way the F key inserts a nugget lying by the rock.
		int picks = 3;
		if (RedProof())
			picks = 1;

		for (int i = 0; i < picks; i++)
		{
			IEntity nugget = LF_Utils.SpawnEntityPrefab(IRON_NUGGET, miner.GetOrigin() + "0 0.3 0.6");
			if (!nugget)
			{
				ctx.Fail("a nugget did not spawn: " + IRON_NUGGET);
				break;
			}

			ctx.True(inventory.TryInsertItem(nugget, EStoragePurpose.PURPOSE_DEPOSIT), "pickup " + (i + 1).ToString() + " goes into the inventory");
		}

		array<IEntity> stacks = LF_InventoryUtils.FindItemsByPrefab(inventory, IRON_NUGGET);
		ctx.Equal(1, stacks.Count(), "the nuggets merge into one stack (the picked entities are deleted, which the harness read as not_inserted)");
		ctx.Equal(3, LF_HarnessCarries.CountCarried(miner, "IronNugget"), "the harness counts the stack's quantity");
		ctx.Equal(3, LF_InventoryUtils.GetAmount(miner, IRON_NUGGET), "the inventory holds 3 iron ore");

		LF_ActivityLiveFacts facts = new LF_ActivityLiveFacts(miner);
		ctx.Equal(3, facts.Carried(IRON_NUGGET), "the mining run's ore step sees 3 carried");

		LF_TestCharacterFixture.Delete(miner);
	}
}
