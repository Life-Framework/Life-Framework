//------------------------------------------------------------------------------------------------
//! What one ask or one hear did to a character's knowledge of a place (card M13).
enum LF_EMapKnownOutcome
{
	VISITED,
	HEARD,
	//! Nothing changed: already known at least as well.
	KEPT,
	REFUSED_RATE,
	REFUSED_DARK,
	REFUSED_RANGE,
	//! A malformed ask (empty id, no character, a kind that is not a POI).
	REFUSED_BAD
}

//------------------------------------------------------------------------------------------------
//! The discovery rule as a pure function (docs/architecture/everon-map.md section 4, card M13). Its
//! node twin is tools/map/rules/discovery.mjs; both run Configs/Map/Rules/discovery.cases.json
//! (LF_Test_MapKnown), so they cannot drift.
class LF_MapKnownRules
{
	//! everon-map.md section 4: a place counts within 20 m unless its kind sets more.
	static const float DEFAULT_RADIUS = 20; // lf-fixed: the design's discovery radius, shared with the node twin and the case table
	//! Network and timing slack on top of the radius, so an honest client standing at the edge is not refused.
	static const float SLACK = 10; // lf-fixed: anti-cheat tolerance, shared with the node twin and the case table
	//! Asks a player may make in one window; more are refused unanswered.
	static const int ASKS_PER_WINDOW = 5; // lf-fixed: anti-flood bound, shared with the node twin and the case table
	static const int WINDOW_MS = 60000; // lf-fixed: see ASKS_PER_WINDOW

	//------------------------------------------------------------------------------------------------
	//! \param[in] visit True for a client's visit ask, false for a server-side hear.
	//! \param[in] known What the character knows of the place now.
	//! \param[in] live The place is registered in LF_Places now.
	//! \param[in] distance Metres from the character to the place (visits only).
	//! \param[in] radius The place's discovery radius; 0 or less is DEFAULT_RADIUS.
	//! \param[in] recent Asks this player made in the window before this one (visits only).
	static LF_EMapKnownOutcome Decide(bool visit, LF_EMapPoiKnown known, bool live, float distance, float radius, int recent)
	{
		if (!visit)
		{
			if (known == LF_EMapPoiKnown.NONE)
				return LF_EMapKnownOutcome.HEARD;

			return LF_EMapKnownOutcome.KEPT;
		}

		if (recent >= ASKS_PER_WINDOW)
			return LF_EMapKnownOutcome.REFUSED_RATE;

		if (!live)
			return LF_EMapKnownOutcome.REFUSED_DARK;

		float reach = radius;
		if (reach <= 0)
			reach = DEFAULT_RADIUS;

		if (distance > reach + SLACK)
			return LF_EMapKnownOutcome.REFUSED_RANGE;

		if (known == LF_EMapPoiKnown.VISITED)
			return LF_EMapKnownOutcome.KEPT;

		return LF_EMapKnownOutcome.VISITED;
	}

	//------------------------------------------------------------------------------------------------
	//! The case table's word for an outcome.
	static string OutcomeName(LF_EMapKnownOutcome outcome)
	{
		if (outcome == LF_EMapKnownOutcome.VISITED)
			return "visited";
		if (outcome == LF_EMapKnownOutcome.HEARD)
			return "heard";
		if (outcome == LF_EMapKnownOutcome.KEPT)
			return "kept";
		if (outcome == LF_EMapKnownOutcome.REFUSED_RATE)
			return "refused:rate";
		if (outcome == LF_EMapKnownOutcome.REFUSED_DARK)
			return "refused:dark";
		if (outcome == LF_EMapKnownOutcome.REFUSED_RANGE)
			return "refused:range";

		return "refused:bad";
	}

	//------------------------------------------------------------------------------------------------
	//! The saved number of a known state. Fixed values, so reordering LF_EMapPoiKnown never changes a save.
	static int SavedState(LF_EMapPoiKnown known)
	{
		if (known == LF_EMapPoiKnown.VISITED)
			return 2;
		if (known == LF_EMapPoiKnown.HEARD)
			return 1;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	static LF_EMapPoiKnown FromSaved(int saved)
	{
		if (saved == 2)
			return LF_EMapPoiKnown.VISITED;
		if (saved == 1)
			return LF_EMapPoiKnown.HEARD;

		return LF_EMapPoiKnown.NONE;
	}
}

//------------------------------------------------------------------------------------------------
//! One saved row of the "mapknown" payload: what one character knows of one place. The key is the
//! character record key (LF_Utils.GetPlayerUID) and the place id (LF_Places), never an entity id.
//! m_iState is 1 heard of, 2 visited (LF_MapKnownRules.SavedState).
class LF_MapKnownRow
{
	int m_iVersion = 1;
	string m_sCharacterId;
	string m_sPlaceId;
	int m_iState;

	//------------------------------------------------------------------------------------------------
	static LF_MapKnownRow Make(string characterId, string placeId, int state)
	{
		LF_MapKnownRow row = new LF_MapKnownRow();
		row.m_sCharacterId = characterId;
		row.m_sPlaceId = placeId;
		row.m_iState = state;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Which places each character knows (card M13, everon-map.md section 4; decisions D-M3, D-M4,
//! D-M11, D-M12). Server: the store, keyed by character record key, saved as "mapknown" and kept
//! across the 24 h world reset with the characters. A client visit is an ask the server checks
//! (VisitAsk: live, range, rate); the board, the clerk and quests call Hear. Client: the local
//! player's own set, filled by the join sync and each later change (LF_MapKnownController.c).
//! Rows for places that are dark or gone are kept and count again when the place comes back.
class LF_MapKnown
{
	static const string FEATURE = "MapKnown";

	//! Server: character key -> (place id -> saved state).
	protected static ref map<string, ref map<string, int>> s_mKnown;
	//! Server: player id -> tick times (ms) of the asks in the current window.
	protected static ref map<int, ref array<int>> s_mAsks;
	//! Client: what the local player's character knows (place id -> saved state).
	protected static ref map<string, int> s_mLocal;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mKnown = null;
		s_mAsks = null;
		s_mLocal = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, int> Character(string characterId, bool create)
	{
		if (!s_mKnown)
			s_mKnown = new map<string, ref map<string, int>>();

		map<string, int> places;
		if (s_mKnown.Find(characterId, places) || !create)
			return places;

		places = new map<string, int>();
		s_mKnown.Set(characterId, places);
		return places;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: what a character knows of a place.
	static LF_EMapPoiKnown KnownOf(string characterId, string placeId)
	{
		map<string, int> places = Character(characterId, false);
		int saved;
		if (places && places.Find(placeId, saved))
			return LF_MapKnownRules.FromSaved(saved);

		return LF_EMapPoiKnown.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: places a character knows (heard of or visited).
	static int CountOf(string characterId)
	{
		map<string, int> places = Character(characterId, false);
		if (!places)
			return 0;

		return places.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Store(string characterId, string placeId, LF_EMapPoiKnown known)
	{
		Character(characterId, true).Set(placeId, LF_MapKnownRules.SavedState(known));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: counts one ask against the player's window and returns how many came before it.
	protected static int CountAsk(int playerId, int nowMs)
	{
		if (!s_mAsks)
			s_mAsks = new map<int, ref array<int>>();

		array<int> times;
		if (!s_mAsks.Find(playerId, times))
		{
			times = {};
			s_mAsks.Set(playerId, times);
		}

		for (int i = times.Count() - 1; i >= 0; i--)
		{
			if (nowMs - times[i] >= LF_MapKnownRules.WINDOW_MS || nowMs < times[i])
				times.Remove(i);
		}

		// A refused ask is not stored, so a flooding client holds at most ASKS_PER_WINDOW times.
		int before = times.Count();
		if (before < LF_MapKnownRules.ASKS_PER_WINDOW)
			times.Insert(nowMs);

		return before;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a checked visit ask. Pure apart from the store, so LOGIC tests drive it with fixtures;
	//! the RPC fills live and distance from LF_Places and the player's own character.
	//! \param[in] playerId The asking player, for the rate window.
	//! \param[in] characterId The asking player's character record key.
	//! \param[in] placeId The place id the client named.
	//! \param[in] live The place is registered now.
	//! \param[in] distance Metres from the player's character to the place, measured by the server.
	//! \param[in] radius The place's discovery radius, 0 for the default.
	//! \param[in] nowMs The server tick time of the ask.
	static LF_EMapKnownOutcome VisitAsk(int playerId, string characterId, string placeId, bool live, float distance, float radius, int nowMs)
	{
		if (characterId.IsEmpty() || placeId.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("visit refused: player %1 sent place '%2' with character '%3'", playerId, placeId, characterId));
			return LF_EMapKnownOutcome.REFUSED_BAD;
		}

		int recent = CountAsk(playerId, nowMs);
		LF_EMapKnownOutcome outcome = LF_MapKnownRules.Decide(true, KnownOf(characterId, placeId), live, distance, radius, recent);
		if (outcome == LF_EMapKnownOutcome.VISITED)
		{
			Store(characterId, placeId, LF_EMapPoiKnown.VISITED);
			LF_Debug.Info(FEATURE, string.Format("%1 visited %2", characterId, placeId));
		}
		else if (outcome != LF_EMapKnownOutcome.KEPT)
		{
			LF_Debug.Warn(FEATURE, string.Format("visit refused (%1): %2 asked for %3 at %4 m", LF_MapKnownRules.OutcomeName(outcome), characterId, placeId, distance));
		}

		return outcome;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the character has heard of the place (the board, the clerk, an NPC, a quest step).
	//! Never lowers visited; works for a place that is not live yet. No network: HearPlayer sends it.
	static LF_EMapKnownOutcome HearCharacter(string characterId, string placeId)
	{
		if (characterId.IsEmpty() || placeId.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("hear refused: character '%1' place '%2'", characterId, placeId));
			return LF_EMapKnownOutcome.REFUSED_BAD;
		}

		LF_EMapKnownOutcome outcome = LF_MapKnownRules.Decide(false, KnownOf(characterId, placeId), true, 0, 0, 0);
		if (outcome == LF_EMapKnownOutcome.HEARD)
		{
			Store(characterId, placeId, LF_EMapPoiKnown.HEARD);
			LF_Debug.Info(FEATURE, string.Format("%1 heard of %2", characterId, placeId));
		}

		return outcome;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player's current character has heard of the place; the player's map gets it at once.
	//! The entry point for the town board, the arrival clerk and quest steps.
	//! \return True when the place is now heard of or visited.
	static bool Hear(int playerId, string placeId)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Error(FEATURE, "Hear called on a client; only the server records knowledge");
			return false;
		}

		string characterId = LF_Utils.GetPlayerUID(playerId);
		LF_EMapKnownOutcome outcome = HearCharacter(characterId, placeId);
		if (outcome == LF_EMapKnownOutcome.HEARD)
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (controller)
				controller.LF_SendMapKnown(placeId, LF_MapKnownRules.SavedState(LF_EMapPoiKnown.HEARD));
		}

		return outcome == LF_EMapKnownOutcome.HEARD || outcome == LF_EMapKnownOutcome.KEPT;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a character's whole set as parallel arrays, for the join sync.
	static int ExportCharacter(string characterId, notnull array<string> placeIds, notnull array<int> states)
	{
		map<string, int> places = Character(characterId, false);
		if (!places)
			return 0;

		foreach (string placeId, int state : places)
		{
			placeIds.Insert(placeId);
			states.Insert(state);
		}

		return places.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every row, for the "mapknown" payload.
	static array<ref LF_MapKnownRow> ExportAll()
	{
		array<ref LF_MapKnownRow> rows = new array<ref LF_MapKnownRow>();
		if (!s_mKnown)
			return rows;

		foreach (string characterId, map<string, int> places : s_mKnown)
		{
			foreach (string placeId, int state : places)
			{
				rows.Insert(LF_MapKnownRow.Make(characterId, placeId, state));
			}
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: replaces the store with saved rows (idempotent). Rows with no key or an unknown state
	//! are skipped; a repeated row keeps the better state. Rows of places that are not live are kept.
	//! \return Rows applied.
	static int ApplyAll(array<ref LF_MapKnownRow> rows)
	{
		s_mKnown = new map<string, ref map<string, int>>();
		if (!rows)
			return 0;

		int applied = 0;
		foreach (LF_MapKnownRow row : rows)
		{
			if (!row || row.m_sCharacterId.IsEmpty() || row.m_sPlaceId.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "load skipped a map knowledge row with no character or place");
				continue;
			}

			LF_EMapPoiKnown known = LF_MapKnownRules.FromSaved(row.m_iState);
			if (known == LF_EMapPoiKnown.NONE)
			{
				LF_Debug.Warn(FEATURE, string.Format("load skipped %1/%2: unknown state %3", row.m_sCharacterId, row.m_sPlaceId, row.m_iState));
				continue;
			}

			if (KnownOf(row.m_sCharacterId, row.m_sPlaceId) == LF_EMapPoiKnown.VISITED)
				continue;

			Store(row.m_sCharacterId, row.m_sPlaceId, known);
			applied++;
		}

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: replaces the local set (the join sync).
	static void SetLocalAll(array<string> placeIds, array<int> states)
	{
		s_mLocal = new map<string, int>();
		if (!placeIds || !states)
			return;

		int count = Math.Min(placeIds.Count(), states.Count());
		for (int i = 0; i < count; i++)
		{
			SetLocal(placeIds[i], states[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one change from the server. Never lowers visited.
	static void SetLocal(string placeId, int state)
	{
		if (!s_mLocal)
			s_mLocal = new map<string, int>();

		if (placeId.IsEmpty() || LF_MapKnownRules.FromSaved(state) == LF_EMapPoiKnown.NONE)
			return;

		int held;
		if (s_mLocal.Find(placeId, held) && held >= state)
			return;

		s_mLocal.Set(placeId, state);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: what the local player's character knows of a place.
	static LF_EMapPoiKnown LocalKnownOf(string placeId)
	{
		int saved;
		if (s_mLocal && s_mLocal.Find(placeId, saved))
			return LF_MapKnownRules.FromSaved(saved);

		return LF_EMapPoiKnown.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: copies the local set into an audience for LF_MapPois.Collect (the map card's seam).
	static void FillAudience(notnull LF_MapPoiAudience audience)
	{
		if (!s_mLocal)
			return;

		foreach (string placeId, int state : s_mLocal)
		{
			audience.SetKnown(placeId, LF_MapKnownRules.FromSaved(state));
		}
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MapKnown's statics (registered at script load; Core/LF_StaticReset.c).
class LF_MapKnown_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_MapKnown_StaticResetHook());

	override string Owner()
	{
		return "LF_MapKnown";
	}

	override void Apply()
	{
		LF_MapKnown.ResetStatics();
	}
}
