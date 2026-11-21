// red-proof: 2026-11-21 `--red LF_Test_BaseWorldClear` RED PROOF OK: with the hide skipped, "every listed prop in the store is hidden" failed.
//! placement/base-world-clear (#3882): a stamp's clear list hides the doubled vanilla furniture of one clothing store
//! (stepladder, coat rack) and nothing else: the building, its other furniture and a second copy of the building stay.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BaseWorldClear : LF_Test
{
	protected static const ResourceName SHOP = "{92E8F78BCF17D93D}Prefabs/Structures/Commercial/Shops/ShopHouse_E_2l01/ShopHouse_E_2I01t_clothesstore.et";
	protected static const string LADDER = "Prefabs/Props/Civilian/StepladderWooden_01.et";
	protected static const string RACK = "{1AB7A2C69A30391D}Prefabs/Props/Furniture/CoatRack_01.et";
	protected static const string TABLE = "Prefabs/Props/Furniture/Table_01.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "placement/base-world-clear";
	}

	//------------------------------------------------------------------------------------------------
	//! The building's furniture spawns as separate world entities inside its box (no Hierarchy on the prefab children).
	protected static void Find(IEntity building, string key, notnull array<IEntity> found)
	{
		vector mins, maxs;
		building.GetBounds(mins, maxs);
		vector mat[4];
		building.GetWorldTransform(mat);
		LF_BaseWorldClearQuery query = new LF_BaseWorldClearQuery();
		query.m_Keys = new set<string>();
		query.m_Keys.Insert(LF_BaseWorldClear.PathKey(key));
		query.m_Building = building;
		building.GetWorld().QueryEntitiesByOBB(mins, maxs, mat, query.Collect, null, EQueryEntitiesFlags.ALL);
		foreach (IEntity e : query.m_aFound)
		{
			found.Insert(e);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool AllHidden(notnull array<IEntity> list)
	{
		foreach (IEntity e : list)
		{
			if (!LF_FelledTreesReplication.IsHidden(e))
				return false;
		}

		return !list.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		float y = GetGame().GetWorld().GetSurfaceY(420, 60);
		IEntity shop = LF_Utils.SpawnEntityPrefab(SHOP, Vector(420, y, 60));
		IEntity other = LF_Utils.SpawnEntityPrefab(SHOP, Vector(460, y, 60));
		ctx.NotNull(shop, "the clothing store spawned");
		ctx.NotNull(other, "a second clothing store spawned");
		if (!shop || !other)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(shop);
			SCR_EntityHelper.DeleteEntityAndChildren(other);
			return;
		}

		array<IEntity> ladders = {};
		array<IEntity> racks = {};
		array<IEntity> tables = {};
		array<IEntity> otherLadders = {};
		Find(shop, LADDER, ladders);
		Find(shop, RACK, racks);
		Find(shop, TABLE, tables);
		Find(other, LADDER, otherLadders);
		ctx.True(ladders.Count() > 0 && racks.Count() > 0 && tables.Count() > 0, string.Format("the store brings its own furniture (%1 ladders, %2 racks, %3 tables)", ladders.Count(), racks.Count(), tables.Count()));

		array<string> clear = {LADDER, RACK};
		int hidden = 0;
		if (!RedProof())
			hidden = LF_BaseWorldClear.HideInBuilding(shop, clear);

		ctx.Equal(ladders.Count() + racks.Count(), hidden, "every listed prop in the store is hidden");
		ctx.True(AllHidden(ladders) && AllHidden(racks), "the stepladder and coat rack are not drawn");
		foreach (IEntity ladder : ladders)
		{
			Physics physics = ladder.GetPhysics();
			if (physics)
				ctx.Equal(SimulationState.NONE, physics.GetSimulationState(), "a hidden prop has no collision");
		}

		ctx.False(LF_FelledTreesReplication.IsHidden(shop), "the building itself stays");
		ctx.False(AllHidden(tables), "furniture off the list stays");
		ctx.False(AllHidden(otherLadders), "another copy of the building keeps its stepladder");
		ctx.Equal(0, LF_BaseWorldClear.HideInBuilding(shop, clear), "a second apply hides nothing more");

		SCR_EntityHelper.DeleteEntityAndChildren(shop);
		SCR_EntityHelper.DeleteEntityAndChildren(other);
	}
}
