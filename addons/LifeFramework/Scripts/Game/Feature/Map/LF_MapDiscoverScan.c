// LF_MapDiscoverScan.c: discovery by the server's own look-around (#2492, card M13). The client used to scan
// for DISCOVER places in reach and ask, which needed every undiscovered place's position on the client. Now a
// client holds no private place, so the server checks each player against a grid of the DISCOVER places every
// SCAN_MS and records a visit itself. 128 players against 400 places is one cell lookup each
// (perf/map-128, map-discover-scan-ms).

//------------------------------------------------------------------------------------------------
//! A grid of place positions keyed by CELL-metre squares, so a "what is within r of here" asks a few cells and
//! not every place. Pure data; the PERF test drives it with fixtures.
class LF_MapDiscoverIndex
{
	//! Cell edge in metres. At least the discovery radius, so a 3 by 3 block covers any reach up to CELL.
	static const float CELL = 32; // lf-fixed: grid bucket; >= LF_MapKnownRules.DEFAULT_RADIUS

	protected ref array<string> m_aIds = {};
	protected ref array<vector> m_aPos = {};
	//! Cell key -> indexes into m_aIds/m_aPos.
	protected ref map<int, ref array<int>> m_mCells = new map<int, ref array<int>>();

	//------------------------------------------------------------------------------------------------
	protected static int CellOf(float v)
	{
		return Math.Floor(v / CELL);
	}

	//------------------------------------------------------------------------------------------------
	//! Cell coordinates fit in 16 bits for any map under 1000 km.
	protected static int Key(int cx, int cz)
	{
		return (cx << 16) ^ (cz & 65535);
	}

	//------------------------------------------------------------------------------------------------
	void Add(string id, vector pos)
	{
		int index = m_aIds.Count();
		m_aIds.Insert(id);
		m_aPos.Insert(pos);
		int key = Key(CellOf(pos[0]), CellOf(pos[2]));
		array<int> cell = m_mCells.Get(key);
		if (!cell)
		{
			cell = {};
			m_mCells.Set(key, cell);
		}

		cell.Insert(index);
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_aIds.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Appends the ids of the places within radius (2D, x and z) of pos. radius must be at most CELL.
	//! \return How many were added.
	int Near(vector pos, float radius, notnull array<string> into)
	{
		int added = 0;
		int cx = CellOf(pos[0]);
		int cz = CellOf(pos[2]);
		float reach = radius * radius;
		for (int dx = -1; dx <= 1; dx++)
		{
			for (int dz = -1; dz <= 1; dz++)
			{
				array<int> cell = m_mCells.Get(Key(cx + dx, cz + dz));
				if (!cell)
					continue;

				foreach (int index : cell)
				{
					vector at = m_aPos[index];
					float ddx = at[0] - pos[0];
					float ddz = at[2] - pos[2];
					if (ddx * ddx + ddz * ddz > reach)
						continue;

					into.Insert(m_aIds[index]);
					added++;
				}
			}
		}

		return added;
	}
}

//------------------------------------------------------------------------------------------------
//! Server: every SCAN_MS, each player standing (not driving past) within DEFAULT_RADIUS of a DISCOVER place
//! their character has not visited visits it. The player gets the new state and their private places at once.
class LF_MapDiscoverScan
{
	static const int SCAN_MS = 2000; // lf-fixed: everon-map.md section 4, the look-around period
	//! No discovery from a vehicle faster than 60 km/h (16.7 m/s): driving past is not a visit.
	static const float MAX_SPEED_MS = 16.7; // lf-fixed: everon-map.md section 4, drive-by bound
	//! How often the grid is built again from LF_Places (places register at world load; a slow rebuild
	//! picks up a late one).
	static const int REINDEX_MS = 60000; // lf-fixed: places are static after world load

	protected static bool s_bRunning;
	protected static ref LF_MapDiscoverIndex s_Index;
	protected static int s_iIndexedAt;

	//------------------------------------------------------------------------------------------------
	//! Server: starts the scan once (the first join sync calls it).
	static void Start()
	{
		if (s_bRunning || !Replication.IsServer())
			return;

		s_bRunning = true;
		GetGame().GetCallqueue().CallLater(Tick, SCAN_MS, true);
		LF_Debug.Info(LF_MapKnown.FEATURE, "server discovery scan started");
	}

	//------------------------------------------------------------------------------------------------
	//! The grid of every live DISCOVER place from the server's LF_Places. Null config builds an empty grid.
	static LF_MapDiscoverIndex BuildIndex(LF_MapPoiConfig config)
	{
		LF_MapDiscoverIndex index = new LF_MapDiscoverIndex();
		if (!config || !config.m_aKinds)
			return index;

		array<ref LF_MapPoiPlace> places = {};
		foreach (LF_MapPoiKind row : config.m_aKinds)
		{
			if (row && !LF_MapPoiConfig.IsSlotRow(row) && row.m_eVisibility == LF_EMapPoiVisibility.DISCOVER)
				LF_MapPois.GatherKind(row.m_sKind, places);
		}

		foreach (LF_MapPoiPlace place : places)
		{
			index.Add(place.m_sId, place.m_vPos);
		}

		return index;
	}

	//------------------------------------------------------------------------------------------------
	//! The places one character visits standing at pos: those within reach it has not visited yet. Pure
	//! apart from reading the server's store.
	//! \return How many ids were added.
	static int VisitsAt(notnull LF_MapDiscoverIndex index, string characterId, vector pos, notnull array<string> into)
	{
		array<string> near = {};
		index.Near(pos, LF_MapKnownRules.DEFAULT_RADIUS, near);
		int added = 0;
		foreach (string placeId : near)
		{
			if (LF_MapKnown.KnownOf(characterId, placeId) == LF_EMapPoiKnown.VISITED)
				continue;

			into.Insert(placeId);
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		if (playerIds.IsEmpty())
			return;

		int nowMs = System.GetTickCount();
		if (!s_Index || nowMs - s_iIndexedAt >= REINDEX_MS || nowMs < s_iIndexedAt)
		{
			s_Index = BuildIndex(LF_MapPoiConfig.Get());
			s_iIndexedAt = nowMs;
		}

		if (s_Index.Count() == 0)
			return;

		foreach (int playerId : playerIds)
		{
			ScanPlayer(players, playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void ScanPlayer(PlayerManager players, int playerId)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(players.GetPlayerController(playerId));
		if (!controller)
			return;

		IEntity character = controller.GetControlledEntity();
		if (!character)
			return;

		Physics physics = character.GetRootParent().GetPhysics();
		if (physics && physics.GetVelocity().Length() > MAX_SPEED_MS)
			return;

		string characterId = LF_Utils.GetPlayerUID(playerId);
		if (characterId.IsEmpty())
			return;

		array<string> visits = {};
		if (VisitsAt(s_Index, characterId, character.GetOrigin(), visits) == 0)
			return;

		int fresh = 0;
		int visited = LF_MapKnownRules.SavedState(LF_EMapPoiKnown.VISITED);
		foreach (string placeId : visits)
		{
			if (LF_MapKnown.VisitServer(characterId, placeId) != LF_EMapKnownOutcome.VISITED)
				continue;

			controller.LF_SendMapKnown(placeId, visited, false);
			fresh++;
		}

		if (fresh > 0)
			LF_MapPrivatePlaces.SendTo(controller);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bRunning && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bRunning = false;
		s_Index = null;
		s_iIndexedAt = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the server scan (LF_StaticReset finds it by reflection).
class LF_MapDiscoverScan_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MapDiscoverScan";
	}

	override void Apply()
	{
		LF_MapDiscoverScan.ResetStatics();
	}
}
