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
//! The perk pick (WK-W8b, #4416). At level 10 of a work trade a character picks one of two passive
//! perks (LF_PerkEffects), at level 20 one of two more, at the labour office counter. The first pick
//! at a level is free; changing it costs CHANGE_FEE cash, once an in-game week (decision N10). The
//! cash is taken before the perk changes and given back if the store refuses.
//!
//! The pool at level 10 is double yield or swift hands; at level 20 light touch or the level-10 perk
//! not taken. A perk held at the other level is never offered twice, and a level-20 pick needs a
//! level-10 pick first, so the F2 card's perk list stays in pick-level order.
//!
//! The picks live in LF_TradePerks (the "tradeperks" save section). The once-a-week change clock is
//! session state: a restart lets a character change again (follow-up: save it with the section).
class LF_PerkPick
{
	static const string FEATURE = "PerkPick";
	static const int CHANGE_FEE = 500; // lf-fixed: decision N10, "$500, once an in-game week"
	static const int WEEK_DAYS = 7; // lf-fixed: one in-game week between two paid changes
	static const int LEVEL_FIRST = 10; // lf-fixed: the work plan's first pick level
	static const int LEVEL_SECOND = 20; // lf-fixed: the work plan's second pick level
	static const string REASON = "perk_change";

	//! "<uid>|<trade>|<atLevel>" -> in-game day number of the last paid change. Session only.
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
	//! The work trades whose gathers the passive perks change, in the order the counter serves them.
	static void Trades(notnull array<string> into)
	{
		into.Clear();
		into.Insert("MINER");
		into.Insert("LUMBERJACK");
		into.Insert("FARMER");
		into.Insert("FISHER");
		into.Insert("SMELTING");
		into.Insert("SALVAGE");
	}

	//------------------------------------------------------------------------------------------------
	//! The perks a pick level draws from, before the ones held are taken out.
	static void Pool(int atLevel, notnull array<string> into)
	{
		into.Clear();
		if (atLevel == LEVEL_FIRST)
		{
			into.Insert(LF_PerkEffects.PERK_DOUBLE_YIELD);
			into.Insert(LF_PerkEffects.PERK_SWIFT_HANDS);
			return;
		}

		if (atLevel == LEVEL_SECOND)
		{
			into.Insert(LF_PerkEffects.PERK_LIGHT_TOUCH);
			into.Insert(LF_PerkEffects.PERK_DOUBLE_YIELD);
			into.Insert(LF_PerkEffects.PERK_SWIFT_HANDS);
		}
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
		Pool(atLevel, pool);
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
		into.Clear();
		if (uid.IsEmpty())
			return;

		array<string> trades = {};
		Trades(trades);
		foreach (string trade : trades)
		{
			OffersFrom(trade, LF_JobLevels.LevelOf(uid, trade), LF_TradePerks.PerkAt(uid, trade, LEVEL_FIRST), LF_TradePerks.PerkAt(uid, trade, LEVEL_SECOND), into);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the offers read from the last F2 sheet the server sent (levels and perks in pick-level
	//! order). False when the client has no sheet yet.
	static bool OffersFromSheet(LF_PlayerMenuData data, notnull array<ref LF_PerkPickOffer> into)
	{
		into.Clear();
		if (!data)
			return false;

		array<string> trades = {};
		Trades(trades);
		foreach (string trade : trades)
		{
			foreach (LF_PlayerMenuSkill skill : data.m_aSkills)
			{
				if (!skill || skill.m_sJob != trade)
					continue;

				string first;
				string second;
				if (skill.m_aPerks.Count() > 0)
					first = skill.m_aPerks[0];
				if (skill.m_aPerks.Count() > 1)
					second = skill.m_aPerks[1];

				OffersFrom(trade, skill.m_iLevel, first, second, into);
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The first offer of a perk at a pick level of one kind (a free take or a paid change), trades in
	//! counter order; null when none.
	static LF_PerkPickOffer Find(notnull array<ref LF_PerkPickOffer> offers, int atLevel, string perk, bool change)
	{
		foreach (LF_PerkPickOffer offer : offers)
		{
			if (offer.m_iAtLevel == atLevel && offer.m_sPerk == perk && offer.m_bChange == change)
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

		int left = last + WEEK_DAYS - Today();
		if (left < 0)
			return 0;

		return left;
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
	//! so a row the client labelled from a stale sheet is refused before any cash moves.
	static LF_EPerkPickResult Take(IEntity user, int atLevel, string perk, int fee, bool change)
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
		LF_PerkPickOffer offer = Find(offers, atLevel, perk, change);
		if (!offer)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 refused %2 at level %3: not offered", uid, perk, atLevel));
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

		if (!s_mChangedDay)
			s_mChangedDay = new map<string, int>();
		s_mChangedDay.Set(Key(uid, offer.m_sTrade, atLevel), Today());
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
