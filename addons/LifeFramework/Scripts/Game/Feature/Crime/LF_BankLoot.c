//------------------------------------------------------------------------------------------------
//! What a forced-entry heist leaves lying (#274). LF_Bank decides when a target pays; this spawns
//! the haul. Cash leaves as carried money bags, never a bank balance.
//------------------------------------------------------------------------------------------------
class LF_BankLoot
{
	static const string KIND_BAGS = "bags";

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
			vector at = Spot(target, fallback, 1.4, (i - (count - 1) * 0.5) * 0.55, 0.3);
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
}
