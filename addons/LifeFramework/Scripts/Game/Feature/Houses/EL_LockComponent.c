//------------------------------------------------------------------------------------------------
//! House door lock: a replicated locked flag plus a replicated house identifier that the
//! matching key must carry. The vanilla SCR_DoorUserAction gate reads this component; see
//! EL_DoorUserAction.c.
//!
//! IDENTIFIER. A door's identifier is the house it belongs to. Editor-set m_sDebugIdentifier
//! binds a whole prefab lineage (the DebugWorld fixtures); a house's EL_HouseManagerComponent
//! syncs its identifier onto every door it owns at spawn and on lock changes. An empty
//! identifier matches nothing, so an unbound door cannot be opened by a stray key.
//!
//! REPLICATION. Both RplProps are written on the server only (the manager sync and the
//! editor attributes), so a client never invents lock state. BumpMe is throttled: only when a
//! value actually changes. A changed flag does not broadcast itself; without BumpMe a locked
//! door would still read as open on every client.
//!
//! KEY MATCHING. IsValidKey reuses the vehicle-lock matching rule
//! (EL_VehicleLockComponent.IdentifiersMatch): both identifiers non-empty and equal. One rule,
//! one implementation, shared by vehicles and houses.
//!
//! PERSISTENCE GAP. Lock state is in-session until the house save/load contract in
//! EL_HouseSaveData.c is wired into a serializer; see that file.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/Houses", description: "Locks a house door to players holding a matching key.")]
class EL_LockComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_LockComponent : ScriptComponent
{
	[Attribute("", desc: "Editor binding: house identifier this door belongs to. Leave empty to sync from the house manager.")]
	protected string m_sDebugIdentifier;

	[Attribute("0", desc: "Editor binding: start the door locked at spawn.")]
	protected bool m_bDebugLocked;

	[RplProp()]
	protected bool m_bLocked;

	[RplProp()]
	protected string m_sHouseIdentifier;

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		// Direct field writes: the initial RplProp values broadcast on registration, so BumpMe
		// (which marks a CHANGE) is not needed here. Same pattern as EL_VehicleLockComponent.
		if (Replication.IsServer())
		{
			if (!m_sDebugIdentifier.IsEmpty())
				m_sHouseIdentifier = m_sDebugIdentifier;

			m_bLocked = m_bDebugLocked;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the door is locked.
	bool IsLocked()
	{
		return m_bLocked;
	}

	//------------------------------------------------------------------------------------------------
	void Lock()
	{
		SetLocked(true);
	}

	//------------------------------------------------------------------------------------------------
	void Unlock()
	{
		SetLocked(false);
	}

	//------------------------------------------------------------------------------------------------
	void ToggleLock()
	{
		SetLocked(!m_bLocked);
	}

	//------------------------------------------------------------------------------------------------
	//! Server-authoritative: only the server changes lock state. The door action gate reads this
	//! state on client and server; a client write would only corrupt its own proxy.
	//! \param locked The new lock state.
	void SetLocked(bool locked)
	{
		if (!Replication.IsServer())
			return;

		if (m_bLocked == locked)
			return;

		m_bLocked = locked;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The identifier of the house this door belongs to, empty when unbound.
	string GetHouseIdentifier()
	{
		return m_sHouseIdentifier;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-authoritative: only the house manager (or the server) assigns a house identifier.
	//! \param identifier The house identifier the matching key must carry.
	void SetHouseIdentifier(string identifier)
	{
		if (!Replication.IsServer())
			return;

		if (m_sHouseIdentifier == identifier)
			return;

		m_sHouseIdentifier = identifier;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! \param key The item to test.
	//! \return True when the key carries an EL_KeyComponent whose identifier matches this door's
	//! house identifier.
	bool IsValidKey(IEntity key)
	{
		if (!key)
			return false;

		EL_KeyComponent keyComponent = EL_Component<EL_KeyComponent>.Find(key);
		if (!keyComponent)
			return false;

		bool matches = EL_VehicleLockComponent.IdentifiersMatch(keyComponent.GetHouseIdentifier(), m_sHouseIdentifier);
		EL_Debug.Log("Houses", string.Format("key check: key=%1 house=%2 match=%3",
			keyComponent.GetHouseIdentifier(), m_sHouseIdentifier, matches));
		return matches;
	}
}