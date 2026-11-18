// red-proof: 2026-09-19 dedicated-server run failed both apple crate physics and
// sweep assertions (fraction=1) with the world entity in Terrain.layer. Moving
// world initialization to default.layer gave both sweeps fraction=0.386069.
// order-proof (#2699): a name-sorted --tier all run failed this test. LF_Test_BreachChargeExploit
// left a character's vest and jacket at <75,0,75> and LF_Test_Siren left a police vehicle with its
// cargo props; both tests now delete what they spawn. Items a body carries are skipped
// (the save-write test keeps three bodies alive on purpose). Green with this test last and in
// full name order.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_DebugWorldCollision : LF_Test
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

		// An item sitting in an inventory slot (a worn jacket, a backpack, a drill in it) is carried
		// equipment, not a prop of the world: it has no collision of its own. A test that keeps a body
		// alive on purpose (e2e/pass1-write-save holds three through the shutdown save) leaves such
		// items behind; they must not fail this scan whatever order the suite runs in (#2699).
		InventoryItemComponent carried = InventoryItemComponent.Cast(entity.FindComponent(InventoryItemComponent));
		if (carried && carried.GetParentSlot())
			return true;

		// Likewise a part attached to a character's body (the vest's belt dummy hangs on the vest slot).
		IEntity parent = entity.GetParent();
		while (parent)
		{
			if (parent.Type().IsInherited(SCR_ChimeraCharacter))
				return true;

			parent = parent.GetParent();
		}

		s_iEntities++;
		EntityPrefabData prefab = entity.GetPrefabData();
		if (prefab && prefab.GetPrefabName().Contains("TableRecreation_01.et") && vector.Distance(entity.GetOrigin(), s_vTraderOrigin) < 5)
			s_aTraderTables.Insert(entity.GetID());

		string label = entity.GetName();
		if (label.IsEmpty() && prefab)
			label = prefab.GetPrefabName() + " at " + entity.GetOrigin().ToString();
		if (label.IsEmpty())
			label = type.ToString() + " at " + entity.GetOrigin().ToString();

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
	override void Run(LF_TestContext ctx)
	{
		s_iEntities = 0;
		s_iMissingTraceable = 0;
		s_aFailures = new array<string>();
		s_aTraderTables = new array<EntityID>();
		World world = GetGame().GetWorld();
		IEntity trader = world.FindEntityByName(LF_TestAnnex.APPLE_TRADER);
		ctx.NotNull(trader, "Annex_AppleTrader exists");
		if (!trader)
			return;
		s_vTraderOrigin = trader.GetOrigin();

		vector center = "256 0 256"; // town-layout-ok: a whole-world sweep, not a position
		GetGame().GetWorld().QueryEntitiesBySphere(center, 500, InspectEntity, null, EQueryEntitiesFlags.ALL);

		ctx.True(s_iEntities > 0, "DebugWorld collision scan found entities");
		ctx.Equal(0, s_iMissingTraceable, "DebugWorld entities are traceable: " + s_aFailures.ToString());
		CheckObstacle(ctx, world.FindEntityByName(LF_TestAnnex.APPLE_TRADE_CONTAINER), LF_TestAnnex.APPLE_TRADE_CONTAINER, "0 0.15 0");
		CheckObstacle(ctx, world.FindEntityByName(LF_TestAnnex.APPLE_TRADE_CONTAINER3), LF_TestAnnex.APPLE_TRADE_CONTAINER3, "0 0.15 0");

		ctx.Equal(3, s_aTraderTables.Count(), "All three apple trader tables were found");
		foreach (int index, EntityID id : s_aTraderTables)
			CheckObstacle(ctx, world.FindEntityByID(id), "Apple trader table " + index, "0 0.75 0");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckObstacle(LF_TestContext ctx, IEntity entity, string name, vector offset)
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
		LF_Debug.Log("CollisionTest", string.Format("%1 sweep fraction=%2", name, fraction));
		ctx.True(fraction > 0 && fraction < 1 && trace.TraceEnt == entity, name + " blocks a sphere sweep");
	}
}
