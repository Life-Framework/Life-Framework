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
	//! Spilled bags: the first stands this far right of the target's centre, the next ones a step on, BAG_ROW a row.
	static const float BAG_FIRST_RIGHT = 0.64;
	static const float BAG_STEP = 0.36;
	static const int BAG_ROW = 3;

	//------------------------------------------------------------------------------------------------
	//! A point in front of the target: forward along its local +Z, right along +X, up in the world, in metres
	//! whatever the target's scale. Without a target the offset is taken from the fallback point.
	static vector Spot(IEntity target, vector fallback, float forward, float right, float up)
	{
		if (!target)
			return fallback + Vector(right, up, forward);

		vector transform[4];
		target.GetWorldTransform(transform);
		vector ahead = transform[2].Normalized();
		vector side = transform[0].Normalized();
		return transform[3] + ahead * forward + side * right + Vector(0, up, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Where bag i of a spill lies, beside the target in its local frame (x right, z forward), in metres: a row
	//! of three along one side of the target, then a second row a step further out in front. side is +1 for the
	//! target's right, -1 for its left.
	static vector BagOffset(int index, float side = 1)
	{
		int column = index % BAG_ROW;
		int row = index / BAG_ROW;
		return Vector(side * (BAG_FIRST_RIGHT + column * BAG_STEP), 0, row * BAG_STEP);
	}

	//------------------------------------------------------------------------------------------------
	//! The side of the target the bags go to: away from the robber. A robber in front (or with no target) gets
	//! them on the target's right; the drill they leave rests on their own right, the target's left.
	static float BagSide(IEntity target, vector robberAt)
	{
		if (!target)
			return 1;

		vector transform[4];
		target.GetWorldTransform(transform);
		float across = vector.Dot(robberAt - transform[3], transform[0].Normalized());
		if (across > 0.3)
			return -1;

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns count money bags in a tidy row beside the target, on the side away from the robber (fallback is
	//! where they stood), each resting on the surface under it and carrying cashPerBag of MoneyStack. They used to
	//! stand in a row 1.4 m out where the robber works, on the road behind the cash van.
	//! \return The bags spawned. A bag that would not fill is deleted, not left empty.
	static array<IEntity> SpillBags(IEntity target, vector fallback, int count, int cashPerBag)
	{
		array<IEntity> bags = {};
		float yaw = 0;
		if (target)
			yaw = target.GetYawPitchRoll()[0];

		float side = BagSide(target, fallback);
		for (int i = 0; i < count; i++)
		{
			vector offset = BagOffset(i, side);
			vector at = Spot(target, fallback, offset[2], offset[0], 0);
			// On the first surface under the spot (the verge, a floor), not hanging at the target's height.
			if (GetGame() && GetGame().GetWorld())
				at = LF_Dispatch.RestingSpot(at + "0 0.3 0");

			// A small turn each, so the row reads as set down by hand rather than stamped out.
			int odd = i % 2;
			float turn = yaw + 12 * odd - 6;
			IEntity bag = LF_Utils.SpawnEntityPrefab(LF_Bank.BagPrefab(), at, Vector(turn, 0, 0));
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
