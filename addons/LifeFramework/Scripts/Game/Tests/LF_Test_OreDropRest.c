// red-proof: 2026-11-22, on the unfixed DropPiece (pivot 5 cm over a traced point, dynamic pieces thrown, random offsets) `--only LF_Test_OreDropRest` failed "nugget 0 rests on the surface (gap=0.019009)", nugget 3 gap 0.0999 and "no two nuggets lie in one spot (closest 0.0441 m, wanted 0.35)". With the fix, green: gaps 0.001 m, pieces 0.68 to 1.13 m from the rock centre.

//! Ore that comes off a rock rests on the ground beside it: five pickaxe breaks drop five iron nuggets, each with
//! its measured bottom on the surface under it (LF_PlaceSnap, the shared runtime probe), outside the rock's own
//! footprint, and no two lying in one spot.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_OreDropRest : LF_Test
{
	protected static const ResourceName IRON_ORE = "{2C571D6F7C05E095}Prefabs/Resources/Mining/IronOre.et";
	protected static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	protected static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	protected static const vector SPOT = "60 0 60";
	//! A piece's bottom may differ from the support by the snap clearance (1 mm) and a little float noise.
	protected static const float REST_TOLERANCE = 0.01;

	protected ref array<IEntity> m_aFound = {};

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "mining/ore-drop-rest";
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectNugget(IEntity entity)
	{
		m_aFound.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsNugget(IEntity entity)
	{
		return entity && LF_Utils.GetPrefabName(entity) == NUGGET && !entity.GetParent();
	}

	//------------------------------------------------------------------------------------------------
	protected void FindNuggets(vector centre)
	{
		m_aFound = new array<IEntity>();
		GetGame().GetWorld().QueryEntitiesBySphere(centre, 6, CollectNugget, IsNugget);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		float y = GetGame().GetWorld().GetSurfaceY(SPOT[0], SPOT[2]);
		vector at = Vector(SPOT[0], y, SPOT[2]);
		IEntity rock = LF_Utils.SpawnEntityPrefab(IRON_ORE, at);
		IEntity pickaxe = LF_Utils.SpawnEntityPrefab(PICKAXE, at + Vector(0, 5, 0));
		ctx.NotNull(rock, "iron rock spawns");
		ctx.NotNull(pickaxe, "pickaxe spawns");
		LF_OreNodeComponent ore;
		if (rock)
			ore = LF_OreNodeComponent.Cast(rock.FindComponent(LF_OreNodeComponent));
		ctx.NotNull(ore, "the rock carries LF_OreNodeComponent");
		if (ctx.FailureCount() == 0)
			RunChecks(ctx, rock, pickaxe, ore);

		FindNuggets(at);
		foreach (IEntity nugget : m_aFound)
			SCR_EntityHelper.DeleteEntityAndChildren(nugget);
		m_aFound = new array<IEntity>();
		if (rock)
			SCR_EntityHelper.DeleteEntityAndChildren(rock);
		if (pickaxe)
			SCR_EntityHelper.DeleteEntityAndChildren(pickaxe);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChecks(LF_TestContext ctx, IEntity rock, IEntity pickaxe, LF_OreNodeComponent ore)
	{
		// The rock's footprint at full size, before the first piece shrinks it: where a piece must not lie.
		vector mins;
		vector maxs;
		rock.GetBounds(mins, maxs);
		float scale = rock.GetScale();
		vector centre = rock.CoordToParent((mins + maxs) * 0.5);
		float halfX = (maxs[0] - mins[0]) * 0.5 * scale;
		float halfZ = (maxs[2] - mins[2]) * 0.5 * scale;

		// Always the same swing from the same side: the worst case for pieces landing in one spot.
		vector hitPos = rock.GetOrigin() + Vector(-0.6, 0.4, 0);
		int dropped;
		for (int piece = 0; piece < 5; piece++)
		{
			if (piece == 1)
				CheckFirstPiece(ctx, rock, halfX, halfZ, centre);
			dropped += ore.OnToolHit(60, hitPos, vector.Right, pickaxe);
		}

		ctx.Equal(5, dropped, "five breaks drop the iron row's five nuggets");
		FindNuggets(rock.GetOrigin());
		ctx.Equal(5, m_aFound.Count(), "five nuggets lie in the world: none lost, none doubled");

		float minApart = 1e9;
		for (int i = 0; i < m_aFound.Count(); i++)
		{
			IEntity nugget = m_aFound[i];
			LF_PlaceSnapResult r = LF_PlaceSnap.Measure(nugget);
			ctx.True(r.m_iHits > 0, string.Format("nugget %1 has ground under it", i));
			if (r.m_iHits > 0)
				ctx.True(Math.AbsFloat(r.m_fGap) < REST_TOLERANCE, string.Format("nugget %1 rests on the surface (%2)", i, r.Format()));

			for (int j = i + 1; j < m_aFound.Count(); j++)
			{
				vector d = nugget.GetOrigin() - m_aFound[j].GetOrigin();
				d[1] = 0;
				if (d.Length() < minApart)
					minApart = d.Length();
			}
		}

		ctx.True(minApart >= LF_OreDrop.SPACING - 0.02, string.Format("no two nuggets lie in one spot (closest %1 m, wanted %2)", minApart, LF_OreDrop.SPACING));
	}

	//------------------------------------------------------------------------------------------------
	//! The piece the first break dropped (the only nugget so far) lies outside the rock's full-size footprint.
	protected void CheckFirstPiece(LF_TestContext ctx, IEntity rock, float halfX, float halfZ, vector centre)
	{
		FindNuggets(rock.GetOrigin());
		ctx.Equal(1, m_aFound.Count(), "the first break dropped one nugget");
		if (m_aFound.Count() != 1)
			return;

		vector p = m_aFound[0].GetOrigin();
		bool outside = Math.AbsFloat(p[0] - centre[0]) > halfX || Math.AbsFloat(p[2] - centre[2]) > halfZ;
		ctx.True(outside, string.Format("the first nugget lies outside the rock's footprint (offset %1 %2, half %3 %4)", p[0] - centre[0], p[2] - centre[2], halfX, halfZ));
	}
}
