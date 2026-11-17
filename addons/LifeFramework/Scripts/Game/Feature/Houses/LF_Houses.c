//------------------------------------------------------------------------------------------------
//! The server's house book: lock state and owner of every house with a stable identifier, and the
//! bridge to the save ("houses" payload, #2410).
//!
//! A house (LF_HouseManagerComponent) registers when it has a stable identifier. A saved row that
//! arrives before its house waits here and is applied when the house registers, and the other way
//! round, so load order never matters. A row whose house never appears is kept and saved again, so
//! a map edit that removes a house for a while does not lose its owner.
//!
//! CHARACTER KEYS. CharacterKeyOf is the one rule for "who is this character" at a house door, on
//! the server and on a client: the character record key LF_Utils.GetPlayerUID returns (replicated
//! to clients), or a key a test bound to an entity no player controls.
//------------------------------------------------------------------------------------------------
class LF_Houses
{
	static const string FEATURE = "Houses";

	protected static ref map<string, ref LF_HouseLockRecord> s_mRecords;
	protected static ref map<EntityID, string> s_mBoundCharacters;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_HouseLockRecord> Records()
	{
		if (!s_mRecords)
			s_mRecords = new map<string, ref LF_HouseLockRecord>();

		return s_mRecords;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, LF_HouseManagerComponent> Live()
	{
		return LF_HouseManagerComponent.Live();
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mRecords = null;
		LF_HouseManagerComponent.ResetLive();
		s_mBoundCharacters = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \param user The character at a house door.
	//! \return The character key ownership is checked against, empty when it is not known.
	static string CharacterKeyOf(IEntity user)
	{
		if (!user)
			return "";

		string key = LF_Utils.GetPlayerUID(user);
		if (!key.IsEmpty())
			return key;

		if (s_mBoundCharacters && s_mBoundCharacters.Contains(user.GetID()))
			return s_mBoundCharacters.Get(user.GetID());

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Test hook: gives an entity no player controls a character key, so a fixture can stand in for
	//! an owner.
	static void BindCharacter(IEntity entity, string characterKey)
	{
		if (!entity)
			return;

		if (!s_mBoundCharacters)
			s_mBoundCharacters = new map<EntityID, string>();

		s_mBoundCharacters.Set(entity.GetID(), characterKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a house with a stable identifier appeared. A saved row that arrived first is applied;
	//! otherwise the house's own state is recorded.
	//! eturn True when the house is registered (its state is saved); false for a second house with
	//! an identifier already in use, which is then not saved.
	static bool Register(LF_HouseManagerComponent house)
	{
		if (!house || !Replication.IsServer())
			return false;

		string id = house.GetHouseIdentifier();
		if (id.IsEmpty())
			return false;

		LF_HouseManagerComponent known = Live().Get(id);
		if (known && known != house)
		{
			LF_Debug.Error(FEATURE, string.Format("house identifier %1 is used twice; the second house is not saved", id));
			return false;
		}

		Live().Set(id, house);

		LF_HouseLockRecord saved = Records().Get(id);
		if (saved)
		{
			house.ApplyLockState(saved);
			return true;
		}

		Remember(house.ExportLockState());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a house is gone. Its row stays, so its state is saved and comes back with it.
	static void Unregister(LF_HouseManagerComponent house)
	{
		if (house)
			house.RemoveFromLive();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stores a house's state under its identifier. A row with no identifier is refused.
	//! \return True when the row was stored.
	static bool Remember(LF_HouseLockRecord record)
	{
		if (!record || record.m_sHouseIdentifier.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "house row refused: no house identifier");
			return false;
		}

		Records().Set(record.m_sHouseIdentifier, record.Copy());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The stored row of a house, null when none.
	static LF_HouseLockRecord Get(string houseIdentifier)
	{
		return Records().Get(houseIdentifier);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The live house registered under the identifier, null when it is not in the world.
	static LF_HouseManagerComponent Find(string houseIdentifier)
	{
		return Live().Get(houseIdentifier);
	}

	//------------------------------------------------------------------------------------------------
	//! The "houses" payload: one row per known house, in identifier order so two saves of the same
	//! state are the same bytes.
	static array<ref LF_HouseLockRecord> ExportAll()
	{
		array<ref LF_HouseLockRecord> rows = new array<ref LF_HouseLockRecord>();
		array<string> ids = {};
		foreach (string id, LF_HouseLockRecord record : Records())
		{
			ids.Insert(id);
		}

		ids.Sort();
		foreach (string sortedId : ids)
		{
			rows.Insert(Records().Get(sortedId).Copy());
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the book with saved rows and applies each to its live house. Idempotent: applying the
	//! same rows twice ends in the same state. A live house the rows do not name keeps its state.
	//! \return Number of rows applied to a live house.
	static int ApplyAll(array<ref LF_HouseLockRecord> rows)
	{
		s_mRecords = new map<string, ref LF_HouseLockRecord>();
		int applied;
		if (rows)
		{
			foreach (LF_HouseLockRecord row : rows)
			{
				if (!Remember(row))
					continue;

				LF_HouseManagerComponent house = Live().Get(row.m_sHouseIdentifier);
				if (house && house.ApplyLockState(row))
					applied++;
			}
		}

		foreach (string id, LF_HouseManagerComponent house : Live())
		{
			if (house && !s_mRecords.Contains(id))
				Remember(house.ExportLockState());
		}

		return applied;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Houses' statics (registered at script load; Core/LF_StaticReset.c).
class LF_Houses_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_Houses_StaticResetHook());

	override string Owner()
	{
		return "LF_Houses";
	}

	override void Apply()
	{
		LF_Houses.ResetStatics();
	}
}
