//------------------------------------------------------------------------------------------------
//! Vehicle lock: a replicated locked flag plus a replicated vehicle identifier that the
//! matching key must carry.
//!
//! IDENTIFIER. The vehicle generates a PersistenceIdUtils UUID at spawn (server side only) so
//! every spawned vehicle is uniquely keyed and two instances of the same prefab do not share a
//! key. An editor-set m_sDebugIdentifier overrides generation and binds a whole prefab lineage to
//! one identifier.
//!
//! PERSISTENCE. This component's fields are still session state. LF_VehicleOwnership stores the
//! identifier, the key id, the owner, and the lock flag by that stable id. A restart spawns the
//! prefab at the lot spawn point (the same rule as a character spawn point) and SetVehicleIdentifier
//! writes the saved id back, so the key from the record matches. The vehicle entity itself is not
//! a SelfSpawn persistence config.
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
class LF_VehicleLockComponentClass : SCR_BaseLockComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_VehicleLockComponent : SCR_BaseLockComponent
{
	[Attribute("", desc: "Editor binding: vehicle identifier this lock accepts. Leave empty to auto-generate at spawn.")]
	protected string m_sDebugIdentifier;

	[Attribute("0", desc: "The engine needs a matching key (or a hotwire, #412) to start. Off keeps the vanilla free start.")]
	protected bool m_bIgnitionLocked;

	[Attribute("0", desc: "Lock the vehicle when the world starts (a placed car, like LF_LockComponent.m_bDebugLocked on a door).")]
	protected bool m_bDebugLocked;

	[RplProp()]
	protected bool m_bELIsLocked;

	//! Player id who hotwired this vehicle; -1 none. Server-written, read by the engine start gate.
	[RplProp()]
	protected int m_iHotwiredPlayerId = -1;

	//! Set on the server when the engine is started without the vehicle's key. Session state, not saved.
	[RplProp()]
	protected bool m_bStolen;

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

		// The initial identifier is assigned during initialization, so explicitly mark the RplProp dirty.
		if (Replication.IsServer() && m_sVehicleIdentifier.IsEmpty())
		{
			if (!m_sDebugIdentifier.IsEmpty())
				m_sVehicleIdentifier = m_sDebugIdentifier;
			else
				m_sVehicleIdentifier = PersistenceIdUtils.Generate();

			Replication.BumpMe();
			LF_Debug.Log("VehicleLock", string.Format("identifier assigned: %1", m_sVehicleIdentifier));
		}

		if (Replication.IsServer() && m_bDebugLocked)
			SetLocked(true);
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
		if (locked && Replication.IsServer())
			m_iHotwiredPlayerId = -1;

		Replication.BumpMe();

		string stateName = "unlocked";
		if (locked)
			stateName = "locked";
		LF_Debug.Log("VehicleLock", string.Format("lock state -> %1", stateName));
	}

	//------------------------------------------------------------------------------------------------
	bool IsIgnitionLocked()
	{
		return m_bIgnitionLocked;
	}

	//------------------------------------------------------------------------------------------------
	int GetHotwiredPlayerId()
	{
		return m_iHotwiredPlayerId;
	}

	//------------------------------------------------------------------------------------------------
	bool IsStolen()
	{
		return m_bStolen;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the engine may start for this player without a key.
	void SetHotwiredBy(int playerId)
	{
		if (!Replication.IsServer() || m_iHotwiredPlayerId == playerId)
			return;

		m_iHotwiredPlayerId = playerId;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: marks the vehicle stolen. Session state; a restart forgets it.
	void SetStolen(bool stolen)
	{
		if (!Replication.IsServer() || m_bStolen == stolen)
			return;

		m_bStolen = stolen;
		Replication.BumpMe();
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

	//! Placeholder lock/unlock sound: the base-game inventory click played on the user at the
	//! car (docs/asset-inventory.md). Swap this one name for a real door-latch event.
	static const string LOCK_SOUND = "SOUND_PICK_UP";
	//! How far from the vehicle a key press may be (the menu action is gated by its own context).
	static const float KEY_REACH = 6.0;
	//! Sort Priority of the lock action in the prefabs. The vanilla get-in action is 1 and a lower
	//! number is listed first, so the lock row sits below "Get in".
	static const int ACTION_SORT_PRIORITY = 5;

	//------------------------------------------------------------------------------------------------
	//! \param locked Current lock state.
	//! eturn The action label for what the action would do next.
	static string ActionLabelKey(bool locked)
	{
		if (locked)
			return "#EL-VehicleLock_ActionUnlock";

		return "#EL-VehicleLock_ActionLock";
	}

	//------------------------------------------------------------------------------------------------
	//! Permission for a lock request from an occupant or someone on foot: a matching key and reach.
	static bool MayToggle(bool hasKey, float distance)
	{
		if (!hasKey)
			return false;

		return distance <= KEY_REACH;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the one path for the menu action and the U key. Validates key and reach, flips the
	//! lock, records it, plays the sound and tells the user.
	//! \param user The character asking.
	//! \param source "menu" or "key", for the log.
	//! eturn True when the lock changed.
	bool TryToggleBy(IEntity user, string source)
	{
		if (!Replication.IsServer() || !user || !GetOwner())
			return false;

		float distance = vector.Distance(user.GetOrigin(), GetOwner().GetOrigin());
		if (!MayToggle(UserHasValidKey(user), distance))
		{
			LF_Debug.Warn("VehicleLock", string.Format("toggle refused (%1): no key or out of reach (%2 m)", source, distance));
			return false;
		}

		ToggleLocked();
		LF_VehicleOwnership.NoteLockState(GetVehicleIdentifier(), IsVehicleLocked());
		LF_Debug.Log("VehicleLock", string.Format("toggled by %1 -> locked=%2", source, IsVehicleLocked()));

		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(user);
		if (inventory)
			inventory.RpcAsk_PlaySound(LF_NetworkUtils.GetRplId(user), LOCK_SOUND);

		PlayerManager playerManager = GetGame().GetPlayerManager();
		int playerId = 0;
		if (playerManager)
			playerId = playerManager.GetPlayerIdFromControlledEntity(user);
		if (playerId > 0)
		{
			if (IsVehicleLocked())
				LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-VehicleLock_Title", "#EL-VehicleLocked");
			else
				LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-VehicleLock_Title", "#EL-VehicleUnlocked");
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the vehicle the U key means. Seated: the vehicle the user sits in. On foot: the
	//! nearest vehicle within reach whose key the user holds.
	static LF_VehicleLockComponent FindForKeyPress(IEntity user)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character)
			return null;

		CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
		if (access && character.IsInVehicle())
			return LF_Component<LF_VehicleLockComponent>.Find(access.GetVehicleIn(character));

		LF_VehicleLockComponent best;
		float bestDistance = KEY_REACH;
		array<IEntity> vehicles = LF_Utils.GetNearbyVehicles(user.GetOrigin(), KEY_REACH);
		foreach (IEntity vehicle : vehicles)
		{
			LF_VehicleLockComponent candidate = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
			if (!candidate || !candidate.UserHasValidKey(user))
				continue;

			float distance = vector.Distance(user.GetOrigin(), vehicle.GetOrigin());
			if (distance <= bestDistance)
			{
				bestDistance = distance;
				best = candidate;
			}
		}

		return best;
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
		LF_Debug.Log("VehicleLock", string.Format("identifier rebound: %1", identifier));
	}

	//------------------------------------------------------------------------------------------------
	//! \param key The item to test.
	//! \return True when the key carries an LF_VehicleKeyComponent whose identifier matches.
	bool IsValidKey(IEntity key)
	{
		if (!key)
			return false;

		LF_VehicleKeyComponent keyComponent = LF_Component<LF_VehicleKeyComponent>.Find(key);
		if (!keyComponent)
			return false;

		bool matches = IdentifiersMatch(keyComponent.GetVehicleIdentifier(), m_sVehicleIdentifier);
		LF_Debug.Log("VehicleLock", string.Format("key check: key=%1 vehicle=%2 match=%3",
			keyComponent.GetVehicleIdentifier(), m_sVehicleIdentifier, matches));
		return matches;
	}

	//------------------------------------------------------------------------------------------------
	//! \param user The character to search.
	//! \return True when the user holds a matching key in the left-hand gadget slot or anywhere in
	//! their inventory, including nested containers.
	bool UserHasValidKey(IEntity user)
	{
		if (!user)
			return false;

		CharacterControllerComponent characterController = LF_Component<CharacterControllerComponent>.Find(user);
		if (characterController && IsValidKey(characterController.GetAttachedGadgetAtLeftHandSlot()))
			return true;

		SCR_InventoryStorageManagerComponent inventoryManager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(user);
		if (!inventoryManager)
			return false;

		array<IEntity> inventoryItems = {};
		LF_VehicleKeyScanPredicate anyItemPredicate = new LF_VehicleKeyScanPredicate();
		inventoryManager.FindItems(inventoryItems, anyItemPredicate);
		foreach (IEntity item : inventoryItems)
		{
			if (IsValidKey(item))
				return true;
		}

		return false;
	}
}

class LF_VehicleKeyScanPredicate : InventorySearchPredicate
{
	//------------------------------------------------------------------------------------------------
	override protected bool IsMatch(BaseInventoryStorageComponent storage, IEntity item, array<GenericComponent> queriedComponents, array<BaseItemAttributeData> queriedAttributes)
	{
		return true;
	}
}
