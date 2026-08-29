//------------------------------------------------------------------------------------------------
//! Vehicle lock: a replicated locked flag plus a replicated vehicle identifier that the
//! matching key must carry.
//!
//! IDENTIFIER. The vehicle generates a PersistenceIdUtils UUID at spawn (server side only) so
//! every spawned vehicle is uniquely keyed and two instances of the same prefab do not share a
//! key. An editor-set m_sDebugIdentifier overrides generation and binds a whole prefab lineage to
//! one identifier.
//!
//! PERSISTENCE GAP. The identifier and the locked flag are in-session state. Vehicles are not
//! persisted entities in Configs/Systems/Persistence/LifeFramework.conf (no vehicle
//! EntityPersistenceConfig, no SelfSpawn), so after a server restart every vehicle spawns fresh:
//! a new identifier is generated and the lock resets to unlocked. A key from before the restart
//! will not match the respawned vehicle. Wiring lock state into persistence requires a vehicle
//! entity persistence config plus a ScriptedComponentSerializer for this component; that is
//! deferred until vehicle ownership/persistence exists.
//!
//! REPLICATION. Both RplProps are written on the server only (identifier generation and the
//! action path), so a client never generates a conflicting identifier. BumpMe is throttled: only
//! when the value actually changes.
//!
//! EXTENDS SCR_BaseLockComponent. The vanilla SCR_GetInUserAction / door actions gate entry via
//! FindComponent(SCR_BaseLockComponent).IsLocked(user, compartment), so the lock must be a
//! SCR_BaseLockComponent to actually stop a stranger from getting in. The vanilla base's own
//! m_bIsLocked is a plain attribute that does not replicate; this class carries its own
//! [RplProp] backing flag and overrides SetLocked/IsLocked to read it.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/VehicleLock", description: "Locks a vehicle to players holding a matching key.")]
class EL_VehicleLockComponentClass : SCR_BaseLockComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_VehicleLockComponent : SCR_BaseLockComponent
{
	[Attribute("", desc: "Editor binding: vehicle identifier this lock accepts. Leave empty to auto-generate at spawn.")]
	protected string m_sDebugIdentifier;

	[RplProp()]
	protected bool m_bELIsLocked;

	[RplProp()]
	protected string m_sVehicleIdentifier;

	//------------------------------------------------------------------------------------------------
	//! A key matches when both identifiers are non-empty and equal. Two empty identifiers must not
	//! match: an unbound key opens nothing.
	//! \param a First identifier (typically the key's).
	//! \param b Second identifier (typically the vehicle's).
	//! \return True when both are set and equal.
	static bool IdentifiersMatch(string a, string b)
	{
		if (a.IsEmpty() || b.IsEmpty())
			return false;

		return a == b;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		// Direct field writes: the initial RplProp value broadcasts on registration, so BumpMe
		// (which marks a CHANGE) is not needed here. Same pattern as EL_LicensePlateManagerComponent.
		if (Replication.IsServer() && m_sVehicleIdentifier.IsEmpty())
		{
			if (!m_sDebugIdentifier.IsEmpty())
				m_sVehicleIdentifier = m_sDebugIdentifier;
			else
				m_sVehicleIdentifier = PersistenceIdUtils.Generate();

			EL_Debug.Log("VehicleLock", string.Format("identifier assigned: %1", m_sVehicleIdentifier));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the vehicle is locked.
	bool IsVehicleLocked()
	{
		return m_bELIsLocked;
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla gate: SCR_GetInUserAction / door actions call this to block entry into a locked
	//! vehicle. The base's own field is never set; the replicated flag is the source of truth.
	//! \param user The entity trying to access.
	//! \param compartmentSlot The compartment being accessed.
	//! \return True when the vehicle is locked.
	override bool IsLocked(IEntity user, BaseCompartmentSlot compartmentSlot)
	{
		if (m_bELIsLocked)
			return true;

		return super.IsLocked(user, compartmentSlot);
	}

	//------------------------------------------------------------------------------------------------
	override void SetLocked(bool locked)
	{
		if (m_bELIsLocked == locked)
			return;

		m_bELIsLocked = locked;
		Replication.BumpMe();

		string stateName = "unlocked";
		if (locked)
			stateName = "locked";
		EL_Debug.Log("VehicleLock", string.Format("lock state -> %1", stateName));
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla SCR_GetInUserAction shows this reason when the vehicle is locked.
	//! \param user The entity trying to get in (unused; the lock is vehicle-wide).
	//! \return A localized reason when locked, else the base spawn-protection text.
	override LocalizedString GetCannotPerformReason(IEntity user)
	{
		if (m_bELIsLocked)
			return "#EL-VehicleLock_Reason";

		return super.GetCannotPerformReason(user);
	}

	//------------------------------------------------------------------------------------------------
	void ToggleLocked()
	{
		SetLocked(!m_bELIsLocked);
	}

	//------------------------------------------------------------------------------------------------
	string GetVehicleIdentifier()
	{
		return m_sVehicleIdentifier;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-authoritative: only the server assigns or re-binds a vehicle identifier.
	//! \param identifier The identifier the matching key must carry.
	void SetVehicleIdentifier(string identifier)
	{
		if (!Replication.IsServer())
			return;

		if (m_sVehicleIdentifier == identifier)
			return;

		m_sVehicleIdentifier = identifier;
		Replication.BumpMe();
		EL_Debug.Log("VehicleLock", string.Format("identifier rebound: %1", identifier));
	}

	//------------------------------------------------------------------------------------------------
	//! \param key The item to test.
	//! \return True when the key carries an EL_VehicleKeyComponent whose identifier matches.
	bool IsValidKey(IEntity key)
	{
		if (!key)
			return false;

		EL_VehicleKeyComponent keyComponent = EL_Component<EL_VehicleKeyComponent>.Find(key);
		if (!keyComponent)
			return false;

		bool matches = IdentifiersMatch(keyComponent.GetVehicleIdentifier(), m_sVehicleIdentifier);
		EL_Debug.Log("VehicleLock", string.Format("key check: key=%1 vehicle=%2 match=%3",
			keyComponent.GetVehicleIdentifier(), m_sVehicleIdentifier, matches));
		return matches;
	}

	//------------------------------------------------------------------------------------------------
	//! \param user The character to search.
	//! \return True when the user holds a matching key in the left-hand gadget slot or anywhere in
	//! the root items of their inventory. Keys inside nested containers are not found.
	bool UserHasValidKey(IEntity user)
	{
		if (!user)
			return false;

		CharacterControllerComponent characterController = EL_Component<CharacterControllerComponent>.Find(user);
		if (characterController && IsValidKey(characterController.GetAttachedGadgetAtLeftHandSlot()))
			return true;

		SCR_InventoryStorageManagerComponent inventoryManager = EL_Component<SCR_InventoryStorageManagerComponent>.Find(user);
		if (!inventoryManager)
			return false;

		array<IEntity> inventoryItems = {};
		inventoryManager.GetAllRootItems(inventoryItems);
		foreach (IEntity item : inventoryItems)
		{
			if (IsValidKey(item))
				return true;
		}

		return false;
	}
}