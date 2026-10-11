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
	//! Server timer for a positive fuse. Fuse 0 never reaches this; Arm opens inline.
	void ScheduleFuse(float fuseSeconds)
	{
		if (fuseSeconds <= 0)
			return;

		if (!GetGame() || !GetGame().GetCallqueue())
		{
			EL_Debug.Error("Breach", "fuse timer skipped: no callqueue");
			return;
		}

		int delayMs = fuseSeconds * 1000;
		if (delayMs < 1)
			delayMs = 1;

		GetGame().GetCallqueue().CallLater(OnFuse, delayMs, false);
		EL_Debug.Log("Breach", string.Format("fuse scheduled %1 ms=%2", GetTargetId(), delayMs));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFuse()
	{
		if (!Replication.IsServer())
			return;

		LF_Breach.Advance(GetTargetId(), m_fFuseSeconds);
	}
}
