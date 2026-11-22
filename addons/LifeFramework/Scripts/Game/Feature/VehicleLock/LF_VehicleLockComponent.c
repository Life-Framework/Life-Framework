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

	[Attribute("0", desc: "A placed job car (the taxi rank's cab): a job started beside it hands it to that job's driver (HandNearestJobVehicle). Off: no job hands it out.")]
	protected bool m_bJobCar;

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

	//! The id a key must carry (#2411). Empty means the vehicle identifier, which is what every key
	//! carried before locks could be changed. Changing the locks gives it a new value, so every
	//! older key stops matching while the vehicle identifier (plates, records, crime) stays put.
	[RplProp()]
	protected string m_sKeyId;

	//! A service vehicle (#3875): a police car standing outside the station. No civilian lockpick, hotwire, key,
	//! chop shop or garage touches it. Server-written by the parked-car marker that put it out; never saved.
	[RplProp()]
	protected bool m_bServiceVehicle;

	//! The owner's record key (#3366). LF_VehicleOwnership's tables exist on the server only, so this
	//! is what a client reads to offer the owner's own car its "Store in garage" row. Server-written
	//! when the car is bought or comes out of the garage; empty on a car nobody owns. Display gate
	//! only: the server still checks LF_VehicleOwnership before it stores anything.
	[RplProp()]
	protected string m_sOwnerUid;

	//! A job vehicle (no free cars): the player id of the one driver the job gave it to (the garbage round's truck, the
	//! depot tanker after a load is bought); -1 none. While set, only that player gets in, whatever the locked
	//! flag says. Server-written by the job; session state like the job itself, never saved.
	[RplProp()]
	protected int m_iJobDriverPlayerId = -1;

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
		// No slot named: the strict answer, as for the driver's seat.
		bool pilot = !compartmentSlot || PilotCompartmentSlot.Cast(compartmentSlot) != null;
		if (!MayEnter(user, pilot))
			return true;

		return super.IsLocked(user, compartmentSlot);
	}

	//------------------------------------------------------------------------------------------------
	//! The one entry rule ("no free cars"), pure. A service car opens for an officer on duty only, locked or not.
	//! A job vehicle's wheel is its job driver's only, locked or not; its other seats (a taxi fare, a mate on the
	//! round) follow the locked flag like any car. Any other car opens when it is unlocked.
	//! \param systemOn LF_Systems "VehicleLock" is on; off (#1076) a saved locked flag shuts no door.
	//! \param jobDriverId The job vehicle's driver player id; -1 when the car is no job vehicle.
	//! \param userPlayerId The asking character's player id; 0 or -1 for no player.
	//! \param pilot The seat asked for is the driver's.
	static bool EntryRule(bool systemOn, bool locked, bool service, bool onDutyPolice, int jobDriverId, int userPlayerId, bool pilot)
	{
		if (!systemOn)
			return true;

		if (service)
			return onDutyPolice;

		if (jobDriverId > 0 && pilot)
			return userPlayerId == jobDriverId;

		return !locked;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when `user` may get into this vehicle now. Reads replicated state only, so the client's
	//! get-in offer and the server's entry guard (LF_VehicleEntryGuard) agree.
	//! \param pilot The seat asked for is the driver's.
	bool MayEnter(IEntity user, bool pilot = true)
	{
		return EntryRule(LF_Systems.IsEnabled("VehicleLock"), m_bELIsLocked, m_bServiceVehicle, IsServiceDriver(user), m_iJobDriverPlayerId, LF_PlayerInteraction.PlayerIdOf(user), pilot);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True for a placed job car (m_bJobCar): only a job hands it out.
	bool IsJobCar()
	{
		return m_bJobCar;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The job driver's player id; -1 when this is no job vehicle.
	int GetJobDriverPlayerId()
	{
		return m_iJobDriverPlayerId;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when a job driver is set and that player is still on the server.
	bool IsHeldByOnlinePlayer()
	{
		if (m_iJobDriverPlayerId <= 0 || !GetGame() || !GetGame().GetPlayerManager())
			return false;

		return GetGame().GetPlayerManager().GetPlayerControlledEntity(m_iJobDriverPlayerId) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: hands the nearest locked job car within `radius` of `point` (the taxi rank's cab) to `driver`.
	//! A car whose holder stands within `radius` of it is skipped. \return The car's lock, or null (logged) when none is free.
	static LF_VehicleLockComponent HandNearestJobVehicle(IEntity point, float radius, IEntity driver)
	{
		if (!Replication.IsServer() || !point || !driver)
			return null;

		int playerId = LF_PlayerInteraction.PlayerIdOf(driver);
		LF_VehicleLockComponent best;
		float bestDistance = radius;
		array<IEntity> vehicles = LF_Utils.GetNearbyVehicles(point.GetOrigin(), radius);
		foreach (IEntity vehicle : vehicles)
		{
			LF_VehicleLockComponent candidate = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
			if (!candidate || !candidate.IsJobCar())
				continue;

			// A cab whose holder is still beside it (or in it) is theirs; one left at the rank goes to the next driver.
			int holder = candidate.GetJobDriverPlayerId();
			if (holder > 0 && holder != playerId && candidate.IsHeldByOnlinePlayer())
			{
				IEntity holderBody = GetGame().GetPlayerManager().GetPlayerControlledEntity(holder);
				if (vector.Distance(holderBody.GetOrigin(), vehicle.GetOrigin()) <= radius)
					continue;
			}

			float distance = vector.Distance(point.GetOrigin(), vehicle.GetOrigin());
			if (distance <= bestDistance)
			{
				bestDistance = distance;
				best = candidate;
			}
		}

		if (!best)
		{
			LF_Debug.Warn("VehicleLock", string.Format("no free job car within %1 m of %2 for player %3", radius, point.GetOrigin(), playerId));
			return null;
		}

		best.SetJobDriver(playerId);
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gives the vehicle to one job driver (only they take the wheel; the doors unlock so riders board),
	//! or -1 to end that: the car locks again.
	void SetJobDriver(int playerId)
	{
		if (!Replication.IsServer())
			return;

		if (playerId <= 0)
			playerId = -1;

		if (m_iJobDriverPlayerId == playerId)
			return;

		m_iJobDriverPlayerId = playerId;
		SetLocked(playerId < 0);
		Replication.BumpMe();
		LF_Debug.Info("VehicleLock", string.Format("job driver of %1 -> player %2", m_sVehicleIdentifier, playerId));
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
	//! \return True for a service vehicle (a police car on a marker): civilians cannot pick, wire, key, chop or store it.
	bool IsServiceVehicle()
	{
		return m_bServiceVehicle;
	}

	//------------------------------------------------------------------------------------------------
	//! A service vehicle opens and starts for an officer on duty and for nobody else, key or not. Reads the
	//! replicated account state, so the client's offer and the server's check agree.
	//! \param user The character asking.
	//! \return True when this is a service vehicle and user is on duty as police.
	bool IsServiceDriver(IEntity user)
	{
		if (!m_bServiceVehicle || !user)
			return false;

		return LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: marks or clears the service-vehicle flag.
	void SetServiceVehicle(bool service)
	{
		if (!Replication.IsServer() || m_bServiceVehicle == service)
			return;

		m_bServiceVehicle = service;
		Replication.BumpMe();
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
		if (!MayEnter(user, false))
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
	//! \return The action label for what the action would do next.
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
	//! \return True when the lock changed.
	bool TryToggleBy(IEntity user, string source)
	{
		if (LF_Systems.Refuses("VehicleLock", "lock toggle"))
			return false;

		if (!Replication.IsServer() || !user || !GetOwner())
			return false;

		// A police car is nobody's to lock or unlock with a key (#3875); an officer on duty opens it.
		if (m_bServiceVehicle && !IsServiceDriver(user))
		{
			LF_Debug.Warn("VehicleLock", string.Format("toggle refused (%1): service vehicle, user not on duty as police", source));
			return false;
		}

		// A company garbage truck waiting to be taken back is never lockable (#3325).
		if (LF_GarbageRound.IsPendingRemoval(GetOwner()))
		{
			LF_Debug.Warn("VehicleLock", string.Format("toggle refused (%1): company truck waiting to be taken back", source));
			return false;
		}

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
	//! \return The replicated owner record key; empty when the car has no owner.
	string GetOwnerUid()
	{
		return m_sOwnerUid;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the owner's record key, replicated so a client can tell its own car (#3366).
	void SetOwnerUid(string ownerUid)
	{
		if (!Replication.IsServer() || m_sOwnerUid == ownerUid)
			return;

		m_sOwnerUid = ownerUid;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when both keys are set and equal. An unowned car matches nobody.
	static bool OwnerMatches(string carOwnerUid, string userUid)
	{
		if (carOwnerUid.IsEmpty() || userUid.IsEmpty())
			return false;

		return carOwnerUid == userUid;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The id a key must carry: the changed-locks key id, else the vehicle identifier.
	string GetKeyId()
	{
		if (!m_sKeyId.IsEmpty())
			return m_sKeyId;

		return m_sVehicleIdentifier;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the id a key must carry from now on (#2411). Empty or the vehicle identifier means
	//! the original key. A key bound to any other id stops opening this vehicle.
	void SetKeyId(string keyId)
	{
		if (!Replication.IsServer())
			return;

		if (keyId == m_sVehicleIdentifier)
			keyId = string.Empty;

		if (m_sKeyId == keyId)
			return;

		m_sKeyId = keyId;
		Replication.BumpMe();
		LF_Debug.Log("VehicleLock", string.Format("key id rebound on %1: %2", m_sVehicleIdentifier, GetKeyId()));
	}

	//------------------------------------------------------------------------------------------------
	//! \param key The item to test.
	//! \return True when the key carries an LF_VehicleKeyComponent bound to this lock's current key
	//! id. Action CanBePerformed runs this every frame, so it does not log (#2411).
	bool IsValidKey(IEntity key)
	{
		if (!key)
			return false;

		LF_VehicleKeyComponent keyComponent = LF_Component<LF_VehicleKeyComponent>.Find(key);
		if (!keyComponent)
			return false;

		return IdentifiersMatch(keyComponent.GetVehicleIdentifier(), GetKeyId());
	}

	//------------------------------------------------------------------------------------------------
	//! \param user The character to search.
	//! \return True when the user holds a matching key in the left-hand gadget slot or anywhere in
	//! their inventory, including nested containers.
	bool UserHasValidKey(IEntity user)
	{
		if (!user)
			return false;

		// A service vehicle takes no key: being on duty as police is the key (lock, U key, engine start).
		if (m_bServiceVehicle)
			return IsServiceDriver(user);

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
