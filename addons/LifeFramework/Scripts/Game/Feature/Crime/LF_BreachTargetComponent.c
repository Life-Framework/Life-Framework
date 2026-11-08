//------------------------------------------------------------------------------------------------
//! Marks a door, gate, or container as a breach target and registers it with LF_Breach
//! on the server. The target id is the persistence key and the world entity name.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Crime", description: "Door a configured charge can open until repair.")]
class LF_BreachTargetComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_BreachTargetComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Stable world target id. Empty uses the entity name.")]
	protected string m_sTargetId;

	[Attribute("0", UIWidgets.EditBox, desc: "Fuse seconds. Zero opens the lock inside Arm.")]
	protected float m_fFuseSeconds;

	[Attribute("", UIWidgets.EditBox, desc: "Owning organization id. Empty means only police can repair.")]
	protected string m_sOwnerOrganization;

	protected bool m_bFusePending;

	//! Replicated copy of the LF_Breach target flags (the table is server-only). Written by
	//! LF_Breach on the server, read by the breach actions on a client. Late joiners get them
	//! with the door snapshot.
	[RplProp()]
	protected bool m_bRplOpen;

	[RplProp()]
	protected bool m_bRplArmed;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		string targetId = GetTargetId();
		if (targetId.IsEmpty())
		{
			EL_Debug.Error("Breach", "target component has no id and the entity is unnamed");
			return;
		}

		if (!EL_Component<EL_LockComponent>.Find(owner))
		{
			EL_Debug.Error("Breach", string.Format("target %1 has no EL_LockComponent", targetId));
			return;
		}

		LF_Breach.Configure(targetId, m_fFuseSeconds, m_sOwnerOrganization);
	}

	//------------------------------------------------------------------------------------------------
	string GetTargetId()
	{
		if (!m_sTargetId.IsEmpty())
			return m_sTargetId;

		if (GetOwner())
			return GetOwner().GetName();

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: mirror the LF_Breach flags. Bumps only on a change.
	void SetReplicatedState(bool isOpen, bool isArmed)
	{
		if (!Replication.IsServer())
			return;

		if (isOpen == m_bRplOpen && isArmed == m_bRplArmed)
			return;

		m_bRplOpen = isOpen;
		m_bRplArmed = isArmed;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	bool IsReplicatedOpen()
	{
		return m_bRplOpen;
	}

	//------------------------------------------------------------------------------------------------
	bool IsReplicatedArmed()
	{
		return m_bRplArmed;
	}

	//------------------------------------------------------------------------------------------------
	//! Offer check for the arm action. The server asks LF_Breach; a client reads the mirror.
	bool CanArmForOffer()
	{
		if (Replication.IsServer())
			return LF_Breach.CanArm(GetTargetId());

		return !m_bRplOpen && !m_bRplArmed;
	}

	//------------------------------------------------------------------------------------------------
	//! Offer check for the repair action. The server asks LF_Breach. A client shows it to police
	//! (replicated account state) and, on an organization door, to anyone: organization
	//! membership is server-only, so LF_Breach.Repair decides and logs the refusal.
	bool CanRepairForOffer(IEntity user)
	{
		if (Replication.IsServer())
			return LF_Breach.CanRepair(GetTargetId(), EL_Utils.GetPlayerUid(user));

		if (EL_AccountStateBits.IsPolice(EL_AccountReplication.GetState(user)))
			return true;

		return !m_sOwnerOrganization.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Server timer for a positive fuse. Fuse 0 never reaches this; Arm opens inline.
	//! \return False when no timer was scheduled (the caller cancels the charge).
	bool ScheduleFuse(float fuseSeconds)
	{
		if (fuseSeconds <= 0)
			return false;

		if (!GetGame() || !GetGame().GetCallqueue())
		{
			EL_Debug.Error("Breach", "fuse timer skipped: no callqueue");
			return false;
		}

		int delayMs = fuseSeconds * 1000;
		if (delayMs < 1)
			delayMs = 1;

		GetGame().GetCallqueue().CallLater(OnFuse, delayMs, false);
		m_bFusePending = true;
		EL_Debug.Log("Breach", string.Format("fuse scheduled %1 ms=%2", GetTargetId(), delayMs));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFuse()
	{
		m_bFusePending = false;
		if (!Replication.IsServer())
			return;

		LF_Breach.Advance(GetTargetId(), m_fFuseSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Door deleted before its fuse fired: drop the timer and the charge so the target is not armed forever.
	override void OnDelete(IEntity owner)
	{
		if (m_bFusePending)
		{
			m_bFusePending = false;
			if (GetGame() && GetGame().GetCallqueue())
				GetGame().GetCallqueue().Remove(OnFuse);

			LF_Breach.CancelCharge(GetTargetId(), "door deleted before the fuse fired");
		}

		super.OnDelete(owner);
	}
}
