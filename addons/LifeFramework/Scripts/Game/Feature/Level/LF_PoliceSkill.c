//------------------------------------------------------------------------------------------------
//! The police skill (PO-R8, #4424, 0.3.0 plan section 2): POLICE XP from outcomes, two small perks.
//! XP comes only from what an officer finishes, never from time on duty (shift pay already pays for
//! time): a call closed as resolved (`police_call_resolved`), a booking at the desk (`police_booking`)
//! and a recovered stolen car (`police_recovery`). An unfounded close, no one found and clocking in
//! pay nothing. The amounts and cooldowns are `jobs.xpAwards` rows in `LF_Jobs.conf`; the track and
//! its curve are the same as every trade (`LF_JobLevels`). Server only.
//!
//! The perks are read through `LF_TradePerks.Has(uid, "POLICE", perk)`: a faster cuff (the hold the
//! server asks for) and a longer radar range. They change a feel, never pay:
//! police stay the lowest paid trade and no row here touches money.
//------------------------------------------------------------------------------------------------
class LF_PoliceSkill : LF_EventSubscriber
{
	static const string FEATURE = "PoliceSkill";
	static const string JOB = "POLICE";

	static const string ACTION_CALL_RESOLVED = "police_call_resolved";
	static const string ACTION_BOOKING = "police_booking";
	static const string ACTION_RECOVERY = "police_recovery";

	static const string PERK_FAST_CUFF = "fast_cuff";
	static const string PERK_LONG_RADAR = "long_radar";

	static const int CUFF_PERCENT = 70;	// lf-fixed: a perked cuff hold asks for 70% of the base time
	static const int RADAR_PERCENT = 150;	// lf-fixed: a perked radar gun reaches 150% of the base range

	protected static ref LF_PoliceSkill s_Subscriber;

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes once (game start). A bus reset drops the subscription; the next call restores it.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (!s_Subscriber)
			s_Subscriber = new LF_PoliceSkill();

		if (LF_EventBus.IsSubscribed(LF_OnVehicleRecovered, s_Subscriber))
			return true;

		return LF_EventBus.Subscribe(LF_OnVehicleRecovered, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset).
	static void ResetStatics()
	{
		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnVehicleRecovered, s_Subscriber);

		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an officer's call closed with this outcome. Only RESOLVED pays; no one found and
	//! unfounded earn nothing. \return XP actually added.
	static int OnCallClosed(string officerUid, LF_CallOutcomeKind outcome)
	{
		if (outcome != LF_CallOutcomeKind.RESOLVED)
			return 0;

		return LF_JobLevels.Award(officerUid, ACTION_CALL_RESOLVED);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an officer booked a suspect at the desk. \return XP actually added.
	static int OnBooked(string officerUid)
	{
		return LF_JobLevels.Award(officerUid, ACTION_BOOKING);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an officer recovered a stolen car. \return XP actually added.
	static int OnRecovered(string officerUid)
	{
		return LF_JobLevels.Award(officerUid, ACTION_RECOVERY);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the officer picked this POLICE perk.
	static bool Has(string officerUid, string perk)
	{
		if (officerUid.IsEmpty())
			return false;

		return LF_TradePerks.Has(officerUid, JOB, perk);
	}

	//------------------------------------------------------------------------------------------------
	//! The cuff hold the server asks for: the base time, shorter with the fast-cuff perk.
	static float CuffSeconds(string officerUid, float baseSeconds)
	{
		if (Has(officerUid, PERK_FAST_CUFF))
			return baseSeconds * CUFF_PERCENT / 100;

		return baseSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! The cuff hold bar's speed (1 with no perk): the engine adds the time slice times this to the
	//! progress, so the bar fills in `CuffSeconds`. A remote client reads its own perks from the owner
	//! sync (LF_PerkSync, #4525).
	static float CuffSpeed(string officerUid)
	{
		if (Has(officerUid, PERK_FAST_CUFF))
			return 100.0 / CUFF_PERCENT;

		return 1.0;
	}

	//------------------------------------------------------------------------------------------------
	//! The radar gun's reach in metres: the base range, longer with the long-radar perk.
	static float RadarRange(string officerUid, float baseMeters)
	{
		if (Has(officerUid, PERK_LONG_RADAR))
			return baseMeters * RADAR_PERCENT / 100;

		return baseMeters;
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnVehicleRecovered recovered = LF_OnVehicleRecovered.Cast(busEvent);
		if (!recovered)
			return false;

		// An officer recovering a car their own account owns earns nothing (same rule as the duty bonus).
		if (!LF_DutyBonus.SameAccount(recovered.m_sOfficerUid, recovered.m_sOwnerUid))
			OnRecovered(recovered.m_sOfficerUid);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PoliceSkill's statics (LF_StaticReset finds it by reflection).
class LF_PoliceSkill_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PoliceSkill";
	}

	override void Apply()
	{
		LF_PoliceSkill.ResetStatics();
	}
}
