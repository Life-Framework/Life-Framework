//------------------------------------------------------------------------------------------------
//! Medic outcome bonuses (#4210): a revive pays $10 and a delivered patient $15 into the medic's pending
//! duty bonus (LF_DutyBonus), capped per hour by rank and paid with the shift at clock-out.
//! Server only; Install subscribes to the revive and bed-delivery events.
class LF_MedicDutyBonus
{
	static const string FEATURE = "MedicDutyBonus";

	protected static ref LF_MedicDutyBonusSubscriber s_Subscriber;
	//! The award just before this one when it was a revive: a bed delivery's own revive.
	protected static string s_sLastReviveMedic;
	protected static string s_sLastRevivePatient;
	protected static int s_iLastReviveAdded;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_sLastReviveMedic = "";
		s_sLastRevivePatient = "";
		s_iLastReviveAdded = 0;
		if (s_Subscriber)
		{
			LF_EventBus.Unsubscribe(LF_OnPlayerRevived, s_Subscriber);
			LF_EventBus.Unsubscribe(LF_OnPatientDelivered, s_Subscriber);
		}

		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a medic rank's hourly bonus cap (orderly $40, EMT $50, paramedic $50, doctor $40). Ranks past
	//! the table keep the last row.
	static int Cap(int rank)
	{
		array<int> caps = {40, 50, 50, 40};	// lf-fixed: plan "The economy table", mirrored by tools/lib/economy-lumber.mjs bonusCaps
		if (rank < 0)
			return 0;

		if (rank >= caps.Count())
			return caps[caps.Count() - 1];

		return caps[rank];
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listens for revives and delivered patients. Safe to call twice.
	static bool Install()
	{
		if (!Replication.IsServer() && Replication.IsRunning())
			return false;

		if (s_Subscriber && LF_EventBus.IsSubscribed(LF_OnPlayerRevived, s_Subscriber))
			return true;

		s_Subscriber = new LF_MedicDutyBonusSubscriber();
		bool revives = LF_EventBus.Subscribe(LF_OnPlayerRevived, s_Subscriber);
		bool deliveries = LF_EventBus.Subscribe(LF_OnPatientDelivered, s_Subscriber);
		return revives && deliveries;
	}

	//------------------------------------------------------------------------------------------------
	//! Awards the medic named by an event. A medic reviving or delivering themselves earns nothing.
	//! \return The amount added to the pending bonus.
	static int AwardTo(string medicUid, string patientUid, LF_DutyBonusKind kind)
	{
		if (medicUid.IsEmpty())
			return 0;

		if (medicUid == patientUid)
		{
			LF_Debug.Info(FEATURE, string.Format("no bonus for %1: the patient is the medic", medicUid));
			return 0;
		}

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
		{
			LF_Debug.Error(FEATURE, "medic bonus refused: no account manager");
			return 0;
		}

		LF_PlayerAccount account = accounts.GetAccount(medicUid);
		if (!account)
		{
			LF_Debug.Warn(FEATURE, string.Format("medic bonus refused: no account for %1", medicUid));
			return 0;
		}

		// A bed delivery publishes the patient's revive and then the delivery, in one call: the delivery's $15
		// replaces that revive's $10 instead of adding to it.
		if (kind == LF_DutyBonusKind.PATIENT_DELIVERED && s_sLastReviveMedic == medicUid && s_sLastRevivePatient == patientUid)
			LF_DutyBonus.Refund(account, s_iLastReviveAdded);

		s_sLastReviveMedic = "";
		s_sLastRevivePatient = "";
		s_iLastReviveAdded = 0;
		int added = LF_DutyBonus.Award(account, kind);
		if (kind == LF_DutyBonusKind.REVIVE)
		{
			s_sLastReviveMedic = medicUid;
			s_sLastRevivePatient = patientUid;
			s_iLastReviveAdded = added;
		}

		return added;
	}
}

//------------------------------------------------------------------------------------------------
class LF_MedicDutyBonusSubscriber : LF_EventSubscriber
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "MedicDutyBonus";
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPlayerRevived revived = LF_OnPlayerRevived.Cast(busEvent);
		if (revived)
		{
			LF_MedicDutyBonus.AwardTo(revived.m_sMedicUid, revived.m_sPlayerUid, LF_DutyBonusKind.REVIVE);
			return true;
		}

		LF_OnPatientDelivered delivered = LF_OnPatientDelivered.Cast(busEvent);
		if (delivered)
		{
			LF_MedicDutyBonus.AwardTo(delivered.m_sMedicUid, delivered.m_sPlayerUid, LF_DutyBonusKind.PATIENT_DELIVERED);
			return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MedicDutyBonus's statics (LF_StaticReset finds it by reflection).
class LF_MedicDutyBonus_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MedicDutyBonus";
	}

	override void Apply()
	{
		LF_MedicDutyBonus.ResetStatics();
	}
}
