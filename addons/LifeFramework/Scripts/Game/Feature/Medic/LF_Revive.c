//------------------------------------------------------------------------------------------------
enum LF_EReviveRefusal
{
	NONE,
	NO_PATIENT,
	SELF,
	NOT_DOWNED,
	NOT_MEDIC,
	TOO_FAR,
	NO_ITEM
}

//------------------------------------------------------------------------------------------------
//! A medic on duty revives a downed patient (#259). The server decides: the medic is clocked in as
//! a medic (LF_MedicDuty), the patient is downed, and they stand within `m_fReviveRange`. A revive
//! brings the patient back through the downed state's own path (LF_Downed.Clear: the flag, the
//! bleed-out clock and the unconsciousness), leaves them their inventory and the sliver of health
//! they were downed with, and publishes LF_OnPlayerRevived.
class LF_Revive
{
	static const string FEATURE = "Medic";

	protected static int s_iRevivedCount;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iRevivedCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the one rule the offer and the server share.
	static LF_EReviveRefusal Decide(bool hasPatient, bool isSelf, bool patientDowned, bool medicOnDuty, float distance, float range)
	{
		if (!hasPatient)
			return LF_EReviveRefusal.NO_PATIENT;

		if (isSelf)
			return LF_EReviveRefusal.SELF;

		if (!patientDowned)
			return LF_EReviveRefusal.NOT_DOWNED;

		if (!medicOnDuty)
			return LF_EReviveRefusal.NOT_MEDIC;

		if (distance > range)
			return LF_EReviveRefusal.TOO_FAR;

		return LF_EReviveRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_EReviveRefusal refusal)
	{
		if (refusal == LF_EReviveRefusal.NOT_MEDIC)
			return "#LF-Revive_NotMedic";

		if (refusal == LF_EReviveRefusal.TOO_FAR)
			return "#LF-Revive_TooFar";

		if (refusal == LF_EReviveRefusal.NO_ITEM)
			return "#LF-Revive_NoItem";

		return "#LF-Revive_NotDowned";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure on the entities: the refusal for this pair, with the duty flag passed in.
	static LF_EReviveRefusal Check(IEntity medic, IEntity patient, bool medicOnDuty)
	{
		if (!medic || !patient)
			return Decide(false, false, false, medicOnDuty, 0, 0);

		return Decide(true, medic == patient, LF_Downed.IsDowned(patient), medicOnDuty, vector.Distance(medic.GetOrigin(), patient.GetOrigin()), LF_MedicConfig.Get().m_fReviveRange);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: revive `patient` for `medic`, whose duty state the caller resolved.
	//! \return NONE when the patient was revived, else why not.
	static LF_EReviveRefusal Apply(IEntity medic, IEntity patient, bool medicOnDuty)
	{
		string medicUid = Uid(medic);
		string patientUid = Uid(patient);
		if (!Replication.IsServer())
			return LF_EReviveRefusal.NO_PATIENT;

		LF_EReviveRefusal refusal = Check(medic, patient, medicOnDuty);
		if (refusal != LF_EReviveRefusal.NONE)
		{
			LF_Debug.Info(FEATURE, string.Format("revive of %1 by %2 refused: %3", patientUid, medicUid, typename.EnumToString(LF_EReviveRefusal, refusal)));
			LF_Utils.NotifyEntity(medic, RefusalKey(refusal), "", 4.0);
			return refusal;
		}

		if (!LF_Downed.Clear(patient))
			return LF_EReviveRefusal.NOT_DOWNED;

		s_iRevivedCount++;
		LF_Debug.Info(FEATURE, string.Format("player %1 revived by medic %2", patientUid, medicUid));
		LF_EventBus.Publish(LF_OnPlayerRevived.Create(patientUid, medicUid, patient.GetOrigin()));
		LF_TreatmentBills.IssueForRevive(medic, patient);
		LF_Utils.NotifyEntity(medic, "#LF-Revive_Done", "", 4.0);
		LF_Utils.NotifyEntity(patient, "#LF-Revive_Revived", "", 6.0);
		return LF_EReviveRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the revive with its cost (#427). The medic must carry `m_iReviveItemCount` of
	//! `m_sReviveItemPrefab` (a field dressing); it is used up only when the revive happens. An empty
	//! prefab in config means the revive is free.
	static LF_EReviveRefusal ApplyWithCost(IEntity medic, IEntity patient, bool medicOnDuty)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		bool costs = medic && !config.m_sReviveItemPrefab.IsEmpty() && config.m_iReviveItemCount > 0;
		LF_EReviveRefusal early = Check(medic, patient, medicOnDuty);
		if (costs && early == LF_EReviveRefusal.NONE && LF_InventoryUtils.GetAmount(medic, config.m_sReviveItemPrefab) < config.m_iReviveItemCount)
		{
			LF_Debug.Info(FEATURE, string.Format("revive of %1 by %2 refused: no %3", Uid(patient), Uid(medic), config.m_sReviveItemPrefab));
			LF_Utils.NotifyEntity(medic, RefusalKey(LF_EReviveRefusal.NO_ITEM), "", 4.0);
			return LF_EReviveRefusal.NO_ITEM;
		}

		LF_EReviveRefusal refusal = Apply(medic, patient, medicOnDuty);
		if (refusal == LF_EReviveRefusal.NONE && costs)
		{
			int removed = LF_InventoryUtils.RemoveAmount(medic, config.m_sReviveItemPrefab, config.m_iReviveItemCount);
			LF_Debug.Info(FEATURE, string.Format("revive of %1 used %2 of %3", Uid(patient), removed, config.m_sReviveItemPrefab));
		}

		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the action's entry. Reads the medic's duty from the account cache.
	static LF_EReviveRefusal Perform(IEntity medic, IEntity patient)
	{
		bool onDuty;
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts && medic)
			onDuty = LF_MedicDuty.IsOnDuty(accounts.GetAccount(LF_Utils.GetPlayerUid(medic)));

		return ApplyWithCost(medic, patient, onDuty);
	}

	//------------------------------------------------------------------------------------------------
	//! Client and server: is the revive offered to this user on this target? The client reads the
	//! replicated account state and the downed flag; the server re-checks in Perform.
	static bool CanOffer(IEntity user, IEntity target)
	{
		return Check(user, target, LF_MedicDuty.IsOnDutyState(LF_AccountReplication.GetState(user))) == LF_EReviveRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	static int GetRevivedCount()
	{
		return s_iRevivedCount;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Uid(IEntity entity)
	{
		string uid = LF_Utils.GetPlayerUID(entity);
		if (uid.IsEmpty())
			return "npc";

		return uid;
	}
}

//------------------------------------------------------------------------------------------------
//! The Revive entry on a player's character (context LF_ID). Shown only to an on-duty medic facing
//! a downed player within range; the server decides in LF_Revive.Perform.
class LF_ReviveAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Medic"))
			return false;

		return LF_Revive.CanOffer(user, GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return LF_Revive.CanOffer(user, GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! The hold began on the acting client: ask the server to show the CPR to everyone (#512).
	override void OnActionStart(IEntity pUserEntity)
	{
		AskAnim(pUserEntity, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnActionCanceled(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		AskAnim(pUserEntity, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void AskAnim(IEntity user, bool start)
	{
		if (!user || user != LF_Utils.GetLocalCharacter())
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.LF_AskReviveAnim(LF_NetworkUtils.GetRplId(GetOwner()), start);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Medic", "LF_ReviveAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_ReviveAnim.End(pUserEntity, "revive done");
		LF_Revive.Perform(pUserEntity, pOwnerEntity);
	}
}
