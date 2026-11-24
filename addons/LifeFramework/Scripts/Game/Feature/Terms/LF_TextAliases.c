//------------------------------------------------------------------------------------------------
//! Per-mission text aliases (#4676): a mission header lists rows "FROM=TO" (string keys, no #), and
//! LF_Terms.Text shows TO wherever a screen, toast or guide asks for FROM. It is how one mission
//! whose world lacks a place the shared text names (the playtest town has no hostel) points that one
//! key at its own wording, without touching the text another mission shows. Core code holds no place
//! name: the rows, and the replacement strings, are the pack's.
class LF_TextAliases
{
	static const string SEPARATOR = "=";

	//! The header's rows, parsed once. Not latched while there is no header to read.
	protected static ref map<string, string> s_Map;
	protected static bool s_bLoaded;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Map = null;
		s_bLoaded = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the key a row list maps `key` to, or `key` itself. Keys compare with or without the leading #.
	//! \param rows "FROM=TO" rows (LF_MissionHeader.m_aLF_TextAliases); null or empty maps nothing.
	//! \param key The string key asked for, "#Quest_Foo" or "Quest_Foo".
	//! \return The aliased key in the same spelling as `key` (with the # when `key` had it).
	static string ResolveIn(array<string> rows, string key)
	{
		if (!rows || rows.IsEmpty() || key.IsEmpty())
			return key;

		bool hash = key.Get(0) == "#";
		string bare = key;
		if (hash)
			bare = key.Substring(1, key.Length() - 1);

		foreach (string row : rows)
		{
			int cut = row.IndexOf(SEPARATOR);
			if (cut <= 0)
				continue;

			string fromKey = row.Substring(0, cut);
			if (fromKey != bare)
				continue;

			string toKey = row.Substring(cut + 1, row.Length() - cut - 1);
			if (toKey.IsEmpty())
				return key;

			if (hash)
				return "#" + toKey;

			return toKey;
		}

		return key;
	}

	//------------------------------------------------------------------------------------------------
	//! \return `key` as the running mission's header aliases it, or `key` unchanged.
	static string Resolve(string key)
	{
		if (key.IsEmpty() || !GetGame())
			return key;

		if (!s_bLoaded)
		{
			LF_MissionHeader header = LF_MissionHeader.GetCurrent();
			if (!header)
				return key;

			s_bLoaded = true;
			if (header.m_aLF_TextAliases && !header.m_aLF_TextAliases.IsEmpty())
			{
				s_Map = new map<string, string>();
				foreach (string row : header.m_aLF_TextAliases)
				{
					int cut = row.IndexOf(SEPARATOR);
					if (cut > 0 && cut < row.Length() - 1)
						s_Map.Set(row.Substring(0, cut), row.Substring(cut + 1, row.Length() - cut - 1));
				}
			}
		}

		if (!s_Map)
			return key;

		bool hash = key.Get(0) == "#";
		string bare = key;
		if (hash)
			bare = key.Substring(1, key.Length() - 1);

		string toKey;
		if (!s_Map.Find(bare, toKey))
			return key;

		if (hash)
			return "#" + toKey;

		return toKey;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_TextAliases' statics (LF_StaticReset finds it by reflection).
class LF_TextAliases_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_TextAliases";
	}

	override void Apply()
	{
		LF_TextAliases.ResetStatics();
	}
}
