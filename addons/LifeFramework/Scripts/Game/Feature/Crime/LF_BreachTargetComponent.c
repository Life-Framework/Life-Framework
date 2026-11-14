//------------------------------------------------------------------------------------------------
//! Marks a door, gate, or container as a breach target and registers it with LF_Breach
//! on the server. The target id is the persistence key; LF_Breach finds the door through LF_Places
//! under that id, so the entity needs no particular name.
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

	[Attribute("", UIWidgets.EditBox, desc: "Child entity (by name or prefab file name) that swings open when the target opens, such as the round vault door. Empty: no visual swing.")]
	protected string m_sSwingChild;

	[Attribute("100", UIWidgets.EditBox, desc: "Degrees the swing child turns about its own Y axis when open.")]
	protected float m_fSwingYaw;

	protected bool m_bFusePending;

	protected IEntity m_SwingFound;
	protected bool m_bSwingKnown;
	protected float m_fSwingClosedYaw;

	//! Replicated copy of the LF_Breach target flags (the table is server-only). Written by
	//! LF_Breach on the server, read by the breach actions on a client. Late joiners get them
	//! with the door snapshot.
	[RplProp(onRplName: "OnRplOpenChanged")]
	protected bool m_bRplOpen;

	[RplProp()]
	protected bool m_bRplArmed;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		string targetId = GetTargetId();
		if (!targetId.IsEmpty())
			LF_Places.Register(LF_Breach.PLACE_KIND, targetId, owner);

		if (!Replication.IsServer())
			return;

		if (targetId.IsEmpty())
		{
			LF_Debug.Error("Breach", "target component has no id and the entity is unnamed");
			return;
		}

		if (!LF_Component<LF_LockComponent>.Find(owner))
		{
			LF_Debug.Error("Breach", string.Format("target %1 has no LF_LockComponent", targetId));
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
		ApplySwing(isOpen);
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the open flag arrived (or changed). Turns the swing child to match.
	protected void OnRplOpenChanged()
	{
		ApplySwing(m_bRplOpen);
	}

	//------------------------------------------------------------------------------------------------
	//! The first child, at any depth, whose name, prefab file or mesh file contains m_sSwingChild.
	protected IEntity FindSwingChild(IEntity parent)
	{
		IEntity child = parent.GetChildren();
		while (child)
		{
			bool match = child.GetName() == m_sSwingChild;
			if (!match && child.GetPrefabData())
				match = child.GetPrefabData().GetPrefabName().Contains(m_sSwingChild);

			if (!match && child.GetVObject())
				match = child.GetVObject().GetResourceName().Contains(m_sSwingChild);

			if (match)
				return child;

			IEntity deeper = FindSwingChild(child);
			if (deeper)
				return deeper;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected bool OnQuerySwing(IEntity entity)
	{
		if (entity == GetOwner())
			return true;

		if (entity.GetVObject() && entity.GetVObject().GetResourceName().Contains(m_sSwingChild))
		{
			m_SwingFound = entity;
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Turns the swing child to its open or closed yaw. A missing child is logged once per call and
	//! the lock still works: the door only loses its motion.
	protected IEntity ResolveSwingChild()
	{
		if (m_sSwingChild.IsEmpty() || !GetOwner())
			return null;

		IEntity child = FindSwingChild(GetOwner());
		if (!child && GetOwner().GetWorld())
		{
			// A prefab's nested entities are not always hierarchy children: look for the door
			// mesh within a few metres instead.
			m_SwingFound = null;
			GetOwner().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), 4, OnQuerySwing);
			child = m_SwingFound;
			m_SwingFound = null;
		}

		return child;
	}

	//------------------------------------------------------------------------------------------------
	//! Harness `seen`: this machine's open flag and the yaw its swing child stands at now, as
	//! "open=O:yaw=Y" (yaw "none" without a swing child), so a proof reads what a client draws.
	string DescribeSwing()
	{
		int open = 0;
		if (m_bRplOpen)
			open = 1;

		IEntity child = ResolveSwingChild();
		if (!child)
			return string.Format("open=%1:yaw=none", open);

		return string.Format("open=%1:yaw=%2", open, Math.Round(child.GetAngles()[1]));
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplySwing(bool isOpen)
	{
		if (m_sSwingChild.IsEmpty() || !GetOwner())
			return;

		IEntity child = ResolveSwingChild();
		if (!child)
		{
			int count = 0;
			IEntity any = GetOwner().GetChildren();
			while (any)
			{
				count++;
				any = any.GetSibling();
			}

			LF_Debug.Warn("Breach", string.Format("swing child %1 not found under %2 (%3 children)", m_sSwingChild, GetTargetId(), count));
			return;
		}

		vector angles = child.GetAngles();
		if (!m_bSwingKnown)
		{
			// The child starts at its prefab yaw, which is the closed one, even when the first
			// call is a saved open vault.
			m_fSwingClosedYaw = angles[1];
			m_bSwingKnown = true;
		}

		float yaw = m_fSwingClosedYaw;
		if (isOpen)
			yaw = m_fSwingClosedYaw + m_fSwingYaw;

		angles[1] = yaw;
		child.SetAngles(angles);
		LF_Debug.Log("Breach", string.Format("swing %1 open=%2 yaw=%3", GetTargetId(), isOpen, yaw));
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
			return LF_Breach.CanRepair(GetTargetId(), LF_Utils.GetPlayerUid(user));

		if (LF_AccountStateBits.IsPolice(LF_AccountReplication.GetState(user)))
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
			LF_Debug.Error("Breach", "fuse timer skipped: no callqueue");
			return false;
		}

		int delayMs = fuseSeconds * 1000;
		if (delayMs < 1)
			delayMs = 1;

		GetGame().GetCallqueue().CallLater(OnFuse, delayMs, false);
		m_bFusePending = true;
		LF_Debug.Log("Breach", string.Format("fuse scheduled %1 ms=%2", GetTargetId(), delayMs));
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
