//------------------------------------------------------------------------------------------------
//! The server's property book: one LF_PropertyRecord per house or plot, keyed by its stable id, and
//! the bridge to the save ("properties" payload; #2646, spec #756).
//!
//! WAITING ROWS. A row is kept whether or not its property is in the world, and saved again, so a
//! map edit that removes a house or plot for a while does not lose it. Nothing here holds an
//! entity: a house is looked up by identifier (LF_Houses) when its owner is read or set.
//!
//! OWNER. A house's owner of record is in "houses" (LF_Houses, LF_HouseManagerComponent.GetOwnerKey);
//! GetOwnerKey reads it from there and SetOwner sets it on a house in the world too. A plot's owner
//! is this book's row.
//------------------------------------------------------------------------------------------------
class LF_Properties
{
	static const string FEATURE = "Property";

	protected static ref map<string, ref LF_PropertyRecord> s_mRecords;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_PropertyRecord> Records()
	{
		if (!s_mRecords)
			s_mRecords = new map<string, ref LF_PropertyRecord>();

		return s_mRecords;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mRecords = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the row of a property, made empty when there is none yet.
	//! \param propertyId The house identifier or the plot key.
	//! \return The stored row, null for an empty id.
	static LF_PropertyRecord Create(string propertyId)
	{
		if (propertyId.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "property row refused: no property id");
			return null;
		}

		LF_PropertyRecord record = Records().Get(propertyId);
		if (record)
			return record;

		record = LF_PropertyRecord.Create(propertyId);
		Records().Set(propertyId, record);
		LF_Debug.Info(FEATURE, string.Format("property %1 recorded", propertyId));
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The stored row of a property, null when none.
	static LF_PropertyRecord Get(string propertyId)
	{
		return Records().Get(propertyId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sets the build stage of a known property.
	//! \return False when the property has no row.
	static bool SetStage(string propertyId, int stage)
	{
		LF_PropertyRecord record = Records().Get(propertyId);
		if (!record)
		{
			LF_Debug.Warn(FEATURE, string.Format("set stage refused: no property %1", propertyId));
			return false;
		}

		record.m_iBuildStage = stage;
		LF_Debug.Info(FEATURE, string.Format("property %1 at build stage %2", propertyId, stage));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sets or clears (empty key) the owner of a known property. A house in the world gets the
	//! owner on the house itself, which saves it in "houses".
	//! \return False when the property has no row.
	static bool SetOwner(string propertyId, string ownerKey)
	{
		LF_PropertyRecord record = Records().Get(propertyId);
		if (!record)
		{
			LF_Debug.Warn(FEATURE, string.Format("set owner refused: no property %1", propertyId));
			return false;
		}

		if (!record.m_sOwnerKey.IsEmpty() && record.m_sOwnerKey != ownerKey)
			record.m_bOwnerSpawnsHere = false; // a new owner does not inherit the spawn choice (#2651)

		record.m_sOwnerKey = ownerKey;
		LF_HouseManagerComponent house = LF_Houses.Find(propertyId);
		if (house)
			house.SetOwner(ownerKey);

		LF_Debug.Info(FEATURE, string.Format("property %1 owner set to '%2'", propertyId, ownerKey));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The owner's character key: the house's when the id is a known house, else the row's;
	//! empty when nobody owns it or it is unknown.
	static string GetOwnerKey(string propertyId)
	{
		LF_HouseManagerComponent house = LF_Houses.Find(propertyId);
		if (house)
			return house.GetOwnerKey();

		LF_HouseLockRecord houseRow = LF_Houses.Get(propertyId);
		if (houseRow)
			return houseRow.m_sOwnerKey;

		LF_PropertyRecord record = Records().Get(propertyId);
		if (record)
			return record.m_sOwnerKey;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The "properties" payload: one row per known property in id order, so two saves of the same
	//! state are the same bytes. A house's row carries the house's current owner.
	static array<ref LF_PropertyRecord> ExportAll()
	{
		array<ref LF_PropertyRecord> rows = new array<ref LF_PropertyRecord>();
		array<string> ids = {};
		foreach (string id, LF_PropertyRecord record : Records())
		{
			ids.Insert(id);
		}

		ids.Sort();
		foreach (string sortedId : ids)
		{
			LF_PropertyRecord row = Records().Get(sortedId).Copy();
			string liveOwner = GetOwnerKey(sortedId);
			if (row.m_sOwnerKey != liveOwner)
				row.m_bOwnerSpawnsHere = false; // a sold house is not the new owner's spawn (#2651)

			row.m_sOwnerKey = liveOwner;
			rows.Insert(row);
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the book with saved rows. Idempotent: applying the same rows twice ends in the same
	//! state. A row with no id is skipped; a later row with the same id replaces the earlier one.
	//! \return Number of rows kept.
	static int ApplyAll(array<ref LF_PropertyRecord> rows)
	{
		s_mRecords = new map<string, ref LF_PropertyRecord>();
		if (!rows)
			return 0;

		foreach (LF_PropertyRecord row : rows)
		{
			if (!row || row.m_sPropertyId.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "saved property row skipped: no property id");
				continue;
			}

			s_mRecords.Set(row.m_sPropertyId, row.Copy());
		}

		return s_mRecords.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Properties' statics (LF_StaticReset finds it by reflection).
class LF_Properties_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_Properties";
	}

	override void Apply()
	{
		LF_Properties.ResetStatics();
	}
}
