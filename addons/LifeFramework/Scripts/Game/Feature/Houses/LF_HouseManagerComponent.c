//------------------------------------------------------------------------------------------------
//! House manager: owns a house's identifier, lock state and owner, and mirrors them onto every door
//! of the building that carries an LF_LockComponent.
//!
//! IDENTIFIER. The editor identifier m_sDebugIdentifier (or one bound at runtime with
//! BindIdentifier) is stable: the house registers with LF_Houses and its lock state and owner are
//! saved under it. A house without one gets a PersistenceIdUtils id at spawn so its doors and keys
//! still pair this session, but that id changes every boot, so its state is not saved (logged).
//! SyncDoors copies the identifier onto every door so a key bound to the house opens any of them.
//!
//! LOCK STATE AND OWNER. m_bHouseLocked and m_sOwnerKey are the authoritative house-wide state;
//! each door's LF_LockComponent mirrors them (the door action reads the door's copy, which
//! replicates). Opening a door never unlocks it (#2410): only SetHouseLocked changes the lock, and
//! it re-syncs the doors even when the state did not change, so a door a breach or a lockpick
//! opened converges again on the next lock.
//!
//! PERSISTENCE. ExportLockState / ApplyLockState are the save/load API LF_Houses calls for the
//! "houses" payload (LF_HouseSaveData.c). ApplyLockState is idempotent and rejects a record for
//! another house.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/Houses", description: "Owns a house's lock state and syncs its doors.")]
class LF_HouseManagerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_HouseManagerComponent : ScriptComponent
{
	[Attribute("", desc: "Editor binding: house identifier. Leave empty to auto-generate at spawn.")]
	protected string m_sDebugIdentifier;

	[Attribute("0", desc: "Editor binding: the house starts locked. A door of the house locked in the editor also starts it locked.")]
	protected bool m_bStartLocked;

	[RplProp()]
	protected string m_sHouseIdentifier;

	[RplProp()]
	protected bool m_bHouseLocked;

	//! Character key of the owner (LF_Houses.CharacterKeyOf), empty when nobody owns the house.
	[RplProp()]
	protected string m_sOwnerKey;

	//! True when the identifier survives a restart (editor-set or bound), so the state is saved.
	protected bool m_bStableIdentifier;

	//! Live houses with a stable identifier, by identifier (LF_Houses registers them). OnDelete removes.
	protected static ref map<string, LF_HouseManagerComponent> s_mLive;

	//------------------------------------------------------------------------------------------------
	static map<string, LF_HouseManagerComponent> Live()
	{
		if (!s_mLive)
			s_mLive = new map<string, LF_HouseManagerComponent>();

		return s_mLive;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetLive()
	{
		s_mLive = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes this house out of the live map when it is the one registered under its identifier.
	void RemoveFromLive()
	{
		if (s_mLive && s_mLive.Get(m_sHouseIdentifier) == this)
			s_mLive.Remove(m_sHouseIdentifier);
	}

	//------------------------------------------------------------------------------------------------
	//! A ScriptComponent gets EOnInit only when it asks for it. Without this a house never got an
	//! identifier and never synced its doors, so no key could match them.
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		if (!Replication.IsServer())
			return;

		if (!m_sDebugIdentifier.IsEmpty())
		{
			m_sHouseIdentifier = m_sDebugIdentifier;
			m_bStableIdentifier = true;
		}
		else if (m_sHouseIdentifier.IsEmpty())
		{
			m_sHouseIdentifier = PersistenceIdUtils.Generate();
			LF_Debug.Info("Houses", string.Format("house %1 has no editor identifier: its lock state and owner are not saved across a restart", m_sHouseIdentifier));
		}

		// The editor's lock is the house's starting state, so the first sync, a load or a purchase
		// never unlocks doors the map maker locked. Direct write: the initial value replicates.
		if (m_bStartLocked || AnyDoorStartsLocked(owner))
			m_bHouseLocked = true;

		SyncDoors();
		if (m_bStableIdentifier)
			RegisterStable();
	}

	//------------------------------------------------------------------------------------------------
	//! Registers with LF_Houses; a refused house (its identifier is already used) is not saved.
	protected void RegisterStable()
	{
		m_bStableIdentifier = true;
		if (!LF_Houses.Register(this))
			m_bStableIdentifier = false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool AnyDoorStartsLocked(IEntity parent)
	{
		IEntity child = parent.GetChildren();
		while (child)
		{
			LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(child);
			if (doorLock && (doorLock.StartsLocked() || doorLock.IsLocked()))
				return true;

			if (AnyDoorStartsLocked(child))
				return true;

			child = child.GetSibling();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_mLive && s_mLive.Get(m_sHouseIdentifier) == this)
			s_mLive.Remove(m_sHouseIdentifier);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gives the house a stable identifier at runtime (a purchase flow or a pack script that
	//! binds houses by place), so its state is saved under it. A saved row for it is applied.
	//! \param identifier The stable house identifier; empty is refused.
	//! \return True when the house now carries the identifier.
	bool BindIdentifier(string identifier)
	{
		if (!Replication.IsServer())
			return false;

		if (identifier.IsEmpty())
		{
			LF_Debug.Warn("Houses", "bind identifier refused: empty identifier");
			return false;
		}

		if (m_bStableIdentifier && m_sHouseIdentifier == identifier)
			return true;

		if (m_bStableIdentifier)
			LF_Houses.Unregister(this);

		m_sHouseIdentifier = identifier;
		m_bStableIdentifier = false;
		Replication.BumpMe();
		SyncDoors();
		RegisterStable();
		if (!m_bStableIdentifier)
			return false;

		LF_Debug.Info("Houses", string.Format("house bound to identifier %1", identifier));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the identifier survives a restart and the house's state is saved.
	bool HasStableIdentifier()
	{
		return m_bStableIdentifier;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The identifier of this house, empty when unassigned.
	string GetHouseIdentifier()
	{
		return m_sHouseIdentifier;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the house is locked.
	bool IsHouseLocked()
	{
		return m_bHouseLocked;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The owner's character key, empty when nobody owns the house.
	string GetOwnerKey()
	{
		return m_sOwnerKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-authoritative: sets or clears (empty key) the owner. The owner passes every locked door
	//! of the house without a key. Saved with the house.
	//! \param ownerKey Character key of the new owner, empty for none.
	void SetOwner(string ownerKey)
	{
		if (!Replication.IsServer())
			return;

		if (m_sOwnerKey == ownerKey)
			return;

		m_sOwnerKey = ownerKey;
		Replication.BumpMe();
		SyncDoors();
		RememberState();
		LF_Debug.Info("Houses", string.Format("house %1 owner -> '%2'", m_sHouseIdentifier, ownerKey));
	}

	//------------------------------------------------------------------------------------------------
	void LockAllDoors()
	{
		if (LF_Systems.Refuses(LF_LockComponent.SYSTEM, "lock all doors"))
			return;

		SetHouseLocked(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-authoritative: only the server changes the house lock state.
	//! \param locked The new house-wide lock state.
	void SetHouseLocked(bool locked)
	{
		if (!Replication.IsServer())
			return;

		if (m_bHouseLocked == locked)
		{
			// Same house state: a door may still differ (a breach or a lockpick opened it), so the
			// doors converge again without a broadcast of the house flag.
			SyncDoors();
			return;
		}

		m_bHouseLocked = locked;
		Replication.BumpMe();
		SyncDoors();
		RememberState();

		string stateName = "unlocked";
		if (locked)
			stateName = "locked";
		LF_Debug.Log("Houses", string.Format("house lock -> %1", stateName));
	}

	//------------------------------------------------------------------------------------------------
	//! Mirrors the house identifier and lock state onto every door lock in the hierarchy.
	void SyncDoors()
	{
		SyncDoorsRecursive(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	protected void SyncDoorsRecursive(IEntity parent)
	{
		IEntity child = parent.GetChildren();
		while (child)
		{
			SyncDoorsRecursive(child);

			LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(child);
			if (doorLock)
			{
				doorLock.SetHouseIdentifier(m_sHouseIdentifier);
				doorLock.SetLocked(m_bHouseLocked);
				doorLock.SetOwnerKey(m_sOwnerKey);
			}

			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the saved row follows a change of a house with a stable identifier.
	protected void RememberState()
	{
		if (m_bStableIdentifier)
			LF_Houses.Remember(ExportLockState());
	}

	//------------------------------------------------------------------------------------------------
	//! \return A record of this house's lock state and owner, ready for a serializer to persist.
	LF_HouseLockRecord ExportLockState()
	{
		return LF_HouseLockRecord.Create(m_sHouseIdentifier, m_bHouseLocked, m_sOwnerKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-applies a saved record to this house. Idempotent: applying the same record twice
	//! converges to the same state. A record for a different house (or an unbound record) is
	//! rejected and the live state is left untouched.
	//! \param record The saved lock record.
	//! \return True when the record matched this house and was applied.
	bool ApplyLockState(LF_HouseLockRecord record)
	{
		if (!record || !record.IsValidFor(m_sHouseIdentifier))
		{
			LF_Debug.Warn("Houses", "apply-lock-state rejected: record missing or identifier mismatch");
			return false;
		}

		SetOwner(record.m_sOwnerKey);
		SetHouseLocked(record.m_bLocked);
		LF_Debug.Log("Houses", string.Format("apply-lock-state accepted: house=%1 locked=%2 owner='%3'", m_sHouseIdentifier, record.m_bLocked, record.m_sOwnerKey));
		return true;
	}
}
