//------------------------------------------------------------------------------------------------
//! "Make this my spawn" at the door of a house the player owns (#2651, spec #811).
//!
//! WHO. Only the owner of record of the house (LF_Properties.GetOwnerKey). A key holder or a stranger
//! does not see the action, and a forged server-side perform by them is refused and logged.
//!
//! WHERE IT IS KEPT. The choice is m_bOwnerSpawnsHere on the house's "properties" row, so it is saved
//! with the record and needs no new payload. One per owner: setting a house clears the flag on every
//! other row that owner owns. A row whose house was sold keeps a stale flag that SpawnHouseOf ignores,
//! because it checks the current owner.
//!
//! WHEN IT APPLIES. The spawn flow reads SpawnHouseOf at login only; a death still wakes at hospital.
//!
//! SERVER DECIDES. The action sits on the house door (EL_Door_Base.et beside LF_HouseLockAction); the
//! house comes from the door the caller used, never from an id the client sends.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_SetHomeAction : ScriptedUserAction
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
	//! \return The house this door belongs to: the building it is a child of, else the live house
	//! registered under the door's identifier. Null for a door that belongs to no house.
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
	//! \return True when the user owns this door's house and it is not already their spawn house.
	protected bool UserMaySet(IEntity user)
	{
		if (!user || m_pHideoutDoor || !LF_Systems.IsEnabled(LF_LockComponent.SYSTEM))
			return false;

		LF_HouseManagerComponent house = FindHouse();
		if (!house || house.GetHouseIdentifier().IsEmpty())
			return false;

		string key = LF_Houses.CharacterKeyOf(user);
		if (key.IsEmpty() || key != LF_Properties.GetOwnerKey(house.GetHouseIdentifier()))
			return false;

		return LF_Properties.SpawnHouseOf(key) != house.GetHouseIdentifier();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return UserMaySet(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return UserMaySet(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Property_SetHome";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: makes this house the user's spawn house; LF_Properties refuses anyone but the owner.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_HouseManagerComponent house = FindHouse();
		if (!house)
		{
			LF_Debug.Warn(LF_Properties.FEATURE, "set spawn house refused: this door belongs to no house");
			return;
		}

		if (m_pHideoutDoor || LF_Systems.Refuses(LF_LockComponent.SYSTEM, "set spawn house action"))
			return;

		LF_Properties.SetSpawnHouse(LF_Houses.CharacterKeyOf(pUserEntity), house.GetHouseIdentifier());
	}
}

//------------------------------------------------------------------------------------------------
//! The spawn-house choice on the property book (#2651): m_bOwnerSpawnsHere, at most one per owner.
modded class LF_Properties
{
	//------------------------------------------------------------------------------------------------
	//! Server: makes a house the owner's spawn house and clears the flag on the owner's other rows.
	//! \param ownerKey The caller's character key; must be the property's current owner.
	//! \param propertyId The house identifier.
	//! \return False, with the reason logged, for an unknown caller, no property or a non-owner.
	static bool SetSpawnHouse(string ownerKey, string propertyId)
	{
		if (ownerKey.IsEmpty() || propertyId.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("set spawn house refused: no character key or property (key='%1', property='%2')", ownerKey, propertyId));
			return false;
		}

		string owner = GetOwnerKey(propertyId);
		if (owner != ownerKey)
		{
			LF_Debug.Warn(FEATURE, string.Format("set spawn house refused: %1 does not own %2 (owner '%3')", ownerKey, propertyId, owner));
			return false;
		}

		LF_PropertyRecord chosen = Create(propertyId);
		if (!chosen)
			return false;

		chosen.m_sOwnerKey = owner;
		foreach (string id, LF_PropertyRecord record : Records())
		{
			if (record != chosen && record.m_bOwnerSpawnsHere && (record.m_sOwnerKey == ownerKey || GetOwnerKey(id) == ownerKey))
			{
				record.m_bOwnerSpawnsHere = false;
				LF_Debug.Info(FEATURE, string.Format("property %1 is no longer the spawn house of %2", id, ownerKey));
			}
		}

		chosen.m_bOwnerSpawnsHere = true;
		LF_Debug.Info(FEATURE, string.Format("property %1 is now the spawn house of %2", propertyId, ownerKey));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \param ownerKey A character key.
	//! \return The id of the property that owner spawns at on login, empty when none. A flagged row
	//! counts only while the owner who set it (the row's mirror) still owns the property.
	static string SpawnHouseOf(string ownerKey)
	{
		if (ownerKey.IsEmpty())
			return "";

		foreach (string id, LF_PropertyRecord record : Records())
		{
			if (record.m_bOwnerSpawnsHere && record.m_sOwnerKey == ownerKey && GetOwnerKey(id) == ownerKey)
				return id;
		}

		return "";
	}
}
