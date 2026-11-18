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
	static const int SHARE_COPY_RANGE = 1; // lf-fixed: #2623, a saved copy of a shared mark sits within 1 m of it

	//! Server: character key -> marks in placing order.
	protected static ref map<string, ref array<ref LF_MapMarkRow>> s_mMarks;
	//! Networked marker adds refused since the world started (both sides; tests read it).
	protected static int s_iNetworkedRefused;
	//! Server: the last restore generation handed out (session only, never saved). 0 is never issued.
	protected static int s_iLastGeneration;
	//! Server: player id -> the generation of that player's latest restore.
	protected static ref map<int, int> s_mGeneration;
	//! Server: player id -> the character key that restore was for.
	protected static ref map<int, string> s_mGenerationCharacter;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mMarks = null;
		s_iNetworkedRefused = 0;
		s_iLastGeneration = 0;
		s_mGeneration = null;
		s_mGenerationCharacter = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a new restore generation for this player's character (spawn, respawn, character
	//! switch). The client echoes it on its next upload; an upload stamped with an older generation was
	//! queued before the switch and holds the previous character's marks (#2546). Numbers are unique
	//! across players for the session.
	static int IssueGeneration(int playerId, string characterId)
	{
		if (!s_mGeneration)
			s_mGeneration = new map<int, int>();
		if (!s_mGenerationCharacter)
			s_mGenerationCharacter = new map<int, string>();

		s_iLastGeneration++;
		s_mGeneration.Set(playerId, s_iLastGeneration);
		s_mGenerationCharacter.Set(playerId, characterId);
		return s_iLastGeneration;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player's current restore generation, 0 before the first restore.
	static int GenerationOf(int playerId)
	{
		if (!s_mGeneration)
			return 0;

		int generation;
		s_mGeneration.Find(playerId, generation);
		return generation;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player's uploaded mark list, saved under `characterId` only when `generation` is that
	//! player's current restore generation and that restore was for the same character. Anything else
	//! is an upload the client queued before a character switch (or before its first restore) and is
	//! dropped with a log line, so one character's marks never land under another's key (#2546).
	//! \return Marks kept, or -1 when the list was dropped.
	static int SetFromPlayer(int playerId, string characterId, int generation, array<ref LF_MapMarkRow> rows)
	{
		int current = GenerationOf(playerId);
		if (current == 0 || generation != current)
		{
			LF_Debug.Warn(FEATURE, string.Format("mark list dropped: player %1 sent generation %2, current is %3 (queued before a character switch)", playerId, generation, current));
			return -1;
		}

		string issuedFor;
		s_mGenerationCharacter.Find(playerId, issuedFor);
		if (issuedFor != characterId)
		{
			LF_Debug.Warn(FEATURE, string.Format("mark list dropped: player %1 generation %2 was for character '%3', now '%4'", playerId, generation, issuedFor, characterId));
			return -1;
		}

		return SetCharacter(characterId, rows);
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
	//! Server: drops the character's marks that are saved copies of a live share the character sees now
	//! (same text, within SHARE_COPY_RANGE metres). Before the review fix (#2495 finding 9) shared marks
	//! were uploaded as personal marks, so old "mapmarks" rows hold org and dispatch marks that came back
	//! as ordinary deletable marks. The store keeps the cleaned list, so the next save writes it. Copies
	//! of shares that already expired cannot be matched and stay (lead decision #2623; wipes clear them).
	//! \return Marks dropped.
	static int DropShareCopies(string characterId)
	{
		array<ref LF_MapMarkRow> marks = Character(characterId, false);
		if (!marks || marks.IsEmpty())
			return 0;

		array<ref LF_MapShareRow> shares = {};
		LF_MapShares.VisibleToCharacter(characterId, shares);
		if (shares.IsEmpty())
			return 0;

		int dropped;
		for (int i = marks.Count() - 1; i >= 0; i--)
		{
			if (!IsShareCopy(marks[i], shares))
				continue;

			marks.RemoveOrdered(i);
			dropped++;
		}

		if (dropped > 0)
			LF_Debug.Info(FEATURE, string.Format("restore dropped %1 saved copy(ies) of shared marks for %2", dropped, characterId));

		return dropped;
	}

	//------------------------------------------------------------------------------------------------
	//! A mark is a copy of a share when it lies within SHARE_COPY_RANGE metres of it and its text is the
	//! share's, or the share's localization key as the client showed it.
	protected static bool IsShareCopy(LF_MapMarkRow row, notnull array<ref LF_MapShareRow> shares)
	{
		if (!row)
			return false;

		foreach (LF_MapShareRow share : shares)
		{
			int dx = row.m_iPosX - share.m_iX;
			int dz = row.m_iPosY - share.m_iZ;
			if (dx * dx + dz * dz > SHARE_COPY_RANGE * SHARE_COPY_RANGE)
				continue;

			if (row.m_sText == share.m_sText)
				return true;

			if (share.m_sText.StartsWith("#") && row.m_sText == WidgetManager.Translate(share.m_sText))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a character's marks on the wire, for the spawn restore. Saved copies of live shares are
	//! dropped first (DropShareCopies).
	static int ExportCharacter(string characterId, notnull array<int> ints, notnull array<string> texts)
	{
		DropShareCopies(characterId);
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
	//! Server: may this player's mark sync or save go ahead? Its own rate bucket (LF_ERequestKind.MAP), not
	//! the shared COMMS one: three phone or board asks in five seconds used to drop the respawn's restore
	//! or the last save, and the next upload then overwrote the stored marks with the client's (maybe
	//! empty) map (review 2026-11-17).
	static bool AllowAsk(int playerId, string request, bool notify = true)
	{
		return LF_RateLimit.Allow(playerId, LF_ERequestKind.MAP, request, notify);
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
//! A read-only view of the share table for the mark restore (#2623). It lives here, beside its only
//! user, so the share table's own file stays as it is.
modded class LF_MapShares
{
	//------------------------------------------------------------------------------------------------
	//! Server: the live shares the player on this character sees now, in put order; none when the
	//! character is not on the server. The rows are the table's own: read them, never change them.
	static void VisibleToCharacter(string characterId, notnull array<ref LF_MapShareRow> outRows)
	{
		outRows.Clear();
		if (characterId.IsEmpty() || !s_mRows || !s_aOrder)
			return;

		array<int> players = {};
		Facts().Players(players);
		foreach (int playerId : players)
		{
			LF_MapShareMember member = Facts().Of(playerId);
			if (!member || member.m_sUid != characterId)
				continue;

			foreach (string key : s_aOrder)
			{
				LF_MapShareRow row = s_mRows.Get(key);
				if (row && LF_MapShareRules.InAudience(row.m_eAudience, row.m_sAudienceId, member))
					outRows.Insert(row);
			}

			return;
		}
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the mark store (LF_StaticReset finds it by reflection).
class LF_MapMarks_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MapMarks";
	}

	override void Apply()
	{
		LF_MapMarks.ResetStatics();
	}
}
