//------------------------------------------------------------------------------------------------
//! House door action: the owner or a key holder locks and unlocks the whole house at its door (#2471).
//! The action lives on the base house door (EL_Door_Base.et) beside the vanilla open action.
//!
//! WHO. The same gate the door uses to let someone through a locked door: LF_LockComponent.UserMayEnter
//! (the owner, or a carried key matching the house). A stranger does not see the action at all.
//! It is never offered on a hideout door (its members have their own rule), on a door that belongs to
//! no house manager, or while Houses is off (LF_LockComponent.SYSTEM).
//!
//! SERVER DECIDES. CanBeShown/CanBePerformed run on the client for the prompt and PerformAction runs
//! on the server too: the server re-checks the gate and refuses a forged request with LF_Debug.Warn.
//! The lock changes only through LF_HouseManagerComponent.SetHouseLocked (saved, mirrored on every door).
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_HouseLockAction : ScriptedUserAction
{
	protected LF_LockComponent m_pDoorLock;
	protected LF_HideoutDoorComponent m_pHideoutDoor;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);

		m_pDoorLock = LF_Component<LF_LockComponent>.Find(pOwnerEntity);
		m_pHideoutDoor = LF_Component<LF_HideoutDoorComponent>.Find(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The house manager this door belongs to: the building it is a child of, else the live
	//! house registered under the door's identifier. Null for a door that belongs to no house.
	protected LF_HouseManagerComponent FindHouse()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;

		IEntity parent = owner.GetParent();
		while (parent)
		{
			LF_HouseManagerComponent house = LF_Component<LF_HouseManagerComponent>.Find(parent);
			if (house)
				return house;

			parent = parent.GetParent();
		}

		if (!m_pDoorLock || m_pDoorLock.GetHouseIdentifier().IsEmpty())
			return null;

		return LF_Houses.Find(m_pDoorLock.GetHouseIdentifier());
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when this door is a house door the user may lock or unlock from.
	protected bool UserMayToggle(IEntity user)
	{
		if (!user || !m_pDoorLock || m_pHideoutDoor)
			return false;

		if (!LF_Systems.IsEnabled(LF_LockComponent.SYSTEM))
			return false;

		if (!FindHouse())
			return false;

		return m_pDoorLock.UserMayEnter(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return UserMayToggle(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return UserMayToggle(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (m_pDoorLock && m_pDoorLock.IsLocked())
			outName = "#LF-HouseLock_Unlock";
		else
			outName = "#LF-HouseLock_Lock";

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: flips the house lock for the owner or a key holder; anyone else is refused and logged.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_HouseManagerComponent house = FindHouse();
		if (!house)
		{
			LF_Debug.Warn("Houses", "house lock refused: this door belongs to no house");
			return;
		}

		if (LF_Systems.Refuses(LF_LockComponent.SYSTEM, "house lock action"))
			return;

		if (!UserMayToggle(pUserEntity))
		{
			LF_Debug.Warn("Houses", string.Format("house lock refused on the server: not the owner or a key holder, house=%1", house.GetHouseIdentifier()));
			return;
		}

		house.SetHouseLocked(!house.IsHouseLocked());
	}
}
