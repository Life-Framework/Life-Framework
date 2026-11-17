//------------------------------------------------------------------------------------------------
//! Persisted form of a house's lock state and owner: one row of the "houses" payload
//! (LF_PersistenceComponentSerializer, LF_Houses.ExportAll / ApplyAll).
//!
//! STABLE KEY. m_sHouseIdentifier identifies the house: the editor identifier set on the building
//! (LF_HouseManagerComponent.m_sDebugIdentifier) or one bound at runtime with BindIdentifier. Doors
//! of the house carry the same identifier on their LF_LockComponent, so a key bound to it matches
//! every door. Never a session-local EntityID: a restart re-resolves the building by identifier,
//! and a row whose building is "not there" waits in LF_Houses until it registers.
//!
//! OWNER. m_sOwnerKey is the owner's character key (LF_Houses.CharacterKeyOf), empty for none.
//!
//! SHAPE. Append-only (docs/persistence.md "Record shapes"): a new field goes at the end.
//------------------------------------------------------------------------------------------------
class LF_HouseLockRecord : Managed
{
	static const int VERSION = 1;

	string m_sHouseIdentifier;
	bool m_bLocked;
	string m_sOwnerKey;

	//------------------------------------------------------------------------------------------------
	//! \param houseIdentifier The stable identifier of the house.
	//! \param locked The house-wide lock state.
	//! \param ownerKey Character key of the owner, empty for none.
	//! \return A fully populated house lock record.
	static LF_HouseLockRecord Create(string houseIdentifier, bool locked, string ownerKey = "")
	{
		LF_HouseLockRecord record();
		record.m_sHouseIdentifier = houseIdentifier;
		record.m_bLocked = locked;
		record.m_sOwnerKey = ownerKey;
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! \return A copy, so a stored row never aliases a caller's record.
	LF_HouseLockRecord Copy()
	{
		return Create(m_sHouseIdentifier, m_bLocked, m_sOwnerKey);
	}

	//------------------------------------------------------------------------------------------------
	//! \param houseIdentifier The house this record would be applied to.
	//! \return True when both identifiers are non-empty and equal. An unbound record applies to
	//! no house.
	bool IsValidFor(string houseIdentifier)
	{
		if (m_sHouseIdentifier.IsEmpty() || houseIdentifier.IsEmpty())
			return false;

		return m_sHouseIdentifier == houseIdentifier;
	}
}
