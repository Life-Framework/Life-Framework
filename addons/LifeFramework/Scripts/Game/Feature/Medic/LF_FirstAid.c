//------------------------------------------------------------------------------------------------
//! First aid on a downed player (#257). The vanilla bandage, tourniquet and saline bag keep their
//! animation and their healing; when one is applied to a downed player on the server it also adds
//! bleed-out time (config per item, capped), and publishes LF_OnFirstAid. Morphine adds nothing.
//! Who may add time is config: anyone, or only players whose job is medic.
//------------------------------------------------------------------------------------------------
class LF_FirstAid
{
	static const string FEATURE = "Medic";
	static const string KIND_BANDAGE = "bandage";
	static const string KIND_TOURNIQUET = "tourniquet";
	static const string KIND_SALINE = "saline";

	//------------------------------------------------------------------------------------------------
	//! Seconds that really get added: `add`, trimmed so the time left never passes `cap` and never
	//! shrinks when it is already above it.
	static float Grant(float left, float add, float cap)
	{
		if (add <= 0 || left >= cap)
			return 0;

		return Math.Min(add, cap - left);
	}

	//------------------------------------------------------------------------------------------------
	static bool MayApply(bool anyone, int helperJob)
	{
		return anyone || helperJob == LF_EJobType.MEDIC;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds one use of this kind adds before the cap; zero for an unknown kind (morphine).
	static float SecondsFor(string kind)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		if (kind == KIND_BANDAGE)
			return config.m_fBandageSeconds;

		if (kind == KIND_TOURNIQUET)
			return config.m_fTourniquetSeconds;

		if (kind == KIND_SALINE)
			return config.m_fSalineSeconds;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a vanilla medical item was applied to `target`. Adds the time when the target is
	//! downed, the helper may, and this exact item was not already counted.
	//! \return The seconds added.
	static float Apply(IEntity target, IEntity user, IEntity item, string kind)
	{
		if (!Replication.IsServer())
			return 0;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(target);
		if (!character || !character.LF_IsDowned())
			return 0;

		string patientUid = LF_Utils.GetPlayerUID(character);
		string helperUid = LF_Utils.GetPlayerUID(user);
		if (!MayApply(LF_MedicConfig.Get().m_bFirstAidAnyone, HelperJob(user)))
		{
			LF_Debug.Info(FEATURE, string.Format("first aid on %1 adds no time: %2 is not a medic and config says medic only", patientUid, helperUid));
			return 0;
		}

		EntityID itemId = EntityID.INVALID;
		if (item)
			itemId = item.GetID();

		if (!character.LF_ClaimAidItem(itemId))
			return 0;

		float granted = Grant(character.LF_GetBleedLeft(), SecondsFor(kind), LF_MedicConfig.Get().m_fFirstAidCapSeconds);
		if (granted <= 0)
		{
			LF_Debug.Info(FEATURE, string.Format("first aid (%1) on %2 adds no time", kind, patientUid));
			return 0;
		}

		character.LF_AddBleedSeconds(granted);
		LF_Debug.Info(FEATURE, string.Format("first aid (%1) by %2 added %3s to %4", kind, helperUid, granted, patientUid));
		LF_EventBus.Publish(LF_OnFirstAid.Create(patientUid, helperUid, kind, granted, character.GetOrigin()));
		return granted;
	}

	//------------------------------------------------------------------------------------------------
	protected static int HelperJob(IEntity user)
	{
		if (!user)
			return LF_EJobType.UNEMPLOYED;

		int job = LF_EJobType.UNEMPLOYED;
		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(user);
		if (jobs)
			job = jobs.GetJob();

		// A medic clocked in at the hospital desk counts as the medic job.
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts)
			job = LF_ServiceCalls.JobFor(accounts.GetAccount(LF_Utils.GetPlayerUid(user)), job);

		return job;
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
modded class SCR_ConsumableBandage
{
	override void ApplyEffect(notnull IEntity target, notnull IEntity user, IEntity item, ItemUseParameters animParams)
	{
		super.ApplyEffect(target, user, item, animParams);
		LF_FirstAid.Apply(target, user, item, LF_FirstAid.KIND_BANDAGE);
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
modded class SCR_ConsumableTourniquet
{
	override void ApplyEffect(notnull IEntity target, notnull IEntity user, IEntity item, ItemUseParameters animParams)
	{
		super.ApplyEffect(target, user, item, animParams);
		LF_FirstAid.Apply(target, user, item, LF_FirstAid.KIND_TOURNIQUET);
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
modded class SCR_ConsumableSalineBag
{
	override void ApplyEffect(notnull IEntity target, notnull IEntity user, IEntity item, ItemUseParameters animParams)
	{
		super.ApplyEffect(target, user, item, animParams);
		LF_FirstAid.Apply(target, user, item, LF_FirstAid.KIND_SALINE);
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	protected EntityID m_LF_LastAidItem;

	//------------------------------------------------------------------------------------------------
	//! Server: take the first-aid credit for this item. False when the same item was already counted
	//! (one bandage extends once, however many times the engine reports the application).
	bool LF_ClaimAidItem(EntityID itemId)
	{
		if (itemId != EntityID.INVALID && itemId == m_LF_LastAidItem)
			return false;

		m_LF_LastAidItem = itemId;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: add bleed-out seconds and move the replicated copy at once (a rare, deliberate bump).
	void LF_AddBleedSeconds(float seconds)
	{
		m_fLF_BleedLeft += seconds;
		m_iLF_BleedSecondsLeft = Math.Ceil(m_fLF_BleedLeft);
		Replication.BumpMe();
	}
}
