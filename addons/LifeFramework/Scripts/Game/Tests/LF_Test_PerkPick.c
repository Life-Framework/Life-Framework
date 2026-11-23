// red-proof: 2026-11-23, `--red LF_Test_PerkPick` skips the first counter pick: RED-OK, "the pick is stored" and the change checks fail (42 assertions). Green with `--only`.
//! perks/pick (WK-W8b, #4416): at Mining 10 the labour office counter shows two "Take <perk>" rows
//! (double yield, swift hands) and taking one stores it (LF_TradePerks); at Mining 9 it shows none.
//! A change to the other perk costs $500, is refused in words with $400 (nothing taken, the perk
//! stays), and goes through once a week.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PerkPick : LF_Test
{
	static const ResourceName COUNTER = "{5E7A1C93D24B86F1}Prefabs/Props/LF_LabourCounter.et";
	static const string UID = "test-perk-pick";
	static const string TRADE = "MINER";

	override string GetName()
	{
		return "perks/pick";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_JobTrackDef track = LF_JobLevels.Track(TRADE);
		ctx.True(track != null, "the jobs config has a miner ladder");
		if (!track)
			return;

		array<ref LF_JobXpRow> levelsBefore = LF_JobLevels.ExportAll();
		array<ref LF_TradePerkRow> perksBefore = LF_TradePerks.ExportAll();
		LF_PerkPick.ResetStatics();
		LF_PerkPick.SetDayForTests(100);
		LF_TradePerks.Forget(UID);

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		IEntity counter = null;
		if (character)
			counter = LF_Utils.SpawnEntityPrefab(COUNTER, character.GetOrigin() + "2 0 0");
		ctx.True(counter != null, "the labour counter prefab spawns");

		if (character && counter)
			RunWith(ctx, track, levelsBefore, character, counter);

		LF_Utils.SetUidForTests(null, string.Empty);
		LF_TradePerks.ApplyAll(perksBefore);
		LF_JobLevels.ApplyAll(levelsBefore);
		LF_PerkPick.ResetStatics();
		if (counter)
			SCR_EntityHelper.DeleteEntityAndChildren(counter);
		LF_TestCharacterFixture.Delete(character);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWith(LF_TestContext ctx, LF_JobTrackDef track, array<ref LF_JobXpRow> levelsBefore, IEntity character, IEntity counter)
	{
		LF_Utils.SetUidForTests(character, UID);
		array<LF_PerkPickAction> rows = {};
		Rows(counter, rows);
		ctx.Equal(35, rows.Count(), "the counter has 35 perk rows (five take rows, five change rows per trade)");

		// Level 9: no row.
		SetLevel(levelsBefore, track, 9);
		ctx.Equal(9, LF_JobLevels.LevelOf(UID, TRADE), "the miner is level 9");
		ctx.Equal(0, Shown(rows, character, null), "a level-9 miner sees no perk row");

		// Level 10: two free "Take" rows, the level-20 rows stay hidden.
		SetLevel(levelsBefore, track, 10);
		ctx.Equal(10, LF_JobLevels.LevelOf(UID, TRADE), "the miner is level 10");
		array<LF_PerkPickAction> shown = {};
		ctx.Equal(2, Shown(rows, character, shown), "a level-10 miner sees two perk rows");
		foreach (LF_PerkPickAction row : shown)
		{
			LF_PerkPickOffer offer = row.OfferFor(character);
			ctx.Equal(10, offer.m_iAtLevel, "a shown row is a level-10 pick");
			ctx.False(offer.m_bChange, "the first pick is a free take, not a change");
			ctx.EqualStr(TRADE, offer.m_sTrade, "the row is for mining");
		}

		// Taking double yield stores it and costs nothing.
		LF_PerkPickAction take = Row(rows, 10, LF_PerkEffects.PERK_DOUBLE_YIELD, false);
		ctx.True(take != null, "the counter has a double-yield row at level 10");
		if (!take)
			return;

		int cashStart = LF_MoneyUtils.GetCash(character);
		if (!RedProof())
			take.PerformAction(counter, character);
		ctx.EqualStr(LF_PerkEffects.PERK_DOUBLE_YIELD, LF_TradePerks.PerkAt(UID, TRADE, 10), "the pick is stored");
		ctx.Equal(cashStart, LF_MoneyUtils.GetCash(character), "the first pick is free");

		// Now one row: the paid change to swift hands.
		shown.Clear();
		ctx.Equal(1, Shown(rows, character, shown), "after the pick only the change row shows at level 10");
		LF_PerkPickAction change = Row(rows, 10, LF_PerkEffects.PERK_SWIFT_HANDS, true);
		if (!change)
			return;

		LF_PerkPickOffer changeOffer = change.OfferFor(character);
		ctx.True(changeOffer && changeOffer.m_bChange, "the swift-hands row is a paid change");

		// A stale free-take row for the other perk never charges: the server refuses it.
		LF_MoneyUtils.AddCash(character, 1000);
		int cashStale = LF_MoneyUtils.GetCash(character);
		ctx.Equal(LF_EPerkPickResult.NOT_OFFERED, LF_PerkPick.Take(character, 10, LF_PerkEffects.PERK_SWIFT_HANDS, LF_PerkPick.CHANGE_FEE, false), "a take row after the pick is refused");
		ctx.Equal(cashStale, LF_MoneyUtils.GetCash(character), "a refused take row takes no cash");
		ctx.EqualStr(LF_PerkEffects.PERK_DOUBLE_YIELD, LF_TradePerks.PerkAt(UID, TRADE, 10), "a refused take row keeps the perk");

		// $400: refused in words, nothing taken, the perk stays.
		LF_MoneyUtils.RemoveCash(character, LF_MoneyUtils.GetCash(character));
		LF_MoneyUtils.AddCash(character, 400);
		LF_EPerkPickResult broke = LF_PerkPick.Take(character, 10, LF_PerkEffects.PERK_SWIFT_HANDS, LF_PerkPick.CHANGE_FEE, true);
		ctx.Equal(LF_EPerkPickResult.NO_MONEY, broke, "$400 does not pay for a change");
		ctx.EqualStr("#LF-PerkPick_NoMoney", LF_PerkPick.ResultKey(broke), "the refusal is said in words");
		ctx.Equal(400, LF_MoneyUtils.GetCash(character), "a refused change takes no cash");
		ctx.EqualStr(LF_PerkEffects.PERK_DOUBLE_YIELD, LF_TradePerks.PerkAt(UID, TRADE, 10), "a refused change keeps the perk");

		// $1,400: the change takes $500 once and swaps the perk.
		LF_MoneyUtils.AddCash(character, 1000);
		change.PerformAction(counter, character);
		ctx.EqualStr(LF_PerkEffects.PERK_SWIFT_HANDS, LF_TradePerks.PerkAt(UID, TRADE, 10), "the change swaps the perk");
		ctx.Equal(900, LF_MoneyUtils.GetCash(character), "the change takes $500");

		// A second change the same week: refused, no cash taken.
		LF_EPerkPickResult again = LF_PerkPick.Take(character, 10, LF_PerkEffects.PERK_DOUBLE_YIELD, LF_PerkPick.CHANGE_FEE, true);
		ctx.Equal(LF_EPerkPickResult.WEEK_LOCK, again, "a second change inside the week is refused");
		ctx.Equal(900, LF_MoneyUtils.GetCash(character), "the week lock takes no cash");
		ctx.EqualStr(LF_PerkEffects.PERK_SWIFT_HANDS, LF_TradePerks.PerkAt(UID, TRADE, 10), "the week lock keeps the perk");

		// A week later it may change again.
		LF_PerkPick.SetDayForTests(107);
		LF_EPerkPickResult later = LF_PerkPick.Take(character, 10, LF_PerkEffects.PERK_DOUBLE_YIELD, LF_PerkPick.CHANGE_FEE, true);
		ctx.Equal(LF_EPerkPickResult.CHANGED, later, "a change a week later goes through");
		ctx.Equal(400, LF_MoneyUtils.GetCash(character), "the second change takes another $500");

		// A level-9 miner cannot take a level-10 perk even by asking the server.
		LF_TradePerks.Forget(UID);
		SetLevel(levelsBefore, track, 9);
		ctx.Equal(LF_EPerkPickResult.NOT_OFFERED, LF_PerkPick.Take(character, 10, LF_PerkEffects.PERK_DOUBLE_YIELD, 0, false), "the server refuses a pick under level 10");
		ctx.EqualStr(string.Empty, LF_TradePerks.PerkAt(UID, TRADE, 10), "nothing is stored under level 10");
	}

	//------------------------------------------------------------------------------------------------
	protected void SetLevel(array<ref LF_JobXpRow> before, LF_JobTrackDef track, int level)
	{
		array<ref LF_JobXpRow> rows = {};
		foreach (LF_JobXpRow row : before)
		{
			if (row && row.m_sPlayerUid != UID)
				rows.Insert(row.Copy());
		}

		rows.Insert(LF_JobXpRow.Create(UID, TRADE, track.XpForLevel(level)));
		LF_JobLevels.ApplyAll(rows);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Rows(IEntity counter, notnull array<LF_PerkPickAction> into)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(counter.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction candidate : actions)
		{
			LF_PerkPickAction row = LF_PerkPickAction.Cast(candidate);
			if (row)
				into.Insert(row);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static int Shown(notnull array<LF_PerkPickAction> rows, IEntity user, array<LF_PerkPickAction> into)
	{
		int count;
		foreach (LF_PerkPickAction row : rows)
		{
			if (!row.OfferFor(user))
				continue;

			count++;
			if (into)
				into.Insert(row);
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PerkPickAction Row(notnull array<LF_PerkPickAction> rows, int atLevel, string perk, bool change)
	{
		foreach (LF_PerkPickAction row : rows)
		{
			if (row.GetAtLevel() == atLevel && row.GetPerk() == perk && row.IsChange() == change)
				return row;
		}

		return null;
	}
}
