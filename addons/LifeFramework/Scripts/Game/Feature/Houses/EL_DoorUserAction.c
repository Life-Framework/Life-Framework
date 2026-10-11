//------------------------------------------------------------------------------------------------
//! Door action gate for locked houses, as a modded override of the vanilla SCR_DoorUserAction
//! (not a replaced action class). The vanilla action keeps its full behavior; the lock check is
//! an extra gate on CanBePerformedScript.
//!
//! SECURITY POSTURE. CanBePerformedScript runs on the client (UI prompt) and again on the
//! server when the action is performed, so the gate is server-validated. The lock state it
//! reads is a replicated RplProp written server-side only (see EL_LockComponent). The action
//! stays visible on a locked door with the reason set, matching the vanilla vehicle-lock UX.
//!
//! The gate reads EL_LockComponent cached from the owner in Init, so it only applies to doors
//! that carry the component. Vanilla doors without it behave exactly as before.
//------------------------------------------------------------------------------------------------
modded class SCR_DoorUserAction
{
	protected EL_LockComponent m_pHouseLock;
	protected LF_HideoutDoorComponent m_pHideoutDoor;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);

		m_pHouseLock = EL_Component<EL_LockComponent>.Find(pOwnerEntity);
		m_pHideoutDoor = EL_Component<LF_HideoutDoorComponent>.Find(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		if (!m_pHouseLock || !m_pHouseLock.IsLocked())
			return true;

		if (m_pHideoutDoor)
		{
			if (!Replication.IsServer())
				return true;

			string characterId = LF_Hideout.CharacterIdOf(user);
			if (!LF_Hideout.AllowsUnlock(characterId, m_pHideoutDoor.GetHideoutId(), true))
			{
				SetCannotPerformReason("#EL-Hideout_NotMember");
				return false;
			}

			return true;
		}

		if (!m_pHouseLock.UserHasValidKey(user))
		{
			SetCannotPerformReason("#EL-HouseLock_Reason");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a member unlocks a hideout door. A valid key still unlocks a house door.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (m_pHideoutDoor && Replication.IsServer())
		{
			string characterId = LF_Hideout.CharacterIdOf(pUserEntity);
			if (!LF_Hideout.TryUnlock(characterId, m_pHideoutDoor.GetHideoutId()))
				return;
		}

		super.PerformAction(pOwnerEntity, pUserEntity);

		if (!Replication.IsServer() || m_pHideoutDoor)
			return;

		if (m_pHouseLock && m_pHouseLock.IsLocked() && m_pHouseLock.UserHasValidKey(pUserEntity))
		{
			m_pHouseLock.Unlock();
			EL_Debug.Log("Houses", "door unlocked by valid key");
		}
	}
};