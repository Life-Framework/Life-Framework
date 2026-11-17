//------------------------------------------------------------------------------------------------
//! One saved row of the "mapmarks" payload: one personal map mark of one character (card M8,
//! everon-map.md "Personal marks"). The key is the character record key (LF_Utils.GetPlayerUID),
//! never an entity or player id. The fields are the vanilla placed marker's own (SCR_MapMarkerBase):
//! m_iIcon and m_iColor index the lists in LF_MapMarkerConfig.conf, so those lists only ever grow
//! at the end.
class LF_MapMarkRow
{
	int m_iVersion = 1;
	string m_sCharacterId;
	int m_iType;
	int m_iPosX;
	int m_iPosY;
	int m_iIcon;
	int m_iColor;
	int m_iRotation;
	int m_iConfigId;
	int m_iFlags;
	string m_sText;

	//------------------------------------------------------------------------------------------------
	static LF_MapMarkRow Make(string characterId, int type, int posX, int posY, int icon, int color, int rotation, int configId, int flags, string text)
	{
		LF_MapMarkRow row = new LF_MapMarkRow();
		row.m_sCharacterId = characterId;
		row.m_iType = type;
		row.m_iPosX = posX;
		row.m_iPosY = posY;
		row.m_iIcon = icon;
		row.m_iColor = color;
		row.m_iRotation = rotation;
		row.m_iConfigId = configId;
		row.m_iFlags = flags;
		row.m_sText = text;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! A copy under another key, so the store never shares a row object with a caller.
	LF_MapMarkRow CopyFor(string characterId)
	{
		return Make(characterId, m_iType, m_iPosX, m_iPosY, m_iIcon, m_iColor, m_iRotation, m_iConfigId, m_iFlags, m_sText);
	}
}

//------------------------------------------------------------------------------------------------
//! The flat wire shape of a mark list: STRIDE ints per mark plus one text each. Owner RPCs carry
//! simple arrays only (AGENTS.md: no objects over RplProp; arrays of ints and strings over an RPC).
class LF_MapMarksWire
{
	static const int STRIDE = 8; // lf-fixed: wire layout, type x y icon color rotation config flags
	static const int TEXT_MAX = 64; // lf-fixed: payload sanity bound on a mark's note

	//------------------------------------------------------------------------------------------------
	static void Append(notnull LF_MapMarkRow row, notnull array<int> ints, notnull array<string> texts)
	{
		ints.Insert(row.m_iType);
		ints.Insert(row.m_iPosX);
		ints.Insert(row.m_iPosY);
		ints.Insert(row.m_iIcon);
		ints.Insert(row.m_iColor);
		ints.Insert(row.m_iRotation);
		ints.Insert(row.m_iConfigId);
		ints.Insert(row.m_iFlags);
		texts.Insert(row.m_sText);
	}

	//------------------------------------------------------------------------------------------------
	//! Rows from the wire, or null when the shape is broken (counts disagree).
	static array<ref LF_MapMarkRow> Read(string characterId, array<int> ints, array<string> texts)
	{
		array<ref LF_MapMarkRow> rows = new array<ref LF_MapMarkRow>();
		if (!ints || !texts)
			return rows;

		if (ints.Count() % STRIDE != 0 || ints.Count() / STRIDE != texts.Count())
			return null;

		for (int i = 0; i < texts.Count(); i++)
		{
			int at = i * STRIDE;
			string text = texts[i];
			if (text.Length() > TEXT_MAX)
				text = text.Substring(0, TEXT_MAX);

			rows.Insert(LF_MapMarkRow.Make(characterId, ints[at], ints[at + 1], ints[at + 2], ints[at + 3], ints[at + 4], ints[at + 5], ints[at + 6], ints[at + 7], text));
		}

		return rows;
	}
}

//------------------------------------------------------------------------------------------------
//! Personal map marks (card M8; decisions D-M7, D-M11). Vanilla's placed-marker dialog stays but
//! every mark is local to the player (LF_MapMarksSync.c refuses networked placing). Server: the
//! store, keyed by character record key, at most MAX_PER_CHARACTER marks each, saved as "mapmarks"
//! and kept with the characters across the 24 h world reset. Client: sends its whole mark list after
//! a change; the server sends it back on spawn and the client re-inserts the marks as local markers.
class LF_MapMarks
{
	static const string FEATURE = "MapMarks";
	static const int MAX_PER_CHARACTER = 20; // lf-fixed: D-M11, 20 personal marks per character

	//! Server: character key -> marks in placing order.
	protected static ref map<string, ref array<ref LF_MapMarkRow>> s_mMarks;
	//! Networked marker adds refused since the world started (both sides; tests read it).
	protected static int s_iNetworkedRefused;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mMarks = null;
		s_iNetworkedRefused = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_MapMarkRow> Character(string characterId, bool create)
	{
		if (!s_mMarks)
			s_mMarks = new map<string, ref array<ref LF_MapMarkRow>>();

		array<ref LF_MapMarkRow> marks;
		if (s_mMarks.Find(characterId, marks) || !create)
			return marks;

		marks = new array<ref LF_MapMarkRow>();
		s_mMarks.Set(characterId, marks);
		return marks;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: how many marks a character has.
	static int CountOf(string characterId)
	{
		array<ref LF_MapMarkRow> marks = Character(characterId, false);
		if (!marks)
			return 0;

		return marks.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one mark of a character, null past the end.
	static LF_MapMarkRow MarkOf(string characterId, int index)
	{
		array<ref LF_MapMarkRow> marks = Character(characterId, false);
		if (!marks || !marks.IsIndexValid(index))
			return null;

		return marks[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Server: adds one mark. Refused (logged) with no key or when the character already has
	//! MAX_PER_CHARACTER marks.
	static bool Add(string characterId, LF_MapMarkRow row)
	{
		if (characterId.IsEmpty() || !row)
		{
			LF_Debug.Warn(FEATURE, string.Format("mark refused: character '%1' sent no mark", characterId));
			return false;
		}

		array<ref LF_MapMarkRow> marks = Character(characterId, true);
		if (marks.Count() >= MAX_PER_CHARACTER)
		{
			LF_Debug.Warn(FEATURE, string.Format("mark refused: %1 already has %2 marks", characterId, MAX_PER_CHARACTER));
			return false;
		}

		marks.Insert(row.CopyFor(characterId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: replaces a character's marks (the client's upload after a change). Marks past the cap
	//! are refused one by one and logged; the first MAX_PER_CHARACTER are kept.
	//! \return Marks kept.
	static int SetCharacter(string characterId, array<ref LF_MapMarkRow> rows)
	{
		if (characterId.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "mark list refused: no character key");
			return 0;
		}

		if (!s_mMarks)
			s_mMarks = new map<string, ref array<ref LF_MapMarkRow>>();

		s_mMarks.Remove(characterId);
		if (!rows)
			return 0;

		int kept = 0;
		foreach (LF_MapMarkRow row : rows)
		{
			if (Add(characterId, row))
				kept++;
		}

		return kept;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a character's marks on the wire, for the spawn restore.
	static int ExportCharacter(string characterId, notnull array<int> ints, notnull array<string> texts)
	{
		array<ref LF_MapMarkRow> marks = Character(characterId, false);
		if (!marks)
			return 0;

		foreach (LF_MapMarkRow row : marks)
		{
			LF_MapMarksWire.Append(row, ints, texts);
		}

		return marks.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every row, for the "mapmarks" payload.
	static array<ref LF_MapMarkRow> ExportAll()
	{
		array<ref LF_MapMarkRow> rows = new array<ref LF_MapMarkRow>();
		if (!s_mMarks)
			return rows;

		foreach (string characterId, array<ref LF_MapMarkRow> marks : s_mMarks)
		{
			foreach (LF_MapMarkRow row : marks)
			{
				rows.Insert(row.CopyFor(characterId));
			}
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: replaces the store with saved rows (idempotent: applying the same rows twice gives the
	//! same store). Rows with no key are skipped; past the cap a character's extra rows are refused.
	//! \return Rows applied.
	static int ApplyAll(array<ref LF_MapMarkRow> rows)
	{
		s_mMarks = new map<string, ref array<ref LF_MapMarkRow>>();
		if (!rows)
			return 0;

		int applied = 0;
		foreach (LF_MapMarkRow row : rows)
		{
			if (!row || row.m_sCharacterId.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "load skipped a map mark row with no character");
				continue;
			}

			if (Add(row.m_sCharacterId, row))
				applied++;
		}

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! Both sides: a networked marker add was refused (D-M7: vanilla shared markers are off).
	static void RefuseNetworked(string where)
	{
		s_iNetworkedRefused++;
		LF_Debug.Warn(FEATURE, "networked marker refused (" + where + "): personal marks are local only");
	}

	//------------------------------------------------------------------------------------------------
	static int NetworkedRefusedCount()
	{
		return s_iNetworkedRefused;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the mark store (registered at script load; Core/LF_StaticReset.c).
class LF_MapMarks_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_MapMarks_StaticResetHook());

	override string Owner()
	{
		return "LF_MapMarks";
	}

	override void Apply()
	{
		LF_MapMarks.ResetStatics();
	}
}
