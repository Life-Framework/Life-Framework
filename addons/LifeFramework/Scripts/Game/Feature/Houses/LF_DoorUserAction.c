//------------------------------------------------------------------------------------------------
//! Door action gate for locked houses, as a modded override of the vanilla SCR_DoorUserAction
//! (not a replaced action class). The vanilla action keeps its full behavior; the lock check is
//! an extra gate on CanBePerformedScript.
//!
//! SECURITY POSTURE. CanBePerformedScript runs on the client (UI prompt) and again on the
//! server when the action is performed, so the gate is server-validated. The lock state it
//! reads is a replicated RplProp written server-side only (see LF_LockComponent). The action
//! stays visible on a locked door with the reason set, matching the vanilla vehicle-lock UX.
//!
//! The gate reads LF_LockComponent cached from the owner in Init, so it only applies to doors
//! that carry the component. Vanilla doors without it behave exactly as before.
//------------------------------------------------------------------------------------------------
modded class SCR_DoorUserAction
{
	protected LF_LockComponent m_pHouseLock;
	protected LF_HideoutDoorComponent m_pHideoutDoor;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);

		m_pHouseLock = LF_Component<LF_LockComponent>.Find(pOwnerEntity);
		m_pHideoutDoor = LF_Component<LF_HideoutDoorComponent>.Find(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! What this hideout door is to this viewer: theirs, members only, or nobody's yet.
	protected LF_EHideoutDoorView HideoutView(IEntity user)
	{
		return LF_HideoutDoorRules.View(m_pHouseLock.GetHouseIdentifier(), LF_OrgReplication.ViewOf(user).m_sOrgId);
	}

	//------------------------------------------------------------------------------------------------
	//! A locked hideout door names itself for the viewer: "Open" for a member, "Members only (<org>)"
	//! for everyone else. Every other door keeps the vanilla Open/Close.
	override bool GetActionNameScript(out string outName)
	{
		if (m_pHideoutDoor && m_pHouseLock && m_pHouseLock.IsLocked())
		{
			LF_EHideoutDoorView view = HideoutView(LF_Utils.GetLocalCharacter());
			outName = LF_HideoutDoorRules.Label(view, m_pHideoutDoor.GetOwnerName());
			return true;
		}

		return super.GetActionNameScript(outName);
	}

	//------------------------------------------------------------------------------------------------
	//! A hideout door shows nothing while Hideouts is off (#1075).
	override bool CanBeShownScript(IEntity user)
	{
		if (m_pHideoutDoor && !LF_Systems.IsEnabled(LF_Hideout.SYSTEM))
			return false;

		return super.CanBeShownScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		if (!m_pHouseLock || !m_pHouseLock.IsLocked())
			return true;

		// Houses off: a house door is a plain door. The lock state stays saved and comes back (#1075).
		if (!m_pHideoutDoor && !LF_Systems.IsEnabled(LF_LockComponent.SYSTEM))
			return true;

		if (m_pHideoutDoor)
		{
			// The client reads the organization replicated on the character (LF_OrgReplication) against
			// the owner written on the lock, so a non-member sees the reason at the gate. The server
			// decides below from the registry.
			if (!Replication.IsServer())
			{
				LF_EHideoutDoorView view = HideoutView(user);
				if (view != LF_EHideoutDoorView.MEMBER)
				{
					SetCannotPerformReason(LF_HideoutDoorRules.Reason(view, m_pHideoutDoor.GetOwnerName()));
					return false;
				}

				return true;
			}

			string characterId = LF_Hideout.CharacterIdOf(user);
			if (!LF_Hideout.AllowsUnlock(characterId, m_pHideoutDoor.GetHideoutId(), true))
			{
				SetCannotPerformReason(LF_HideoutDoorRules.Reason(HideoutView(user), m_pHideoutDoor.GetOwnerName()));
				return false;
			}

			return true;
		}

		// The owner and key holders pass; the door stays locked behind them (#2410).
		if (!m_pHouseLock.UserMayEnter(user))
		{
			SetCannotPerformReason("#EL-HouseLock_Reason");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a member unlocks a hideout door. A house door is never unlocked by opening it: the
	//! owner or a key holder passes the gate and the lock stays on for everyone else (#2410). Only
	//! the house manager (LF_HouseManagerComponent.SetHouseLocked) changes a house's lock state.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (m_pHideoutDoor && Replication.IsServer())
		{
			string characterId = LF_Hideout.CharacterIdOf(pUserEntity);
			if (!LF_Hideout.TryUnlock(characterId, m_pHideoutDoor.GetHideoutId()))
				return;
		}

		// The server decides again here rather than trusting that the client's gate ran: a locked
		// house door moves only for its owner or a key holder.
		if (Replication.IsServer() && !m_pHideoutDoor && m_pHouseLock && m_pHouseLock.IsLocked()
			&& LF_Systems.IsEnabled(LF_LockComponent.SYSTEM) && !m_pHouseLock.UserMayEnter(pUserEntity))
		{
			LF_Debug.Warn("Houses", string.Format("locked door refused on the server: house=%1", m_pHouseLock.GetHouseIdentifier()));
			return;
		}

		super.PerformAction(pOwnerEntity, pUserEntity);

		if (!Replication.IsServer() || m_pHideoutDoor)
			return;

		if (m_pHouseLock && m_pHouseLock.IsLocked())
			LF_Debug.Log("Houses", string.Format("locked door opened by owner or key holder, lock kept: house=%1", m_pHouseLock.GetHouseIdentifier()));
	}
};
