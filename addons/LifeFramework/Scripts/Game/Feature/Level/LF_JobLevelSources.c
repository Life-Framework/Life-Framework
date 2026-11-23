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

	//! SMELTING and SALVAGE are trades of their own (#4184, decision N3): a furnace batch never feeds MINER.
	static const string JOB_SMELTING = "SMELTING";
	static const string JOB_SALVAGE = "SALVAGE";
	static const string ACTION_SMELT_BATCH = "smelt_batch";
	static const string ACTION_SALVAGE_WORK = "salvage_work";

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
		if (!LF_MedicSkill.MayAward(medicUid, patientUid))
			return 0;

		return LF_JobLevels.Award(medicUid, action);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: pays one trade action, but only when its `jobs.xpAwards` row feeds that trade. A row that
	//! points at another track (a batch that pays MINER) pays nothing and logs an error.
	//! \return XP actually added.
	protected static int AwardTrade(string playerUid, string action, string trade, int units)
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		LF_JobXpAwardDef row;
		if (config)
			row = config.FindAward(action);

		if (row && row.m_sJob != trade)
		{
			LF_Debug.Error(FEATURE, string.Format("xp refused: action '%1' feeds '%2' in the jobs config, it must feed %3", action, row.m_sJob, trade));
			return 0;
		}

		return LF_JobLevels.Award(playerUid, action, units);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a finished smelter batch pays SMELTING xp, never MINER xp.
	//! \param[in] batches Batches finished; the row's xp is per batch.
	//! \return XP actually added.
	static int AwardSmelting(string playerUid, int batches = 1)
	{
		return AwardTrade(playerUid, ACTION_SMELT_BATCH, JOB_SMELTING, batches);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one unit of salvage work pays SALVAGE xp.
	//! \return XP actually added.
	static int AwardSalvage(string playerUid, int units = 1)
	{
		return AwardTrade(playerUid, ACTION_SALVAGE_WORK, JOB_SALVAGE, units);
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
