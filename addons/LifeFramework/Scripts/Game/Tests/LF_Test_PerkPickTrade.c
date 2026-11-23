// red-proof: observed 2026-11-23 `tools/cli test --no-build --red LF_Test_PerkPickTrade`: RED-OK perks/pick-trade (16 assertions); with the lumberjack row not performed it fails "the lumberjack's perk changes to swift hands" and "the change takes $500 once". Green with `--only`.
//! perks/pick-trade (#4525): a Miner and a Lumberjack who both picked double yield at level 10 each
//! see their own "Change to Swift hands" row at the labour office counter. The Lumberjack's row
//! changes the Lumberjack's perk for $500 and leaves the Miner's alone; before #4525 one change row
//! per perk served only the first trade, so the Lumberjack's change was out of reach.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PerkPickTrade : LF_Test
{
	static const ResourceName COUNTER = "{5E7A1C93D24B86F1}Prefabs/Props/LF_LabourCounter.et";
	static const string UID = "test-perk-pick-trade";
	static const string MINER = "MINER";
	static const string LUMBER = "LUMBERJACK";

	override string GetName()
	{
		return "perks/pick-trade";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_JobTrackDef miner = LF_JobLevels.Track(MINER);
		LF_JobTrackDef lumber = LF_JobLevels.Track(LUMBER);
		ctx.True(miner != null && lumber != null, "the jobs config has miner and lumberjack ladders");
		if (!miner || !lumber)
			return;

		array<ref LF_JobXpRow> levelsBefore = LF_JobLevels.ExportAll();
		array<ref LF_TradePerkRow> perksBefore = LF_TradePerks.ExportAll();
		LF_PerkPick.ResetStatics();
		LF_PerkPick.SetDayForTests(200);
		LF_TradePerks.Forget(UID);

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		IEntity counter = null;
		if (character)
			counter = LF_Utils.SpawnEntityPrefab(COUNTER, character.GetOrigin() + "2 0 0");
		ctx.True(counter != null, "the labour counter prefab spawns");

		if (character && counter)
			RunWith(ctx, miner, lumber, levelsBefore, character, counter);

		LF_Utils.SetUidForTests(null, string.Empty);
		LF_TradePerks.ApplyAll(perksBefore);
		LF_JobLevels.ApplyAll(levelsBefore);
		LF_PerkPick.ResetStatics();
		if (counter)
			SCR_EntityHelper.DeleteEntityAndChildren(counter);
		LF_TestCharacterFixture.Delete(character);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWith(LF_TestContext ctx, LF_JobTrackDef miner, LF_JobTrackDef lumber, array<ref LF_JobXpRow> before, IEntity character, IEntity counter)
	{
		LF_Utils.SetUidForTests(character, UID);
		array<ref LF_JobXpRow> rows = {};
		foreach (LF_JobXpRow row : before)
		{
			if (row && row.m_sPlayerUid != UID)
				rows.Insert(row.Copy());
		}

		rows.Insert(LF_JobXpRow.Create(UID, MINER, miner.XpForLevel(10)));
		rows.Insert(LF_JobXpRow.Create(UID, LUMBER, lumber.XpForLevel(10)));
		LF_JobLevels.ApplyAll(rows);
		LF_TradePerks.Pick(UID, MINER, 10, LF_PerkEffects.PERK_DOUBLE_YIELD);
		LF_TradePerks.Pick(UID, LUMBER, 10, LF_PerkEffects.PERK_DOUBLE_YIELD);

		array<LF_PerkPickAction> counterRows = {};
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(counter.FindComponent(BaseActionsManagerComponent));
		if (manager)
		{
			array<BaseUserAction> actions = {};
			manager.GetActionsList(actions);
			foreach (BaseUserAction candidate : actions)
			{
				LF_PerkPickAction row = LF_PerkPickAction.Cast(candidate);
				if (row)
					counterRows.Insert(row);
			}
		}

		// Both trades show their own change row.
		LF_PerkPickAction lumberRow = null;
		int changeRows = 0;
		foreach (LF_PerkPickAction row : counterRows)
		{
			LF_PerkPickOffer offer = row.OfferFor(character);
			if (!offer || !offer.m_bChange)
				continue;

			changeRows++;
			if (offer.m_sTrade == LUMBER && offer.m_sPerk == LF_PerkEffects.PERK_SWIFT_HANDS && offer.m_iAtLevel == 10)
				lumberRow = row;
		}

		ctx.Equal(2, changeRows, "a miner and a lumberjack who both picked see two change rows");
		ctx.True(lumberRow != null, "the lumberjack has its own change-to-swift-hands row");
		if (!lumberRow)
			return;

		ctx.EqualStr(LUMBER, lumberRow.GetTrade(), "the lumberjack's row names the lumberjack trade");

		// The lumberjack's row changes the lumberjack's perk for $500, the miner's stays.
		LF_MoneyUtils.RemoveCash(character, LF_MoneyUtils.GetCash(character));
		LF_MoneyUtils.AddCash(character, 600);
		if (!RedProof())
			lumberRow.PerformAction(counter, character);

		ctx.EqualStr(LF_PerkEffects.PERK_SWIFT_HANDS, LF_TradePerks.PerkAt(UID, LUMBER, 10), "the lumberjack's perk changes to swift hands");
		ctx.EqualStr(LF_PerkEffects.PERK_DOUBLE_YIELD, LF_TradePerks.PerkAt(UID, MINER, 10), "the miner keeps double yield");
		ctx.Equal(100, LF_MoneyUtils.GetCash(character), "the change takes $500 once");
		ctx.True(LF_PerkPick.DaysLocked(UID, LUMBER, 10) > 0, "the lumberjack's change starts its week lock");
		ctx.Equal(0, LF_PerkPick.DaysLocked(UID, MINER, 10), "the miner's change is not locked");

		// A change row naming a trade with no such offer is refused before any cash moves.
		LF_MoneyUtils.AddCash(character, 900);
		ctx.Equal(LF_EPerkPickResult.NOT_OFFERED, LF_PerkPick.Take(character, 10, LF_PerkEffects.PERK_SWIFT_HANDS, LF_PerkPick.CHANGE_FEE, true, "FARMER"), "a change for a trade with no pick is refused");
		ctx.Equal(1000, LF_MoneyUtils.GetCash(character), "the refused change takes no cash");
	}
}
