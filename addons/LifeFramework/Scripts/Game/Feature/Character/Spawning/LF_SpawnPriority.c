//------------------------------------------------------------------------------------------------
//! The rule a login spawn follows (#2652, spec #747). Jail time wins, a death wakes at the hospital,
//! a rejoin takes the saved spot (#1778, off by default), then the player's own house, then the
//! role's spawn point. The decision is pure so a test names each pair; the world work (the house's
//! point) lives in LF_HouseSpawnComponent.
enum LF_SpawnRule
{
	//! Serving a sentence: spawn normally, LF_Jail.OnPlayerSpawned then moves the body to the cell.
	JAIL,
	//! A death is pending: wake in a hospital bed (LF_HospitalRespawn).
	HOSPITAL,
	//! Rejoin at the spot the player logged out at (LF_RejoinSpot). Slot only until #640 lands.
	REJOIN,
	//! Wake inside the player's own spawn house (LF_Properties.SpawnHouseOf).
	HOUSE,
	//! The spawn point of the player's role.
	ROLE
}

//------------------------------------------------------------------------------------------------
class LF_SpawnPriority
{
	static const string FEATURE = "Spawn";

	//------------------------------------------------------------------------------------------------
	//! Pure: which rule decides this login.
	//! \param serving The character is serving a sentence.
	//! \param pendingWake A death is pending (the next spawn wakes in hospital).
	//! \param rejoinSpot A safe saved spot is available.
	//! \param houseSpawn A usable spawn house is available (HouseVerdict returned empty).
	static LF_SpawnRule Decide(bool serving, bool pendingWake, bool rejoinSpot, bool houseSpawn)
	{
		if (serving)
			return LF_SpawnRule.JAIL;

		if (pendingWake)
			return LF_SpawnRule.HOSPITAL;

		if (rejoinSpot)
			return LF_SpawnRule.REJOIN;

		if (houseSpawn)
			return LF_SpawnRule.HOUSE;

		return LF_SpawnRule.ROLE;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: why a recorded spawn house cannot be used, or an empty string when it can. A property id
	//! is not remapped when a map edit renames a place, so the id can point at nothing.
	//! \param houseId The id LF_Properties.SpawnHouseOf returned (empty: the player chose none).
	//! \param houseInWorld The house with that id is in the world.
	//! \param liveOwner The character key that owns it now.
	//! \param ownerKey The character who is logging in.
	static string HouseVerdict(string houseId, bool houseInWorld, string liveOwner, string ownerKey)
	{
		if (houseId.IsEmpty())
			return "";

		if (!houseInWorld)
			return string.Format("house %1 is not in the world", houseId);

		if (ownerKey.IsEmpty() || liveOwner != ownerKey)
			return string.Format("house %1 is no longer owned by this player", houseId);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_SpawnLogic.GetCreationPosition after the hospital wake, the first-spawn bed and
	//! the rejoin spot: the spawn house of a player who is not serving and has no death pending.
	//! \return True and a clear point inside the house; false for the role's spawn point. A recorded
	//! house that is gone, or no longer the player's, logs a warning and falls through.
	static bool ResolveHouse(int playerId, string uid, out vector position, out vector yawPitchRoll)
	{
		// Asked with a house assumed: only the jail and the hospital come out ahead of it here.
		if (Decide(LF_Jail.IsServing(uid), LF_HospitalRespawn.HasPendingWake(uid), false, true) != LF_SpawnRule.HOUSE)
			return false;

		string houseId = LF_Properties.SpawnHouseOf(uid);
		if (houseId.IsEmpty())
			return false;

		LF_HouseManagerComponent house = LF_Houses.Find(houseId);
		string liveOwner = LF_Properties.GetOwnerKey(houseId);
		string reason = HouseVerdict(houseId, house != null, liveOwner, uid);
		if (!reason.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("player %1 spawn house refused (%2): role spawn", playerId, reason));
			return false;
		}

		LF_HouseSpawnComponent spawn = LF_Component<LF_HouseSpawnComponent>.Find(house.GetOwner());
		if (!spawn)
		{
			LF_Debug.Info(FEATURE, string.Format("player %1 spawn house %2 has no spawn points: role spawn", playerId, houseId));
			return false;
		}

		if (!spawn.GetSpawnPoint(position, yawPitchRoll))
		{
			LF_Debug.Warn(FEATURE, string.Format("player %1 spawn house %2 has no usable point: role spawn", playerId, houseId));
			return false;
		}

		LF_Debug.Info(FEATURE, string.Format("player %1 spawns in house %2 at %3", playerId, houseId, position));
		return true;
	}
}
