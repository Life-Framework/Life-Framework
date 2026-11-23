//------------------------------------------------------------------------------------------------
//! The outcome of one perk pick at the labour office counter (WK-W8b, #4416).
enum LF_EPerkPickResult
{
	TAKEN,
	CHANGED,
	NOT_OFFERED,
	NO_MONEY,
	WEEK_LOCK,
	FAILED
}

//------------------------------------------------------------------------------------------------
//! One choice the counter offers a character: take (or change to) `m_sPerk` in `m_sTrade` at pick
//! level `m_iAtLevel`. `m_bChange` is a paid swap of a perk already picked at that level.
class LF_PerkPickOffer
{
	string m_sTrade;
	int m_iAtLevel;
	string m_sPerk;
	bool m_bChange;

	//------------------------------------------------------------------------------------------------
	static LF_PerkPickOffer Make(string trade, int atLevel, string perk, bool change)
	{
		LF_PerkPickOffer offer = new LF_PerkPickOffer();
		offer.m_sTrade = trade;
		offer.m_iAtLevel = atLevel;
		offer.m_sPerk = perk;
		offer.m_bChange = change;
		return offer;
	}
}

//------------------------------------------------------------------------------------------------
//! One paid perk change (#4525): the character, the trade, the pick level and the in-game day number
//! of the change. Saved in the "tradeperks" section from version 2. Positional in a binary save:
//! never add, remove or reorder a field.
class LF_PerkChangeRow
{
	string m_sPlayerUid;
	string m_sTrade;
	int m_iAtLevel;
	int m_iDay;

	//------------------------------------------------------------------------------------------------
	static LF_PerkChangeRow Create(string playerUid, string trade, int atLevel, int day)
	{
		LF_PerkChangeRow row = new LF_PerkChangeRow();
		row.m_sPlayerUid = playerUid;
		row.m_sTrade = trade;
		row.m_iAtLevel = atLevel;
		row.m_iDay = day;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The perk pick (WK-W8b, #4416). At level 10 of a work trade a character picks one of two passive
//! perks (LF_PerkEffects), at level 20 one of two more, at the labour office counter. The first pick
//! at a level is free; changing it costs CHANGE_FEE cash, once an in-game week (decision N10). The
//! cash is taken before the perk changes and given back if the store refuses.
//!
//! A work trade's pool at level 10 is double yield or swift hands; at level 20 light touch, good word
//! (in a trade whose goods a trader buys, LF_PerkPrice) or the level-10 perk not taken. The duty trades
//! pick at their own boards (#4573): POLICE fast cuff or long radar at 10 and the other at 20 on the
//! police dispatch board; MEDIC quick revive or quick aid at 10, steady carry or the other at 20 on
//! the clinic's. A perk held at the other level is never offered twice, and a level-20 pick needs a
//! level-10 pick first, so the F2 card's perk list stays in pick-level order.
//!
//! The picks live in LF_TradePerks and the once-a-week change clock here; both are saved in the
//! "tradeperks" section (LF_TradePerksSavePayload, version 2 added the clock, #4525).
//!
//! A change row names its trade (#4525): two trades that both hold a pick each get their own change
//! row, so the second trade's change is not stuck behind the first's.
class LF_PerkPick
{
	static const string FEATURE = "PerkPick";
	static const int CHANGE_FEE = 500; // lf-fixed: decision N10, "$500, once an in-game week"
	static const int WEEK_DAYS = 7; // lf-fixed: one in-game week between two paid changes
	static const int LEVEL_FIRST = 10; // lf-fixed: the work plan's first pick level
	static const int LEVEL_SECOND = 20; // lf-fixed: the work plan's second pick level
	static const string REASON = "perk_change";

	static const string JOB_POLICE = "POLICE";
	static const string JOB_MEDIC = "MEDIC";
	//! The medic perk ids LF_MedicSkill reads (#4433); the police ids are LF_PoliceSkill's.
	static const string PERK_QUICK_REVIVE = "quick_revive";
	static const string PERK_QUICK_AID = "quick_aid";
	static const string PERK_STEADY_CARRY = "steady_carry";

	//! "<uid>|<trade>|<atLevel>" -> in-game day number of the last paid change. Saved (tradeperks v2).
	protected static ref map<string, int> s_mChangedDay;
	//! Tests: the in-game day number to use instead of the world clock; -1 for the clock.
	protected static int s_iTestDay = -1;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mChangedDay = null;
		s_iTestDay = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: pins today's in-game day number; -1 goes back to the world clock.
	static void SetDayForTests(int day)
	{
		s_iTestDay = day;
	}

	//------------------------------------------------------------------------------------------------
	//! Every trade with a perk pick, in the order the rows serve them: the six work trades of the labour
	//! office counter, then the duty trades of the police and clinic boards (#4573). The owner sync
	//! carries the level of each (LF_PerkSync).
	static void Trades(notnull array<string> into)
	{
		into.Clear();
		into.Insert("MINER");
		into.Insert("LUMBERJACK");
		into.Insert("FARMER");
		into.Insert("FISHER");
		into.Insert("SMELTING");
		into.Insert("SALVAGE");
		into.Insert(JOB_POLICE);
		into.Insert(JOB_MEDIC);
	}

	//------------------------------------------------------------------------------------------------
	//! The perks a trade's pick level draws from, before the ones held are taken out.
	static void Pool(string trade, int atLevel, notnull array<string> into)
	{
		into.Clear();
		if (atLevel != LEVEL_FIRST && atLevel != LEVEL_SECOND)
			return;

		if (trade == JOB_POLICE)
		{
			into.Insert(LF_PoliceSkill.PERK_FAST_CUFF);
			into.Insert(LF_PoliceSkill.PERK_LONG_RADAR);
			return;
		}

		if (trade == JOB_MEDIC)
		{
			if (atLevel == LEVEL_SECOND)
				into.Insert(PERK_STEADY_CARRY);
			into.Insert(PERK_QUICK_REVIVE);
			into.Insert(PERK_QUICK_AID);
			return;
		}

		if (atLevel == LEVEL_SECOND)
		{
			into.Insert(LF_PerkEffects.PERK_LIGHT_TOUCH);
			if (LF_PerkPrice.PaysIn(trade))
				into.Insert(LF_PerkPrice.PERK_GOOD_WORD);
		}

		into.Insert(LF_PerkEffects.PERK_DOUBLE_YIELD);
		into.Insert(LF_PerkEffects.PERK_SWIFT_HANDS);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the offers for one trade, given the level and the perks picked at 10 and 20.
	static void OffersFrom(string trade, int level, string pickFirst, string pickSecond, notnull array<ref LF_PerkPickOffer> into)
	{
		AddLevel(trade, level, LEVEL_FIRST, pickFirst, pickSecond, into);
		if (!pickFirst.IsEmpty())
			AddLevel(trade, level, LEVEL_SECOND, pickSecond, pickFirst, into);
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddLevel(string trade, int level, int atLevel, string current, string other, notnull array<ref LF_PerkPickOffer> into)
	{
		if (level < atLevel)
			return;

		array<string> pool = {};
		Pool(trade, atLevel, pool);
		foreach (string perk : pool)
		{
			if (perk == current || perk == other)
				continue;

			into.Insert(LF_PerkPickOffer.Make(trade, atLevel, perk, !current.IsEmpty()));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every offer for a character, read from the level and perk stores.
	static void Offers(string uid, notnull array<ref LF_PerkPickOffer> into)
	{
		OffersWith(uid, false, into);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: every offer for its own character, read from the owner sync (LF_PerkSync levels, the
	//! picks it wrote into LF_TradePerks). False when no sync arrived yet.
	static bool OffersSynced(string uid, notnull array<ref LF_PerkPickOffer> into)
	{
		into.Clear();
		if (!LF_PerkSync.Ready(uid))
			return false;

		OffersWith(uid, true, into);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void OffersWith(string uid, bool synced, notnull array<ref LF_PerkPickOffer> into)
	{
		into.Clear();
		if (uid.IsEmpty())
			return;

		array<string> trades = {};
		Trades(trades);
		foreach (string trade : trades)
		{
			int level;
			if (synced)
				level = LF_PerkSync.LevelOf(uid, trade);
			else
				level = LF_JobLevels.LevelOf(uid, trade);

			OffersFrom(trade, level, LF_TradePerks.PerkAt(uid, trade, LEVEL_FIRST), LF_TradePerks.PerkAt(uid, trade, LEVEL_SECOND), into);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The first offer of a perk at a pick level of one kind (a free take or a paid change), trades in
	//! counter order; null when none. A non-empty `trade` takes only that trade's offer (a change row).
	static LF_PerkPickOffer Find(notnull array<ref LF_PerkPickOffer> offers, int atLevel, string perk, bool change, string trade = string.Empty)
	{
		foreach (LF_PerkPickOffer offer : offers)
		{
			if (offer.m_iAtLevel != atLevel || offer.m_sPerk != perk || offer.m_bChange != change)
				continue;

			if (trade.IsEmpty() || offer.m_sTrade == trade)
				return offer;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Today's in-game day number (the world clock's date), or the test day.
	static int Today()
	{
		if (s_iTestDay >= 0)
			return s_iTestDay;

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return 0;

		TimeAndWeatherManagerEntity clock = world.GetTimeAndWeatherManager();
		if (!clock)
			return 0;

		return LF_IslandCalendarRules.DayNumber(clock.GetYear(), clock.GetMonth(), clock.GetDay());
	}

	//------------------------------------------------------------------------------------------------
	//! Days until a character may change the perk at this level again; 0 when it may now.
	static int DaysLocked(string uid, string trade, int atLevel)
	{
		if (!s_mChangedDay)
			return 0;

		int last;
		if (!s_mChangedDay.Find(Key(uid, trade, atLevel), last))
			return 0;

		// A world clock that started over after a restart (an earlier date) never locks past a week.
		return LF_Utils.ClampInt(last + WEEK_DAYS - Today(), 0, WEEK_DAYS);
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the week lock of a pick level today (a paid change; tests).
	static void MarkChanged(string uid, string trade, int atLevel)
	{
		if (!s_mChangedDay)
			s_mChangedDay = new map<string, int>();

		s_mChangedDay.Set(Key(uid, trade, atLevel), Today());
	}

	//------------------------------------------------------------------------------------------------
	//! Every change stamp as rows, for the save.
	static array<ref LF_PerkChangeRow> ExportChanges()
	{
		array<ref LF_PerkChangeRow> rows = new array<ref LF_PerkChangeRow>();
		if (!s_mChangedDay)
			return rows;

		foreach (string key, int day : s_mChangedDay)
		{
			array<string> parts = {};
			key.Split("|", parts, false);
			if (parts.Count() == 3)
				rows.Insert(LF_PerkChangeRow.Create(parts[0], parts[1], parts[2].ToInt(), day));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the change stamps with the rows (null or empty clears them), so a second load ends in
	//! the same state. \return How many rows were kept; a row with a missing field is skipped.
	static int ApplyChanges(array<ref LF_PerkChangeRow> rows)
	{
		s_mChangedDay = new map<string, int>();
		if (!rows)
			return 0;

		int kept;
		foreach (LF_PerkChangeRow row : rows)
		{
			if (!row || row.m_sPlayerUid.IsEmpty() || row.m_sTrade.IsEmpty() || row.m_iAtLevel <= 0 || row.m_sPlayerUid.Contains("|") || row.m_sTrade.Contains("|"))
			{
				LF_Debug.Warn(FEATURE, "a saved perk change row has a missing field and is skipped");
				continue;
			}

			s_mChangedDay.Set(Key(row.m_sPlayerUid, row.m_sTrade, row.m_iAtLevel), row.m_iDay);
			kept++;
		}

		return kept;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Key(string uid, string trade, int atLevel)
	{
		return uid + "|" + trade + "|" + atLevel.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Takes `perk` at pick level `atLevel` for the user, in the first trade that offers
	//! it. A first pick is free; a change takes `fee` cash first and is refused inside the week. `change`
	//! is the kind of row the player used: a take row never charges, a change row never picks for free,
	//! so a row the client labelled from a stale view is refused before any cash moves. `trade` is the
	//! trade a change row names; empty takes the first trade that offers the perk.
	static LF_EPerkPickResult Take(IEntity user, int atLevel, string perk, int fee, bool change, string trade = string.Empty)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
		{
			LF_Debug.Warn(FEATURE, "pick asked off the server, ignored");
			return LF_EPerkPickResult.FAILED;
		}

		string uid = LF_Utils.GetPlayerUID(user);
		if (!user || uid.IsEmpty() || perk.IsEmpty() || fee < 0)
		{
			LF_Debug.Error(FEATURE, string.Format("pick with a bad input (user %1, uid '%2', perk '%3', fee %4)", user != null, uid, perk, fee));
			return LF_EPerkPickResult.FAILED;
		}

		array<ref LF_PerkPickOffer> offers = {};
		Offers(uid, offers);
		LF_PerkPickOffer offer = Find(offers, atLevel, perk, change, trade);
		if (!offer)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 refused %2 at level %3 (trade '%4'): not offered", uid, perk, atLevel, trade));
			return LF_EPerkPickResult.NOT_OFFERED;
		}

		int taken = 0;
		if (offer.m_bChange)
		{
			if (DaysLocked(uid, offer.m_sTrade, atLevel) > 0)
			{
				LF_Debug.Info(FEATURE, string.Format("%1 refused a change in %2 at level %3: changed this week", uid, offer.m_sTrade, atLevel));
				return LF_EPerkPickResult.WEEK_LOCK;
			}

			if (LF_MoneyUtils.GetCash(user) < fee)
			{
				LF_Debug.Info(FEATURE, string.Format("%1 refused a change in %2: cash %3 under %4", uid, offer.m_sTrade, LF_MoneyUtils.GetCash(user), fee));
				return LF_EPerkPickResult.NO_MONEY;
			}

			if (fee > 0)
			{
				taken = LF_MoneyUtils.RemoveCash(user, fee, REASON);
				if (taken != fee)
				{
					if (taken > 0)
						LF_MoneyUtils.AddCash(user, taken, REASON);
					LF_Debug.Warn(FEATURE, string.Format("change undone: took %1 of %2 cash", taken, fee));
					return LF_EPerkPickResult.NO_MONEY;
				}
			}
		}

		if (!LF_TradePerks.Pick(uid, offer.m_sTrade, atLevel, perk))
		{
			if (taken > 0)
				LF_MoneyUtils.AddCash(user, taken, REASON);
			return LF_EPerkPickResult.FAILED;
		}

		if (!offer.m_bChange)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 took %2 in %3 at level %4", uid, perk, offer.m_sTrade, atLevel));
			return LF_EPerkPickResult.TAKEN;
		}

		MarkChanged(uid, offer.m_sTrade, atLevel);
		LF_Debug.Info(FEATURE, string.Format("%1 changed to %2 in %3 at level %4 for %5", uid, perk, offer.m_sTrade, atLevel, fee));
		return LF_EPerkPickResult.CHANGED;
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key that says the outcome in words.
	static string ResultKey(LF_EPerkPickResult result)
	{
		if (result == LF_EPerkPickResult.TAKEN)
			return "#LF-PerkPick_Taken";
		if (result == LF_EPerkPickResult.CHANGED)
			return "#LF-PerkPick_Changed";
		if (result == LF_EPerkPickResult.NOT_OFFERED)
			return "#LF-PerkPick_NotOffered";
		if (result == LF_EPerkPickResult.NO_MONEY)
			return "#LF-PerkPick_NoMoney";
		if (result == LF_EPerkPickResult.WEEK_LOCK)
			return "#LF-PerkPick_WeekLock";
		return "#LF-PerkPick_Failed";
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key of a perk's name.
	static string PerkNameKey(string perk)
	{
		if (perk == LF_PerkEffects.PERK_DOUBLE_YIELD)
			return "#LF-PerkPick_Perk_DoubleYield";
		if (perk == LF_PerkEffects.PERK_SWIFT_HANDS)
			return "#LF-PerkPick_Perk_SwiftHands";
		if (perk == LF_PerkEffects.PERK_LIGHT_TOUCH)
			return "#LF-PerkPick_Perk_LightTouch";
		if (perk == LF_PerkPrice.PERK_GOOD_WORD)
			return "#LF-PerkPick_Perk_GoodWord";
		if (perk == LF_PoliceSkill.PERK_FAST_CUFF)
			return "#LF-PerkPick_Perk_FastCuff";
		if (perk == LF_PoliceSkill.PERK_LONG_RADAR)
			return "#LF-PerkPick_Perk_LongRadar";
		if (perk == PERK_QUICK_REVIVE)
			return "#LF-PerkPick_Perk_QuickRevive";
		if (perk == PERK_QUICK_AID)
			return "#LF-PerkPick_Perk_QuickAid";
		if (perk == PERK_STEADY_CARRY)
			return "#LF-PerkPick_Perk_SteadyCarry";
		return perk;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PerkPick's statics (LF_StaticReset finds it by reflection).
class LF_PerkPick_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PerkPick";
	}

	override void Apply()
	{
		LF_PerkPick.ResetStatics();
	}
}
