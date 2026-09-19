// tier: WORLD
// red-proof: 2026-09-19 dedicated-server run failed both apple crate physics and
// sweep assertions (fraction=1) with the world entity in Terrain.layer. Moving
// world initialization to default.layer gave both sweeps fraction=0.386069.
class EL_Test_DebugWorldCollision : EL_Test
{
	protected static int s_iEntities;
	protected static int s_iMissingTraceable;
	protected static ref array<string> s_aFailures = {};
	protected static ref array<EntityID> s_aTraderTables = {};
	protected static vector s_vTraderOrigin;

	//------------------------------------------------------------------------------------------------
	static bool InspectEntity(IEntity entity)
	{
		if (!entity)
			return true;

		typename type = entity.Type();
		if (type.IsInherited(GenericTerrainEntity) || type.IsInherited(GenericWorldEntity) || type.IsInherited(SCR_ChimeraCharacter))
			return true;

		s_iEntities++;
		EntityPrefabData prefab = entity.GetPrefabData();
		if (prefab && prefab.GetPrefabName().Contains("TableRecreation_01.et") && vector.Distance(entity.GetOrigin(), s_vTraderOrigin) < 5)
			s_aTraderTables.Insert(entity.GetID());

		string label = entity.GetName();
		if (label.IsEmpty())
			label = type.ToString();

		if (!(entity.GetFlags() & EntityFlags.TRACEABLE))
		{
			s_iMissingTraceable++;
			s_aFailures.Insert(label + " is not traceable");
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "debugworld/entity-collision";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		s_iEntities = 0;
		s_iMissingTraceable = 0;
		s_aFailures = new array<string>();
		s_aTraderTables = new array<EntityID>();
		World world = GetGame().GetWorld();
		IEntity trader = world.FindEntityByName("AppleTrader");
		ctx.NotNull(trader, "AppleTrader exists");
		if (!trader)
			return;
		s_vTraderOrigin = trader.GetOrigin();

		vector center = "256 0 256";
		GetGame().GetWorld().QueryEntitiesBySphere(center, 500, InspectEntity, null, EQueryEntitiesFlags.ALL);

		ctx.True(s_iEntities > 0, "DebugWorld collision scan found entities");
		ctx.Equal(0, s_iMissingTraceable, "DebugWorld entities are traceable: " + s_aFailures.ToString());
		CheckObstacle(ctx, world.FindEntityByName("AppleTradeContainer"), "AppleTradeContainer", "0 0.15 0");
		CheckObstacle(ctx, world.FindEntityByName("AppleTradeContainer3"), "AppleTradeContainer3", "0 0.15 0");

		ctx.Equal(3, s_aTraderTables.Count(), "All three apple trader tables were found");
		foreach (int index, EntityID id : s_aTraderTables)
			CheckObstacle(ctx, world.FindEntityByID(id), "Apple trader table " + index, "0 0.75 0");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckObstacle(EL_TestContext ctx, IEntity entity, string name, vector offset)
	{
		World world = GetGame().GetWorld();
		ctx.NotNull(entity, name + " exists");
		if (!entity)
			return;

		Physics physics = entity.GetPhysics();
		ctx.True(physics != null, name + " has physics");
		if (physics)
			ctx.True(physics.GetNumGeoms() > 0, name + " has collision geometry");

		TraceSphere trace = new TraceSphere();
		trace.Radius = 0.25;
		trace.Start = entity.GetOrigin() + offset + "0 0 -2";
		trace.End = entity.GetOrigin() + offset + "0 0 2";
		trace.Flags = TraceFlags.ENTS;
		trace.LayerMask = -1;
		trace.Include = entity;
		float fraction = world.TraceMove(trace, null);
		EL_Debug.Log("CollisionTest", string.Format("%1 sweep fraction=%2", name, fraction));
		ctx.True(fraction > 0 && fraction < 1 && trace.TraceEnt == entity, name + " blocks a sphere sweep");
	}
}
