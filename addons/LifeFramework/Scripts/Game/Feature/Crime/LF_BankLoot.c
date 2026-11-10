//------------------------------------------------------------------------------------------------
//! What a forced-entry heist leaves lying (#274, #275). LF_Bank decides when a target pays; this
//! spawns the haul. Cash leaves as carried money bags (never a bank balance); stolen weapons are
//! stamped with a serial from the fence book, booked to the thief, so a weapon found at a scene and
//! checked at the evidence locker names them like a fence tool.
//------------------------------------------------------------------------------------------------
class LF_BankLoot
{
	static const string KIND_BAGS = "bags";
	static const string KIND_WEAPONS = "weapons";

	//------------------------------------------------------------------------------------------------
	//! A point in front of the target: forward along its local +Z, right along +X, up in the world.
	//! Without a target the offset is taken from the fallback point.
	static vector Spot(IEntity target, vector fallback, float forward, float right, float up)
	{
		if (!target)
			return fallback + Vector(right, up, forward);

		vector transform[4];
		target.GetWorldTransform(transform);
		return transform[3] + transform[2] * forward + transform[0] * right + Vector(0, up, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns count money bags in a row behind the target, each carrying cashPerBag of MoneyStack.
	//! \return The bags spawned. A bag that would not fill is deleted, not left empty.
	static array<IEntity> SpillBags(IEntity target, vector fallback, int count, int cashPerBag)
	{
		array<IEntity> bags = {};
		for (int i = 0; i < count; i++)
		{
			vector at = Spot(target, fallback, 1.4, (i - (count - 1) * 0.5) * 0.7, 0.3);
			// Lay each bag on the ground, not at the target's height: no bag hangs in the air.
			if (GetGame() && GetGame().GetWorld())
				at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + 0.05;

			IEntity bag = LF_Utils.SpawnEntityPrefab(LF_Bank.BAG_PREFAB, at);
			if (!bag)
			{
				LF_Debug.Error("Bank", "spill refused: the money bag did not spawn");
				continue;
			}

			if (!LF_Bank.FillBag(bag, cashPerBag))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(bag);
				continue;
			}

			bags.Insert(bag);
		}

		return bags;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns one weapon per entry of the comma list and stamps each with a serial booked to the
	//! thief. Weapons lie in a row in front of the target.
	//! \param[out] serials Comma list of the serials issued.
	//! \return The weapons spawned.
	static array<IEntity> IssueWeapons(IEntity target, vector fallback, string loot, string thiefUid, out string serials)
	{
		serials = "";
		array<string> entries = {};
		loot.Split(",", entries, true);
		array<IEntity> spawned = {};
		for (int i = 0; i < entries.Count(); i++)
		{
			ResourceName prefab = entries[i];
			vector at = Spot(target, fallback, 1.0, (i - (entries.Count() - 1) * 0.5) * 0.5, 0.25);
			IEntity weapon = LF_Utils.SpawnEntityPrefab(prefab, at);
			if (!weapon)
			{
				LF_Debug.Error("Bank", string.Format("loot refused: weapon %1 did not spawn", prefab));
				continue;
			}

			spawned.Insert(weapon);
			LF_SerialComponent stamp = LF_SerialComponent.Cast(weapon.FindComponent(LF_SerialComponent));
			if (!stamp)
			{
				LF_Debug.Error("Bank", string.Format("weapon %1 has no serial component; it is untracked", prefab));
				continue;
			}

			string serial = LF_FenceSerials.Issue(thiefUid, prefab);
			if (serial.IsEmpty() || !stamp.SetSerial(serial))
				continue;

			if (!serials.IsEmpty())
				serials += ",";

			serials += serial;
		}

		return spawned;
	}
}
