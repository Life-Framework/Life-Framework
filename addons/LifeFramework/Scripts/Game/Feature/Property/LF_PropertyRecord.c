//------------------------------------------------------------------------------------------------
//! Persisted form of one property (a house or a plot): one row of the "properties" payload
//! (LF_PersistenceComponentSerializer, LF_Properties.ExportAll / ApplyAll; #2646, spec #756).
//!
//! STABLE KEY. m_sPropertyId is the house identifier (LF_HouseManagerComponent.GetHouseIdentifier)
//! or the plot key. Never coordinates and never a session-local EntityID: a row whose property is
//! not in the world waits in LF_Properties and is saved again.
//!
//! OWNER. For a house the owner of record lives in the "houses" payload (LF_Houses); this row keeps
//! a mirror that LF_Properties refreshes from the house. For a plot this row is the owner of record.
//!
//! CONTENTS of a house are not here: they ride LF_PersistentStorageComponent under "property:<id>".
//!
//! SHAPE. Append-only (docs/persistence.md "Record shapes"): a new field goes at the end and VERSION
//! goes up. The field order is the save order.
//------------------------------------------------------------------------------------------------
class LF_PropertyRecord : Managed
{
	static const int VERSION = 1;

	string m_sPropertyId;
	string m_sOwnerKey;
	string m_sOrgId;
	string m_sLayoutId;
	int m_iBuildStage;
	int m_iWorkDone;
	string m_sDeposited; // "prefab=count;..."
	int m_iPurchasedAt;
	int m_iRentPaidUntil; // 0 = owned outright
	bool m_bOwnerSpawnsHere;

	//------------------------------------------------------------------------------------------------
	//! \param propertyId The house identifier or the plot key.
	//! \return An empty record for that property.
	static LF_PropertyRecord Create(string propertyId)
	{
		LF_PropertyRecord record();
		record.m_sPropertyId = propertyId;
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! \return A copy, so a stored row never aliases a caller's record.
	LF_PropertyRecord Copy()
	{
		LF_PropertyRecord copy = Create(m_sPropertyId);
		copy.m_sOwnerKey = m_sOwnerKey;
		copy.m_sOrgId = m_sOrgId;
		copy.m_sLayoutId = m_sLayoutId;
		copy.m_iBuildStage = m_iBuildStage;
		copy.m_iWorkDone = m_iWorkDone;
		copy.m_sDeposited = m_sDeposited;
		copy.m_iPurchasedAt = m_iPurchasedAt;
		copy.m_iRentPaidUntil = m_iRentPaidUntil;
		copy.m_bOwnerSpawnsHere = m_bOwnerSpawnsHere;
		return copy;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when every field equals the other row's.
	bool SameAs(LF_PropertyRecord other)
	{
		if (!other)
			return false;

		if (m_sPropertyId != other.m_sPropertyId || m_sOwnerKey != other.m_sOwnerKey || m_sOrgId != other.m_sOrgId)
			return false;

		if (m_sLayoutId != other.m_sLayoutId || m_iBuildStage != other.m_iBuildStage || m_iWorkDone != other.m_iWorkDone)
			return false;

		if (m_sDeposited != other.m_sDeposited || m_iPurchasedAt != other.m_iPurchasedAt)
			return false;

		return m_iRentPaidUntil == other.m_iRentPaidUntil && m_bOwnerSpawnsHere == other.m_bOwnerSpawnsHere;
	}
}
