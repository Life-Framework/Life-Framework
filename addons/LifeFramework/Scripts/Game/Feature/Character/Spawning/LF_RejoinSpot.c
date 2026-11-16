//------------------------------------------------------------------------------------------------
//! What the world says about a saved spot (#1778). Filled by LF_RejoinSpot.ProbeWorld on the
//! server; a test fills it by hand. The defaults describe a safe open field.
class LF_RejoinProbe
{
	//! Terrain height under the spot.
	float m_fSurfaceY;
	//! Water level (ocean base height).
	float m_fWaterY;
	//! No room to stand: solid geometry at the spot, or no head room.
	bool m_bBlocked;
	//! Inside the jail cell zone.
	bool m_bInJail;
	//! Inside a zone that keeps people out (police may not enter).
	bool m_bRestricted;
	//! Inside a locked house.
	bool m_bLockedHouse;
}

//------------------------------------------------------------------------------------------------
//! Finds the entities of a locked house near a spot (QueryEntitiesBySphere callback).
class LF_RejoinHouseQuery
{
	vector m_vSpot;
	bool m_bFound;

	//------------------------------------------------------------------------------------------------
	bool Visit(IEntity entity)
	{
		LF_HouseManagerComponent house = LF_HouseManagerComponent.Cast(entity.FindComponent(LF_HouseManagerComponent));
		if (!house || !house.IsHouseLocked())
			return true;

		vector mins, maxs;
		entity.GetWorldBounds(mins, maxs);
		if (m_vSpot[0] >= mins[0] && m_vSpot[0] <= maxs[0] && m_vSpot[1] >= mins[1] && m_vSpot[1] <= maxs[1] && m_vSpot[2] >= mins[2] && m_vSpot[2] <= maxs[2])
		{
			m_bFound = true;
			return false;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! A returning character rejoins where it logged out (#1778, second half of #640), when it is
//! safe to. Off by default (LF_RespawnConfig.m_bRejoinAtLastPosition: vanilla spawn). The saved
//! spot comes from LF_LastPosition (#1777). The order in LF_SpawnLogic.GetCreationPosition: the jail
//! return (a prisoner is moved into the cell after the spawn), the hospital wake, this spot, the
//! spawn point. A stale spot, or one under the terrain, in water, inside geometry, inside the jail,
//! in a restricted zone or in a locked house, is refused with a logged reason and the character
//! takes the normal spawn. A character with no row (a new arrival) takes the spawn point too.
class LF_RejoinSpot
{
	static const string FEATURE = "Rejoin";

	//! Players that already had a body on this connection (uid -> true). Only the first spawn of a
	//! connection is a rejoin; a respawn after a death is not, and takes the normal spawn.
	protected static ref map<string, bool> s_mSpawned;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mSpawned = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Marks that the player has spawned on this connection. \return True the first time (a rejoin).
	static bool NoteSpawn(string uid)
	{
		if (!s_mSpawned)
			s_mSpawned = new map<string, bool>();

		if (s_mSpawned.Contains(uid))
			return false;

		s_mSpawned.Set(uid, true);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The player left: their next spawn is a rejoin again.
	static void ForgetConnection(string uid)
	{
		if (s_mSpawned)
			s_mSpawned.Remove(uid);
	}

	//! Below the terrain by more than this is under it (a body clips a little).
	static const float UNDER_TERRAIN_M = 0.5;
	//! Above the water by less than this is in it.
	static const float WATER_MARGIN_M = 0.5;
	//! Room a standing body needs above its feet.
	static const float HEAD_ROOM_M = 1.8;
	//! The jail cell zone: a spot this close to the cell is inside the jail.
	static const float JAIL_RADIUS_M = 15;	// lf-fixed: a safety margin, not a tunable
	//! Largest house the lock query looks around.
	static const float HOUSE_QUERY_RADIUS_M = 15;	// lf-fixed: the house search margin, not a tunable

	//------------------------------------------------------------------------------------------------
	//! Pure: does the saved spot take part in this spawn at all?
	//! \param enabled The rejoinAtLastPosition setting.
	//! \param serving The character is serving a sentence (the jail return comes first).
	//! \param pendingWake A hospital wake is pending (it comes first).
	static bool Applies(bool enabled, bool serving, bool pendingWake)
	{
		return enabled && !serving && !pendingWake;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: why a saved spot is not safe, or an empty string when it is.
	//! \param maxAgeHours Older than this many hours is stale. 0 or less: no age limit.
	static string Verdict(LF_LastPositionRow row, int nowUnix, int maxAgeHours, LF_RejoinProbe probe)
	{
		if (!row)
			return "no saved spot";

		if (maxAgeHours > 0 && nowUnix - row.m_iTakenUnix > maxAgeHours * 3600)
			return string.Format("stale: taken %1 h ago, limit %2 h", Math.Round((nowUnix - row.m_iTakenUnix) / 3600.0), maxAgeHours);

		if (!probe)
			return "the spot could not be checked";

		float y = row.m_vPosition[1];
		if (y < probe.m_fSurfaceY - UNDER_TERRAIN_M)
			return "under the terrain";

		if (probe.m_fSurfaceY < probe.m_fWaterY && y < probe.m_fWaterY + WATER_MARGIN_M)
			return "in water";

		if (probe.m_bBlocked)
			return "inside geometry";

		if (probe.m_bInJail)
			return "inside the jail";

		if (probe.m_bRestricted)
			return "in a restricted zone";

		if (probe.m_bLockedHouse)
			return "inside a locked house";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: reads the world around a spot.
	static LF_RejoinProbe ProbeWorld(vector spot)
	{
		LF_RejoinProbe probe = new LF_RejoinProbe();
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			probe.m_bBlocked = true;
			return probe;
		}

		probe.m_fSurfaceY = world.GetSurfaceY(spot[0], spot[2]);
		probe.m_fWaterY = world.GetOceanBaseHeight();

		// Head room: a clear line from just above the feet to standing height.
		TraceParam trace = new TraceParam();
		trace.Start = spot + "0 0.2 0";
		trace.End = spot + Vector(0, HEAD_ROOM_M, 0);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;
		float fraction = world.TraceMove(trace, null);
		if (fraction < 1 && !ChimeraCharacter.Cast(trace.TraceEnt))
			probe.m_bBlocked = true;

		if (LF_Jail.HasCell() && vector.Distance(spot, LF_Jail.GetCellOrigin()) <= JAIL_RADIUS_M)
			probe.m_bInJail = true;

		if (!LF_Zones.RuleSetAt(spot).m_bCopsEnter)
			probe.m_bRestricted = true;

		LF_RejoinHouseQuery query = new LF_RejoinHouseQuery();
		query.m_vSpot = spot;
		world.QueryEntitiesBySphere(spot, HOUSE_QUERY_RADIUS_M, query.Visit);
		probe.m_bLockedHouse = query.m_bFound;

		return probe;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_SpawnLogic.GetCreationPosition, after the jail return and the hospital wake.
	//! \return True and the spot when the character rejoins at its saved spot; false for the
	//!         normal spawn (setting off, no row, or an unsafe spot, each logged).
	static bool Resolve(int playerId, string uid, string characterId, out vector position, out vector yawPitchRoll)
	{
		LF_RespawnConfig config = LF_RespawnConfig.Get();
		bool rejoin = NoteSpawn(uid);
		if (config && config.m_bRejoinAtLastPosition && !rejoin)
		{
			LF_Debug.Info(FEATURE, string.Format("player %1 respawns on the same connection: normal spawn", playerId));
			return false;
		}

		if (!config || !Applies(config.m_bRejoinAtLastPosition, LF_Jail.IsServing(uid), LF_HospitalRespawn.HasPendingWake(uid)))
			return false;

		LF_LastPositionRow row = LF_LastPosition.Get(characterId);
		if (!row)
		{
			LF_Debug.Info(FEATURE, string.Format("player %1 has no saved spot: normal spawn", playerId));
			return false;
		}

		string reason = Verdict(row, System.GetUnixTime(), config.m_iRejoinMaxAgeHours, ProbeWorld(row.m_vPosition));
		if (!reason.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("player %1 saved spot %2 refused (%3): normal spawn", playerId, row.m_vPosition, reason));
			return false;
		}

		position = row.m_vPosition;
		yawPitchRoll = Vector(row.m_fYaw, 0, 0);
		LF_Debug.Info(FEATURE, string.Format("player %1 rejoins at the saved spot %2 heading %3", playerId, position, row.m_fYaw));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_RejoinSpot's statics (registered at script load; Core/LF_StaticReset.c).
class LF_RejoinSpot_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_RejoinSpot_StaticResetHook());

	override string Owner()
	{
		return "LF_RejoinSpot";
	}

	override void Apply()
	{
		LF_RejoinSpot.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! A leaver's next spawn is a rejoin again.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (Replication.IsServer())
		{
			string uid = LF_Utils.GetPlayerUID(playerId);
			if (!uid.IsEmpty())
				LF_RejoinSpot.ForgetConnection(uid);
		}

		super.OnPlayerDisconnected(playerId, cause, timeout);
	}
}
