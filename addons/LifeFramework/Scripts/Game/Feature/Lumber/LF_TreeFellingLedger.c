//------------------------------------------------------------------------------------------------
//! The felled trees and when each stands again (#3259). Pure arithmetic on a caller clock in whole
//! seconds, so a test drives time without a world. The server owns one (#3260); replication (#3261)
//! sends its keys and the save (#3262) writes its keys with the seconds left. Keys are
//! LF_TreeFelling.KeyOf.
class LF_TreeFellingLedger : Managed
{
	protected ref map<string, int> m_mRegrowAt = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	//! Records a fell. False (nothing changed) when the tree is already down.
	bool Fell(string key, int nowS, int regrowS)
	{
		if (IsFelled(key, nowS))
			return false;

		m_mRegrowAt.Set(key, nowS + LF_Utils.MaxInt(regrowS, 1));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool IsFelled(string key, int nowS)
	{
		int at;
		if (!m_mRegrowAt.Find(key, at))
			return false;

		return nowS < at;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds until the tree stands again; 0 when it stands.
	int SecondsLeft(string key, int nowS)
	{
		int at;
		if (!m_mRegrowAt.Find(key, at))
			return 0;

		return LF_Utils.MaxInt(at - nowS, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Removes every tree whose time has come and returns their keys (to show them again).
	array<string> TakeRegrown(int nowS)
	{
		array<string> regrown = {};
		foreach (string key, int at : m_mRegrowAt)
		{
			if (nowS >= at)
				regrown.Insert(key);
		}

		foreach (string key : regrown)
		{
			m_mRegrowAt.Remove(key);
		}

		return regrown;
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_mRegrowAt.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The keys still down, for a snapshot (late joiner, save).
	array<string> Keys()
	{
		array<string> keys = {};
		foreach (string key, int at : m_mRegrowAt)
		{
			keys.Insert(key);
		}

		return keys;
	}

	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		m_mRegrowAt.Clear();
	}
}
