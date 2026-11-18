//------------------------------------------------------------------------------------------------
//! The market stall prop on the square (#2925, framework #786). A player in reach rents it for the pitch
//! fee in carried cash on market day; the stall then shows the holder's name to everyone. The rules are
//! `LF_StallRules` (#2924); this component is the world side: the clock, the cash, one stall per player
//! and the replicated name.
//!
//! RENT. Server only (`Rent`): the market must be open now (config day and hours on the in-game clock), the
//! stall free, the renter without another stall for this market day, and the renter must carry the fee. The
//! rules answer first; the fee is taken only after an OK and given back if the stacks moved under us. A
//! refusal moves no cash. One market day, one pitch: the rent ends at the closing hour (`Lapse`, called on
//! every rent and by a slow server timer), and a stall rented for another day is free again.
//!
//! REPLICATION. Two RplProps per stall, set by the server and bumped on change: whether it is rented and the
//! holder's display name (a short string). The holder's key (uid) never leaves the server. A client reads
//! only those two to show the prompt; a late joiner gets them with the entity snapshot.
//!
//! NOT SAVED. The rent lives in server memory: a restart before closing hour frees the stall and the fee is
//! not refunded. A save section is a later card.
//------------------------------------------------------------------------------------------------
//! The in-game day and hour the stalls use. The day is the whole in-game days since 1970-01-01 (the
//! rules take `day % 7` as the week day); the hour is 0..23.
class LF_StallClock
{
	//! Tests: the day and hour to use instead of the clock; -1 reads the clock.
	protected static int s_iDayOverride = -1;
	protected static int s_iHourOverride = -1;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iDayOverride = -1;
		s_iHourOverride = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: pin the clock; a day of -1 releases it.
	static void SetForTest(int day, int hour)
	{
		s_iDayOverride = day;
		s_iHourOverride = hour;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true and the in-game day and hour now; false (logged) when the world has no clock.
	static bool Now(out int day, out int hour)
	{
		day = 0;
		hour = 0;
		if (s_iDayOverride >= 0)
		{
			day = s_iDayOverride;
			hour = s_iHourOverride;
			return true;
		}

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		TimeAndWeatherManagerEntity clock;
		if (world)
			clock = world.GetTimeAndWeatherManager();

		if (!clock)
		{
			LF_Debug.Error(LF_StallConfig.FEATURE, "stall clock: the world has no TimeAndWeatherManagerEntity; stalls stay closed");
			return false;
		}

		day = LF_LicenceDesk.DaysFromCivil(clock.GetYear(), clock.GetMonth(), clock.GetDay());
		hour = LF_Utils.ClampInt((int)clock.GetTimeOfTheDay(), 0, 23);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_StallClock's statics.
class LF_StallClock_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_StallClock";
	}

	override void Apply()
	{
		LF_StallClock.ResetStatics();
		LF_StallComponent.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Market", description: "A market stall a player rents for the pitch fee on market day")]
class LF_StallComponentClass : ScriptComponentClass
{
}

class LF_StallComponent : ScriptComponent
{
	//! Metres from the renter's character to the stall.
	static const float REACH = 5.0; // lf-fixed: interaction reach, the same 5 m as the plot sign
	//! The ledger reason of a pitch fee.
	static const string REASON_RENT = "stall_rent";
	//! Longest holder name that is replicated.
	static const int MAX_NAME = 32; // lf-fixed: a short string on the wire
	//! Milliseconds between server checks that end a lapsed rent.
	static const int LAPSE_CHECK_MS = 30000; // lf-fixed: how often the server frees a stall past closing

	//! Whether a holder has the pitch now. Set by the server; clients read the replicated copy.
	[RplProp()]
	protected bool m_bRented;

	//! The holder's display name, empty while unrented or when the holder has no name. Set by the server.
	[RplProp()]
	protected string m_sHolderName;

	//! Server: the rules' state for this stall; the holder key is the renter's uid.
	protected ref LF_Stall m_Stall;

	//! Live stalls; the component's own OnDelete removes it.
	protected static ref array<LF_StallComponent> s_aLive;

	//! Tests: the holder name to show instead of the renter's (a fixture character has none).
	protected static string s_sNameForTest;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_sNameForTest = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: the next rents show this name; empty releases it.
	static void SetNameForTest(string name)
	{
		s_sNameForTest = name;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!s_aLive)
			s_aLive = new array<LF_StallComponent>();

		s_aLive.Insert(this);
		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(LapseTick, LAPSE_CHECK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aLive)
			s_aLive.RemoveItem(this);

		GetGame().GetCallqueue().Remove(LapseTick);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! \return true while a holder has the pitch. On a client this is the replicated copy.
	bool IsRented()
	{
		return m_bRented;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The holder's display name; empty while unrented or when the holder has no name.
	string GetHolderName()
	{
		return m_sHolderName;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: \return The holder's key, empty while unrented.
	string GetHolderKey()
	{
		if (!m_Stall || !m_bRented)
			return "";

		return m_Stall.m_sHolder;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the rules' stall (listings, earnings) while rented, else null. LF_StallTrade works on it (#2926).
	LF_Stall GetStall()
	{
		if (!m_bRented)
			return null;

		return m_Stall;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: ends the rent when the market day it was rented for is over (another day, or the closing
	//! hour has passed). Does nothing on a client or while the pitch is still good.
	//! \return true when a rent ended
	bool Lapse()
	{
		if (!Replication.IsServer() || !m_bRented || !m_Stall)
			return false;

		int day;
		int hour;
		if (!LF_StallClock.Now(day, hour))
			return false;

		LF_StallConfig config = LF_StallConfig.Get();
		if (!config)
			return false;

		if (m_Stall.m_iRentedDay == day && hour < config.m_iCloseHour)
			return false;

		LF_Debug.Info(LF_StallConfig.FEATURE, string.Format("stall of %1 lapsed: rented for day %2, now day %3 hour %4", m_sHolderName, m_Stall.m_iRentedDay, day, hour));
		m_Stall.m_iRentedDay = LF_Stall.NOT_RENTED;
		SetShown(false, "");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void LapseTick()
	{
		Lapse();
	}

	//------------------------------------------------------------------------------------------------
	protected void SetShown(bool rented, string holderName)
	{
		m_bRented = rented;
		m_sHolderName = holderName;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when a live stall other than this one is rented to `uid` for market day `day`.
	protected bool HoldsAnother(string uid, int day)
	{
		if (!s_aLive)
			return false;

		foreach (LF_StallComponent other : s_aLive)
		{
			if (other && other != this && other.m_bRented && other.m_Stall && other.m_Stall.m_sHolder == uid && other.m_Stall.m_iRentedDay == day)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `renter` rents this stall's pitch for today's market with carried cash.
	//! \param renter The caller's character; it must be within REACH.
	//! \param uid The renter's character key (LF_Utils.GetPlayerUid); the holder of record.
	//! \return OK, or why nothing changed (no cash moved for any refusal).
	LF_EStallResult Rent(IEntity renter, string uid)
	{
		if (!Replication.IsServer() || !renter || uid.IsEmpty())
		{
			LF_Debug.Warn(LF_StallConfig.FEATURE, "stall rent refused: not on the server, or no renter or key");
			return LF_EStallResult.BAD_INPUT;
		}

		IEntity owner = GetOwner();
		if (!owner || vector.Distance(renter.GetOrigin(), owner.GetOrigin()) > REACH)
		{
			LF_Debug.Warn(LF_StallConfig.FEATURE, string.Format("stall rent refused for %1: out of reach of the stall", uid));
			return LF_EStallResult.BAD_INPUT;
		}

		LF_StallConfig config = LF_StallConfig.Get();
		int day;
		int hour;
		if (!config || !LF_StallClock.Now(day, hour))
			return LF_EStallResult.CLOSED;

		Lapse();
		if (m_bRented)
		{
			LF_Debug.Info(LF_StallConfig.FEATURE, string.Format("stall rent refused for %1: the stall is rented to %2", uid, m_sHolderName));
			return LF_EStallResult.ALREADY_RENTED;
		}

		if (HoldsAnother(uid, day))
		{
			LF_Debug.Info(LF_StallConfig.FEATURE, string.Format("stall rent refused for %1: one stall per player for day %2", uid, day));
			return LF_EStallResult.ALREADY_RENTED;
		}

		// The rules decide on a fresh stall; the real one changes only after the fee is taken.
		LF_Stall candidate = LF_Stall.Make(uid);
		LF_EStallResult result = LF_StallRules.Rent(config, candidate, day, hour, LF_MoneyUtils.GetCash(renter));
		if (result != LF_EStallResult.OK)
			return result;

		int fee = config.m_iPitchFee;
		int taken = LF_MoneyUtils.RemoveAmount(renter, fee, REASON_RENT);
		if (taken != fee)
		{
			// The stacks moved under us: give back what was taken, rent nothing.
			if (taken > 0)
				LF_MoneyUtils.AddCash(renter, taken, REASON_RENT, "", true);

			LF_Debug.Error(LF_StallConfig.FEATURE, string.Format("stall rent refused for %1: took %2 of %3, refunded", uid, taken, fee));
			return LF_EStallResult.CANT_AFFORD;
		}

		m_Stall = candidate;
		string name = LF_Utils.GetCharacterName(renter);
		if (!s_sNameForTest.IsEmpty())
			name = s_sNameForTest;

		if (name.Length() > MAX_NAME)
			name = name.Substring(0, MAX_NAME);

		SetShown(true, name);
		LF_Debug.Info(LF_StallConfig.FEATURE, string.Format("stall rented by %1 ('%2') for day %3, fee %4", uid, name, day, fee));
		return LF_EStallResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tests and tools: frees the stall at once with no refund.
	void Clear()
	{
		if (m_Stall)
			m_Stall.m_iRentedDay = LF_Stall.NOT_RENTED;

		if (Replication.IsServer())
			SetShown(false, "");
	}
}
