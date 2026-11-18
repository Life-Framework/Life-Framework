//------------------------------------------------------------------------------------------------
//! XP sources for MINER, LUMBERJACK and MEDIC (#2707, Level Feature.md "V3"). One bus subscriber turns
//! three events into `jobs.xpAwards` rows, so the gather, revive and bed code never call the level store:
//! - LF_OnGathered of family MINING pays `gather_mining` (MINER); family LOGGING pays `gather_logging`
//!   (LUMBERJACK). Other families (DIGGING, FORAGING) pay nothing here.
//! - LF_OnPlayerRevived pays `medic_revive` and LF_OnPatientDelivered pays `medic_delivery`, both to
//!   the medic (MEDIC). A medic who revives themselves earns nothing.
//! The action rows own the XP, the job and the cooldown; the track owns the daily cap. LF_JobLevels.Award
//! applies both, so a repeat inside the cooldown or past the cap earns nothing. Server only.
//------------------------------------------------------------------------------------------------
class LF_JobLevelSources : LF_EventSubscriber
{
	static const string FEATURE = "JobLevels";

	static const string FAMILY_MINING = "MINING";
	static const string FAMILY_LOGGING = "LOGGING";

	static const string ACTION_GATHER_MINING = "gather_mining";
	static const string ACTION_GATHER_LOGGING = "gather_logging";
	static const string ACTION_MEDIC_REVIVE = "medic_revive";
	static const string ACTION_MEDIC_DELIVERY = "medic_delivery";

	protected static ref LF_JobLevelSources s_Subscriber;

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes once (game start). A bus reset drops the subscription; the next call restores it.
	//! \return True when every event is subscribed after the call.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (!s_Subscriber)
			s_Subscriber = new LF_JobLevelSources();

		bool ok = SubscribeTo(LF_OnGathered);
		ok = SubscribeTo(LF_OnPlayerRevived) && ok;
		ok = SubscribeTo(LF_OnPatientDelivered) && ok;
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool SubscribeTo(typename eventType)
	{
		if (LF_EventBus.IsSubscribed(eventType, s_Subscriber))
			return true;

		return LF_EventBus.Subscribe(eventType, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset).
	static void ResetStatics()
	{
		if (s_Subscriber)
		{
			LF_EventBus.Unsubscribe(LF_OnGathered, s_Subscriber);
			LF_EventBus.Unsubscribe(LF_OnPlayerRevived, s_Subscriber);
			LF_EventBus.Unsubscribe(LF_OnPatientDelivered, s_Subscriber);
		}

		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The xpAwards action a gather family pays, or "" when it pays none.
	static string ActionForFamily(string family)
	{
		if (family == FAMILY_MINING)
			return ACTION_GATHER_MINING;

		if (family == FAMILY_LOGGING)
			return ACTION_GATHER_LOGGING;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Awards a medic one action; a missing medic (an NPC) or a self-revive earns nothing.
	//! \return XP actually added.
	static int AwardMedic(string medicUid, string patientUid, string action)
	{
		if (medicUid.IsEmpty() || medicUid == patientUid)
			return 0;

		return LF_JobLevels.Award(medicUid, action);
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnGathered gathered = LF_OnGathered.Cast(busEvent);
		if (gathered)
		{
			string action = ActionForFamily(gathered.m_sJobFamily);
			if (!action.IsEmpty() && gathered.m_iAmount > 0)
				LF_JobLevels.Award(gathered.m_sPlayerUid, action);

			return true;
		}

		LF_OnPlayerRevived revived = LF_OnPlayerRevived.Cast(busEvent);
		if (revived)
		{
			AwardMedic(revived.m_sMedicUid, revived.m_sPlayerUid, ACTION_MEDIC_REVIVE);
			return true;
		}

		LF_OnPatientDelivered delivered = LF_OnPatientDelivered.Cast(busEvent);
		if (delivered)
		{
			AwardMedic(delivered.m_sMedicUid, delivered.m_sPlayerUid, ACTION_MEDIC_DELIVERY);
			return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_JobLevelSources's statics (LF_StaticReset finds it by reflection).
class LF_JobLevelSources_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_JobLevelSources";
	}

	override void Apply()
	{
		LF_JobLevelSources.ResetStatics();
	}
}
