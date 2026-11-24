// red-proof: observed 2026-11-24 in the #4821 batched `--red` run (pack leg): RED-OK everon/tool-tier-wear, "the fine pickaxe lost ten fine uses (expected 95, got 100)". Before #4821 every tier wore at the standard rate (the store always made a standard tool). Green with `--only`.
//! everon/tool-tier-wear (#4821): the pack's worn, standard and fine pickaxes, axes and rods carry their tier
//! on the item, so the same work wears a fine tool slowest and a worn one fastest. Before #4821 every
//! tier wore at the standard rate from one per-player store.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronToolTierWear : LF_Test
{
	static const ResourceName PICKAXE_WORN = "{D9D9C619E82ECEA7}Prefabs/Tools/Tiers/LF_Tool_Pickaxe_Worn.et";
	static const ResourceName PICKAXE_STANDARD = "{A1E3A11E03AEFC5C}Prefabs/Tools/Tiers/LF_Tool_Pickaxe_Standard.et";
	static const ResourceName PICKAXE_FINE = "{67822853D125045C}Prefabs/Tools/Tiers/LF_Tool_Pickaxe_Fine.et";
	static const int SWINGS = 10;

	override string GetName()
	{
		return "everon/tool-tier-wear";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		IEntity wornEntity = LF_Utils.SpawnEntityPrefab(PICKAXE_WORN, character.GetOrigin() + "3 0.5 0");
		IEntity standardEntity = LF_Utils.SpawnEntityPrefab(PICKAXE_STANDARD, character.GetOrigin() + "3 0.5 1");
		IEntity fineEntity = LF_Utils.SpawnEntityPrefab(PICKAXE_FINE, character.GetOrigin() + "3 0.5 2");
		LF_ToolConditionComponent worn = LF_ToolConditionComponent.Of(wornEntity);
		LF_ToolConditionComponent standard = LF_ToolConditionComponent.Of(standardEntity);
		LF_ToolConditionComponent fine = LF_ToolConditionComponent.Of(fineEntity);
		ctx.True(worn && standard && fine, "every tier of pickaxe carries a tool condition");
		if (worn && standard && fine)
		{
			ctx.Equal(LF_ToolCondition.TIER_WORN, worn.GetTier(), "the worn pickaxe is the worn tier");
			ctx.Equal(LF_ToolCondition.TIER_STANDARD, standard.GetTier(), "the standard pickaxe is the standard tier");
			ctx.Equal(LF_ToolCondition.TIER_FINE, fine.GetTier(), "the fine pickaxe is the fine tier");

			for (int i = 0; i < SWINGS; i++)
			{
				LF_ToolWear.OnUse(character, LF_ToolBench.KIND_PICKAXE, wornEntity);
				LF_ToolWear.OnUse(character, LF_ToolBench.KIND_PICKAXE, standardEntity);
				if (!RedProof())
					LF_ToolWear.OnUse(character, LF_ToolBench.KIND_PICKAXE, fineEntity);
				else
					LF_ToolWear.OnUse(character, LF_ToolBench.KIND_PICKAXE, wornEntity);
			}

			ctx.True(fine.GetCondition() > standard.GetCondition(), string.Format("the fine pickaxe wears slower than the standard one (%1 vs %2)", fine.GetCondition(), standard.GetCondition()));
			ctx.True(standard.GetCondition() > worn.GetCondition(), string.Format("the standard pickaxe wears slower than the worn one (%1 vs %2)", standard.GetCondition(), worn.GetCondition()));
			ctx.EqualFloat(100.0 - SWINGS * LF_ToolCondition.WearPerUse(LF_ToolCondition.TIER_FINE), fine.GetCondition(), 0.001, "the fine pickaxe lost ten fine uses");
		}

		LF_TestCharacterFixture.Delete(wornEntity);
		LF_TestCharacterFixture.Delete(standardEntity);
		LF_TestCharacterFixture.Delete(fineEntity);
		LF_TestCharacterFixture.Delete(character);
	}
}
