// red-proof: 2026-11-23, `--red LF_Test_PerkPickDuty` skips the three free picks (good word, fast cuff, quick revive): RED-OK perks/pick-duty (44 assertions), "good word is stored at Mining 20", "with good word iron ore pays 10% more (expected 110, got 100)", "the officer holds fast cuff", "quick revive is stored at MEDIC 10" fail. Green with `--only`.
//! perks/pick-duty (#4573): good word is a level-20 pick at the labour office counter in a trade whose
//! goods a trader buys, and the pick makes the sale pay 10% more; smelting (no goods) is never offered
//! it. The police dispatch board offers fast cuff or long radar at POLICE 10, the clinic's dispatch
//! board quick revive or quick aid at MEDIC 10 and steady carry at 20. A duty perk is free to take, a
//! change costs $500 in cash (refused with $400, nothing taken), and the rows of one place never show
//! another place's perks. The owner sync carries the duty levels.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PerkPickDuty : LF_Test
{
	static const ResourceName COUNTER = "{5E7A1C93D24B86F1}Prefabs/Props/LF_LabourCounter.et";
	static const ResourceName POLICE_BOARD = "{5BC90DB066B123D4}Prefabs/Comms/LF_DispatchBoard_Police.et";
	static const ResourceName MEDIC_BOARD = "{38D6A62C995DACCA}Prefabs/Comms/LF_DispatchBoard_Medic.et";
	static const ResourceName IRON_ORE = "{2C571D6F7C05E095}Prefabs/Resources/Mining/IronOre.et";
	static const string UID = "test-perk-pick-duty";

	override string GetName()
	{
		return "perks/pick-duty";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_JobXpRow> levelsBefore = LF_JobLevels.ExportAll();
		array<ref LF_TradePerkRow> perksBefore = LF_TradePerks.ExportAll();
		LF_PerkPick.ResetStatics();
		LF_PerkPick.SetDayForTests(200);
		LF_TradePerks.Forget(UID);

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		IEntity counter = null;
		IEntity police = null;
		IEntity medic = null;
		if (character)
		{
			counter = LF_Utils.SpawnEntityPrefab(COUNTER, character.GetOrigin() + "2 0 0");
			police = LF_Utils.SpawnEntityPrefab(POLICE_BOARD, character.GetOrigin() + "-2 0 0");
			medic = LF_Utils.SpawnEntityPrefab(MEDIC_BOARD, character.GetOrigin() + "0 0 2");
		}
		ctx.True(counter != null && police != null && medic != null, "the counter and both dispatch boards spawn");

		if (character && counter && police && medic)
		{
			LF_Utils.SetUidForTests(character, UID);
			GoodWord(ctx, levelsBefore, character, counter, police);
			Police(ctx, levelsBefore, character, counter, police);
			Medic(ctx, levelsBefore, character, medic);
		}

		LF_Utils.SetUidForTests(null, string.Empty);
		LF_TradePerks.ApplyAll(perksBefore);
		LF_JobLevels.ApplyAll(levelsBefore);
		LF_PerkPick.ResetStatics();
		if (counter)
			SCR_EntityHelper.DeleteEntityAndChildren(counter);
		if (police)
			SCR_EntityHelper.DeleteEntityAndChildren(police);
		if (medic)
			SCR_EntityHelper.DeleteEntityAndChildren(medic);
		LF_TestCharacterFixture.Delete(character);
	}

	//------------------------------------------------------------------------------------------------
	protected void GoodWord(LF_TestContext ctx, array<ref LF_JobXpRow> before, IEntity character, IEntity counter, IEntity police)
	{
		LF_TradePerks.Forget(UID);
		SetLevels(before, "MINER", 20, "SMELTING", 20);
		LF_TradePerks.Pick(UID, "MINER", 10, LF_PerkEffects.PERK_DOUBLE_YIELD);
		LF_TradePerks.Pick(UID, "SMELTING", 10, LF_PerkEffects.PERK_DOUBLE_YIELD);

		array<LF_PerkPickAction> rows = {};
		Rows(counter, rows);
		LF_PerkPickAction take = Row(rows, 20, LF_PerkPrice.PERK_GOOD_WORD, false);
		ctx.True(take != null, "the counter has a good-word take row at level 20");
		if (!take)
			return;

		LF_PerkPickOffer offer = take.OfferFor(character);
		ctx.True(offer != null, "a Mining 20 miner is offered good word");
		if (offer)
			ctx.EqualStr("MINER", offer.m_sTrade, "good word is offered in mining");

		array<ref LF_PerkPickOffer> offers = {};
		LF_PerkPick.Offers(UID, offers);
		ctx.True(LF_PerkPick.Find(offers, 20, LF_PerkPrice.PERK_GOOD_WORD, false, "SMELTING") == null, "smelting (no goods a trader buys) is never offered good word");
		ctx.Equal(0, Shown(Board(police), character), "a miner sees no row on the police board");

		ctx.Equal(100, LF_PerkPrice.Adjust(UID, IRON_ORE, 100), "before the pick iron ore pays the plain price");
		int cash = LF_MoneyUtils.GetCash(character);
		if (!RedProof())
			take.PerformAction(counter, character);
		ctx.EqualStr(LF_PerkPrice.PERK_GOOD_WORD, LF_TradePerks.PerkAt(UID, "MINER", 20), "good word is stored at Mining 20");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(character), "the good-word pick is free");
		ctx.Equal(110, LF_PerkPrice.Adjust(UID, IRON_ORE, 100), "with good word iron ore pays 10% more");
		ctx.EqualStr("#LF-PerkPick_Perk_GoodWord", LF_PerkPick.PerkNameKey(LF_PerkPrice.PERK_GOOD_WORD), "good word has a name row");

		// The mining good-word change row is now a change away from it, for mining only.
		LF_PerkPickAction light = TradeRow(rows, 20, LF_PerkEffects.PERK_LIGHT_TOUCH, "MINER");
		ctx.True(light != null && light.OfferFor(character) != null, "the miner may change good word for light touch");
	}

	//------------------------------------------------------------------------------------------------
	protected void Police(LF_TestContext ctx, array<ref LF_JobXpRow> before, IEntity character, IEntity counter, IEntity police)
	{
		LF_TradePerks.Forget(UID);
		array<LF_PerkPickAction> rows = {};
		Rows(police, rows);
		ctx.Equal(8, rows.Count(), "the police board has 8 perk rows (four take rows, four change rows)");

		SetLevels(before, "POLICE", 9, string.Empty, 0);
		ctx.Equal(0, Shown(rows, character), "a POLICE 9 officer sees no perk row");

		SetLevels(before, "POLICE", 10, string.Empty, 0);
		ctx.Equal(2, Shown(rows, character), "a POLICE 10 officer sees two take rows");
		ctx.Equal(0, Shown(Board(counter), character), "an officer sees no row at the labour counter");

		LF_PerkPickAction cuff = Row(rows, 10, LF_PoliceSkill.PERK_FAST_CUFF, false);
		ctx.True(cuff != null, "the police board has a fast-cuff row at level 10");
		if (!cuff)
			return;

		int cash = LF_MoneyUtils.GetCash(character);
		if (!RedProof())
			cuff.PerformAction(police, character);
		ctx.True(LF_PoliceSkill.Has(UID, LF_PoliceSkill.PERK_FAST_CUFF), "the officer holds fast cuff");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(character), "the duty pick is free");

		// The change to long radar: refused with $400, $500 with $1,400.
		LF_MoneyUtils.RemoveCash(character, LF_MoneyUtils.GetCash(character));
		LF_MoneyUtils.AddCash(character, 400);
		ctx.Equal(LF_EPerkPickResult.NO_MONEY, LF_PerkPick.Take(character, 10, LF_PoliceSkill.PERK_LONG_RADAR, LF_PerkPick.CHANGE_FEE, true, "POLICE"), "$400 does not pay for a duty perk change");
		ctx.Equal(400, LF_MoneyUtils.GetCash(character), "a refused duty change takes no cash");

		LF_MoneyUtils.AddCash(character, 1000);
		LF_PerkPickAction change = TradeRow(rows, 10, LF_PoliceSkill.PERK_LONG_RADAR, "POLICE");
		ctx.True(change != null && change.OfferFor(character) != null, "the long-radar change row shows after the pick");
		if (change)
			change.PerformAction(police, character);
		ctx.True(LF_PoliceSkill.Has(UID, LF_PoliceSkill.PERK_LONG_RADAR), "the change swaps to long radar");
		ctx.False(LF_PoliceSkill.Has(UID, LF_PoliceSkill.PERK_FAST_CUFF), "fast cuff is gone after the change");
		ctx.Equal(900, LF_MoneyUtils.GetCash(character), "the duty change takes $500");

		// A take row cannot pick a police perk at level 20 without POLICE 20.
		ctx.Equal(LF_EPerkPickResult.NOT_OFFERED, LF_PerkPick.Take(character, 20, LF_PoliceSkill.PERK_FAST_CUFF, 0, false), "POLICE 10 cannot take a level-20 perk");

		// The owner sync carries the duty level, so a client labels the board rows.
		string wire = LF_PerkSync.Encode(UID);
		ctx.True(wire.Contains("L|POLICE|10"), "the owner sync carries the POLICE level");
	}

	//------------------------------------------------------------------------------------------------
	protected void Medic(LF_TestContext ctx, array<ref LF_JobXpRow> before, IEntity character, IEntity medic)
	{
		LF_TradePerks.Forget(UID);
		array<LF_PerkPickAction> rows = {};
		Rows(medic, rows);
		ctx.Equal(10, rows.Count(), "the clinic board has 10 perk rows (five take rows, five change rows)");

		SetLevels(before, "MEDIC", 20, string.Empty, 0);
		array<LF_PerkPickAction> shown = {};
		ctx.Equal(2, Shown(rows, character, shown), "a MEDIC 20 medic with no pick sees the two level-10 rows only");

		LF_PerkPickAction revive = Row(rows, 10, LF_PerkPick.PERK_QUICK_REVIVE, false);
		if (revive && !RedProof())
			revive.PerformAction(medic, character);
		ctx.EqualStr(LF_PerkPick.PERK_QUICK_REVIVE, LF_TradePerks.PerkAt(UID, "MEDIC", 10), "quick revive is stored at MEDIC 10");

		LF_PerkPickAction carry = Row(rows, 20, LF_PerkPick.PERK_STEADY_CARRY, false);
		ctx.True(carry != null && carry.OfferFor(character) != null, "steady carry is offered at MEDIC 20 after the first pick");
		LF_PerkPickAction again = Row(rows, 20, LF_PerkPick.PERK_QUICK_REVIVE, false);
		ctx.True(again != null && again.OfferFor(character) == null, "quick revive is not offered twice");
		if (carry)
			carry.PerformAction(medic, character);
		ctx.True(LF_TradePerks.Has(UID, "MEDIC", LF_PerkPick.PERK_STEADY_CARRY), "the medic holds steady carry");
	}

	//------------------------------------------------------------------------------------------------
	protected void SetLevels(array<ref LF_JobXpRow> before, string tradeA, int levelA, string tradeB, int levelB)
	{
		array<ref LF_JobXpRow> rows = {};
		foreach (LF_JobXpRow row : before)
		{
			if (row && row.m_sPlayerUid != UID)
				rows.Insert(row.Copy());
		}

		AddLevel(rows, tradeA, levelA);
		AddLevel(rows, tradeB, levelB);
		LF_JobLevels.ApplyAll(rows);
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddLevel(notnull array<ref LF_JobXpRow> rows, string trade, int level)
	{
		if (trade.IsEmpty())
			return;

		LF_JobTrackDef track = LF_JobLevels.Track(trade);
		if (track)
			rows.Insert(LF_JobXpRow.Create(UID, trade, track.XpForLevel(level)));
	}

	//------------------------------------------------------------------------------------------------
	protected static array<LF_PerkPickAction> Board(IEntity prop)
	{
		array<LF_PerkPickAction> rows = {};
		Rows(prop, rows);
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Rows(IEntity prop, notnull array<LF_PerkPickAction> into)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(prop.FindComponent(BaseActionsManagerComponent));
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
	protected static int Shown(notnull array<LF_PerkPickAction> rows, IEntity user, array<LF_PerkPickAction> into = null)
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

	//------------------------------------------------------------------------------------------------
	protected static LF_PerkPickAction TradeRow(notnull array<LF_PerkPickAction> rows, int atLevel, string perk, string trade)
	{
		foreach (LF_PerkPickAction row : rows)
		{
			if (row.GetAtLevel() == atLevel && row.GetPerk() == perk && row.IsChange() && row.GetTrade() == trade)
				return row;
		}

		return null;
	}
}
