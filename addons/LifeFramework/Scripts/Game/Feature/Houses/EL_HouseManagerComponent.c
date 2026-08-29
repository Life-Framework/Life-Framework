//------------------------------------------------------------------------------------------------
//! House manager: owns a house's identifier and lock state and mirrors them onto every door of
//! the building that carries an EL_LockComponent.
//!
//! IDENTIFIER. Generated at spawn (server only) with PersistenceIdUtils, or editor-set via
//! m_sDebugIdentifier to bind a whole prefab lineage. SyncDoors copies the identifier onto every
//! door so a key bound to the house opens any of its doors.
//!
//! LOCK STATE. m_bHouseLocked is the authoritative house-wide state; each door's
//! EL_LockComponent mirrors it (the door action reads the door's copy, which replicates).
//! SyncDoors runs at spawn for editor-placed doors and on every lock change, so doors added at
//! runtime converge on the next LockAllDoors/UnlockAllDoors.
//!
//! PERSISTENCE. ExportLockState / ApplyLockState are the public save/load API (see
//! EL_HouseSaveData.c). ApplyLockState is idempotent: re-applying the same record converges to
//! the same state, and a record whose identifier does not match this house is rejected rather
//! than applied to the wrong building.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/Houses", description: "Owns a house's lock state and syncs its doors.")]
class EL_HouseManagerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_HouseManagerComponent : ScriptComponent
{
	[Attribute("", desc: "Editor binding: house identifier. Leave empty to auto-generate at spawn.")]
	protected string m_sDebugIdentifier;

	[RplProp()]
	protected string m_sHouseIdentifier;

	[RplProp()]
	protected bool m_bHouseLocked;

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		if (Replication.IsServer())
		{
			if (!m_sDebugIdentifier.IsEmpty())
				m_sHouseIdentifier = m_sDebugIdentifier;
			else if (m_sHouseIdentifier.IsEmpty())
				m_sHouseIdentifier = PersistenceIdUtils.Generate();

			SyncDoors();
		}
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
	void LockAllDoors()
	{
		SetHouseLocked(true);
	}

	//------------------------------------------------------------------------------------------------
	void UnlockAllDoors()
	{
		SetHouseLocked(false);
	}

	//------------------------------------------------------------------------------------------------
	void ToggleHouseLocked()
	{
		SetHouseLocked(!m_bHouseLocked);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-authoritative: only the server changes the house lock state.
	//! \param locked The new house-wide lock state.
	void SetHouseLocked(bool locked)
	{
		if (!Replication.IsServer())
			return;

		if (m_bHouseLocked == locked)
			return;

		m_bHouseLocked = locked;
		Replication.BumpMe();
		SyncDoors();

		string stateName = "unlocked";
		if (locked)
			stateName = "locked";
		EL_Debug.Log("Houses", string.Format("house lock -> %1", stateName));
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

			EL_LockComponent doorLock = EL_Component<EL_LockComponent>.Find(child);
			if (doorLock)
			{
				doorLock.SetHouseIdentifier(m_sHouseIdentifier);
				doorLock.SetLocked(m_bHouseLocked);
			}

			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return A record of this house's lock state, ready for a serializer to persist.
	EL_HouseLockRecord ExportLockState()
	{
		return EL_HouseLockRecord.Create(m_sHouseIdentifier, m_bHouseLocked);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-applies a saved lock record to this house. Idempotent: applying the same record twice
	//! converges to the same state. A record for a different house (or an unbound record) is
	//! rejected and the live state is left untouched.
	//! \param record The saved lock record.
	//! \return True when the record matched this house and was applied.
	bool ApplyLockState(EL_HouseLockRecord record)
	{
		if (!record || !record.IsValidFor(m_sHouseIdentifier))
		{
			EL_Debug.Log("Houses", "apply-lock-state rejected: record missing or identifier mismatch");
			return false;
		}

		SetHouseLocked(record.m_bLocked);
		EL_Debug.Log("Houses", string.Format("apply-lock-state accepted: house=%1 locked=%2", m_sHouseIdentifier, record.m_bLocked));
		return true;
	}
}