// red-proof: 2026-11-11, before the fix (LF_Utils.SpawnEntityPrefab dropped the prefab scale, bags 1.4 m out in a row),
//   cli test --no-build --only LF_Test_HaulTidy:
//   [LFTEST]   - bag 0 keeps the money bag prefab's scale (expected 0.55, got 1)
//   [LFTEST]   - bag 0 is a money bag's size (1.36045 m tall, want 0.4 to 0.6)
//   [LFTEST]   - bag 0 lies at the box (2.19135 m away, want under 1.5 m)
//   [LFTEST]   - bag 1 is clear of where the robber works (0.420013 m, want over 0.8 m)
// Fixed (the spawn matrix carries the prefab scale, the bag is 0.4, BagOffset row beside the box); green.
// tier: WORLD
//! crime/haul-tidy: the money bags a cash van heist spills are bag-sized, lie in a tidy row at the box and rest
//! on the surface. The trailer showed three 1.4 m sacks scattered across the road behind the van: the spawn
//! dropped the money bag prefab's scale, and the row stood where the robber works (#571 maintainer review).
class LF_Test_HaulTidy : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crime/haul-tidy";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity box = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.CASH_VAN_BOX);
		ctx.NotNull(box, "the cash van box is placed in the playtest town");
		if (!box)
			return;

		vector robberAt = LF_BankLoot.Spot(box, box.GetOrigin(), 1.1, 0, 0);
		array<IEntity> bags = LF_BankLoot.SpillBags(box, box.GetOrigin(), 3, 100);
		ctx.Equal(3, bags.Count(), "three bags spill");

		float scale = LF_Utils.PrefabScale(Resource.Load(LF_Bank.BagPrefab()));
		for (int i = 0; i < bags.Count(); i++)
		{
			IEntity bag = bags[i];
			ctx.EqualFloat(scale, bag.GetScale(), 0.01, string.Format("bag %1 keeps the money bag prefab's scale", i));
			vector mins;
			vector maxs;
			bag.GetWorldBounds(mins, maxs);
			float tall = maxs[1] - mins[1];
			ctx.True(tall > 0.4 && tall < 0.6, string.Format("bag %1 is a money bag's size (%2 m tall, want 0.4 to 0.6)", i, tall));

			vector under = LF_Dispatch.RestingSpot(Vector(bag.GetOrigin()[0], mins[1] + 0.3, bag.GetOrigin()[2]), bag);
			ctx.EqualFloat(under[1], mins[1], 0.08, string.Format("bag %1 rests on the surface under it", i));

			vector at = bag.GetOrigin();
			float fromBox = vector.DistanceXZ(at, box.GetOrigin());
			ctx.True(fromBox < 1.5, string.Format("bag %1 lies at the box (%2 m away, want under 1.5 m)", i, fromBox));
			float fromRobber = vector.DistanceXZ(at, robberAt);
			ctx.True(fromRobber > 0.8, string.Format("bag %1 is clear of where the robber works (%2 m, want over 0.8 m)", i, fromRobber));
			for (int j = 0; j < i; j++)
			{
				float apart = vector.DistanceXZ(at, bags[j].GetOrigin());
				ctx.True(apart > 0.3, string.Format("bags %1 and %2 do not overlap (%3 m apart)", j, i, apart));
			}
		}

		foreach (IEntity spilled : bags)
			SCR_EntityHelper.DeleteEntityAndChildren(spilled);

		// A robber who worked from the box's right-hand side gets the bags on its other side, not at their feet.
		vector besideRobber = LF_BankLoot.Spot(box, box.GetOrigin(), 0, 1.0, 0);
		array<IEntity> others = LF_BankLoot.SpillBags(box, besideRobber, 3, 100);
		ctx.Equal(3, others.Count(), "three bags spill for a robber at the side");
		foreach (int k, IEntity other : others)
		{
			float clear = vector.DistanceXZ(other.GetOrigin(), besideRobber);
			ctx.True(clear > 0.8, string.Format("bag %1 is clear of a robber at the box's side (%2 m)", k, clear));
			SCR_EntityHelper.DeleteEntityAndChildren(other);
		}
	}
}
