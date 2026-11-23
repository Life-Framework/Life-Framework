//------------------------------------------------------------------------------------------------
//! The medic skill (PO-S5, #4433, 0.3.0 plan section 2): MEDIC XP from outcomes, three small perks.
//! XP comes only from what a medic finishes, never from time on duty (shift pay already pays for
//! time): a revive and a delivered patient (both paid in LF_JobLevelSources, which asks `MayAward`
//! here) and first aid on a bleeding player (`medic_first_aid`, paid here from LF_OnFirstAid).
//! Clocking in, standing by and a bandage that bought no time pay nothing. The amounts and cooldowns
//! are `jobs.xpAwards` rows in `LF_Jobs.conf`; the track and its curve are the same as every trade
//! (`LF_JobLevels`). Server only.
//!
//! Self-award holes are closed in one place, `MayAward`: a medic reviving, carrying or bandaging
//! themselves earns nothing, and neither does one working on another character of their own account
//! (an alt), the same account rule as the duty bonus (#4319).
//!
//! The perks are read through `LF_TradePerks.Has(uid, "MEDIC", perk)`: a quicker revive hold, a
//! quicker first aid (each dressing buys more bleed-out time) and a slower bleed-out while the
//! medic's patient is carried (seated in a vehicle the medic loaded). They change a feel, never
//! pay: medics stay on the mid-tier rung and no row here touches money.
//------------------------------------------------------------------------------------------------
class LF_MedicSkill : LF_EventSubscriber
{
	static const string FEATURE = "MedicSkill";
	static const string JOB = "MEDIC";

	static const string ACTION_FIRST_AID = "medic_first_aid";

	static const string PERK_QUICK_REVIVE = "quick_revive";
	static const string PERK_QUICK_AID = "quick_aid";
	static const string PERK_STEADY_CARRY = "steady_carry";

	static const int REVIVE_PERCENT = 70;	// lf-fixed: a perked revive hold takes 70% of the base time
	static const int AID_PERCENT = 130;	// lf-fixed: a perked dressing buys 130% of the base bleed-out seconds
	static const int CARRY_BLEED_PERCENT = 60;	// lf-fixed: a carried patient of a perked medic bleeds at 60% speed

	protected static ref LF_MedicSkill s_Subscriber;
	//! Server: patient uid to the uid of the medic who loaded them (read while the patient is seated).
	protected static ref map<string, string> s_mCarrier;

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes once (game start). A bus reset drops the subscription; the next call restores it.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (!s_Subscriber)
			s_Subscriber = new LF_MedicSkill();

		bool ok = SubscribeTo(LF_OnFirstAid);
		ok = SubscribeTo(LF_OnPlayerCarried) && ok;
		ok = SubscribeTo(LF_OnPlayerRevived) && ok;
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
			LF_EventBus.Unsubscribe(LF_OnFirstAid, s_Subscriber);
			LF_EventBus.Unsubscribe(LF_OnPlayerCarried, s_Subscriber);
			LF_EventBus.Unsubscribe(LF_OnPlayerRevived, s_Subscriber);
		}

		s_Subscriber = null;
		s_mCarrier = null;
	}

	//------------------------------------------------------------------------------------------------
	//! One rule for every MEDIC award: a real medic, working on another player of another account.
	//! A missing medic (an NPC), a self-revive and an alt on the same account earn nothing.
	static bool MayAward(string medicUid, string patientUid)
	{
		if (medicUid.IsEmpty())
			return false;

		return !LF_DutyBonus.SameAccount(medicUid, patientUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a medic's first aid bought a bleeding player this many seconds. Only an on-duty medic
	//! earns, only for time actually added. \return XP actually added.
	static int OnFirstAid(string medicUid, string patientUid, float seconds, bool medicOnDuty)
	{
		if (!medicOnDuty || seconds <= 0 || !MayAward(medicUid, patientUid))
			return 0;

		return LF_JobLevels.Award(medicUid, ACTION_FIRST_AID);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the medic picked this MEDIC perk.
	static bool Has(string medicUid, string perk)
	{
		if (medicUid.IsEmpty())
			return false;

		return LF_TradePerks.Has(medicUid, JOB, perk);
	}

	//------------------------------------------------------------------------------------------------
	//! The revive hold the server's rule asks for: the base time, shorter with the quick-revive perk.
	static float ReviveSeconds(string medicUid, float baseSeconds)
	{
		if (Has(medicUid, PERK_QUICK_REVIVE))
			return baseSeconds * REVIVE_PERCENT / 100;

		return baseSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! The revive hold bar's speed (1 with no perk): the engine adds the time slice times this to the
	//! progress, so the bar fills in `ReviveSeconds`. A remote client reads its own perks from the
	//! owner sync (LF_PerkSync, #4525).
	static float ReviveSpeed(string medicUid)
	{
		if (Has(medicUid, PERK_QUICK_REVIVE))
			return 100.0 / REVIVE_PERCENT;

		return 1.0;
	}

	//------------------------------------------------------------------------------------------------
	//! Bleed-out seconds one dressing buys: the base, more with the quick-aid perk. The cap on the
	//! total stays LF_FirstAid's own.
	static float AidSeconds(string medicUid, float baseSeconds)
	{
		if (Has(medicUid, PERK_QUICK_AID))
			return baseSeconds * AID_PERCENT / 100;

		return baseSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! How fast a patient bleeds (1 with no perk): slower while carried by a medic with the
	//! steady-carry perk. `carried` is the patient sitting in a vehicle; `carrierUid` the medic who
	//! loaded them.
	static float BleedScale(string carrierUid, bool carried)
	{
		if (carried && Has(carrierUid, PERK_STEADY_CARRY))
			return CARRY_BLEED_PERCENT / 100.0;

		return 1.0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the bleed scale for one patient, read by LF_BleedOut.Advance on each tick.
	static float BleedScaleFor(IEntity patient)
	{
		if (!s_mCarrier || s_mCarrier.IsEmpty() || !patient)
			return 1.0;

		string carrierUid;
		if (!s_mCarrier.Find(LF_Utils.GetPlayerUID(patient), carrierUid))
			return 1.0;

		return BleedScale(carrierUid, LF_Carry.IsSeatedIn(patient));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: remember who loaded a patient (and forget it again on a revive).
	static void NoteCarried(string patientUid, string carrierUid)
	{
		if (patientUid.IsEmpty())
			return;

		if (!s_mCarrier)
			s_mCarrier = new map<string, string>();

		if (carrierUid.IsEmpty())
			s_mCarrier.Remove(patientUid);
		else
			s_mCarrier.Set(patientUid, carrierUid);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsOnDutyMedic(string uid)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts || uid.IsEmpty())
			return false;

		return LF_MedicDuty.IsOnDuty(accounts.GetAccount(uid));
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnFirstAid aid = LF_OnFirstAid.Cast(busEvent);
		if (aid)
		{
			OnFirstAid(aid.m_sHelperUid, aid.m_sPlayerUid, aid.m_fSeconds, IsOnDutyMedic(aid.m_sHelperUid));
			return true;
		}

		LF_OnPlayerCarried carried = LF_OnPlayerCarried.Cast(busEvent);
		if (carried)
		{
			NoteCarried(carried.m_sPlayerUid, carried.m_sCarrierUid);
			return true;
		}

		LF_OnPlayerRevived revived = LF_OnPlayerRevived.Cast(busEvent);
		if (revived)
		{
			NoteCarried(revived.m_sPlayerUid, string.Empty);
			return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MedicSkill's statics (LF_StaticReset finds it by reflection).
class LF_MedicSkill_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MedicSkill";
	}

	override void Apply()
	{
		LF_MedicSkill.ResetStatics();
	}
}
