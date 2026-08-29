//------------------------------------------------------------------------------------------------
//! Persisted form of a house's lock state.
//!
//! STABLE KEY. m_sHouseIdentifier identifies the house: editor-set on the building or generated
//! at spawn (see EL_HouseManagerComponent). Doors of the house carry the same identifier on
//! their EL_LockComponent, so a key bound to it matches every door. Never a session-local
//! EntityID: a restart re-resolves the building, and a record whose building is "not there" is
//! rejected by IsValidFor rather than applied to the wrong house.
//!
//! SAVE/LOAD CONTRACT. Export/Apply live on EL_HouseManagerComponent (public API):
//! ExportLockState() copies the live state into a record; ApplyLockState(record) re-applies it
//! idempotently through the manager's mutator and rejects records that do not match this house.
//!
//! VERSIONING follows the repo serializer pattern (see EL_PersistenceComponentSerializer): a
//! future ScriptedComponentSerializer writes VERSION first, then the record fields, positionally.
//! Binding that serializer into Configs/Systems/Persistence/LifeFramework.conf and proving a
//! real save-reload round trip (PERSISTENCE tier) is the follow-up; until then lock state is
//! in-session, same as the vehicle lock.
//------------------------------------------------------------------------------------------------
class EL_HouseLockRecord
{
	static const int VERSION = 1;

	string m_sHouseIdentifier;
	bool m_bLocked;

	//------------------------------------------------------------------------------------------------
	//! \param houseIdentifier The stable identifier of the house.
	//! \param locked The house-wide lock state.
	//! \return A fully populated house lock record.
	static EL_HouseLockRecord Create(string houseIdentifier, bool locked)
	{
		EL_HouseLockRecord record();
		record.m_sHouseIdentifier = houseIdentifier;
		record.m_bLocked = locked;
		return record;
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