// tier: WORLD
// red-proof: before collision flags were added, debug mesh fixtures rendered but
// player movement and projectile traces passed through them; this test catches
// the missing physics/traceability state in the live DebugWorld.
class EL_Test_DebugWorldCollision : EL_Test
{
	protected static int s_iMeshEntities;
	protected static int s_iMissingTraceable;
	protected static ref array<string> s_aFailures = {};

	//------------------------------------------------------------------------------------------------
	static bool InspectMeshEntity(IEntity entity)
	{
		if (!entity)
			return true;

		typename type = entity.Type();
		if (type.IsInherited(GenericTerrainEntity) || type.IsInherited(GenericWorldEntity) || type.IsInherited(SCR_ChimeraCharacter))
			return true;

		if (!entity.GetVObject())
			return true;

		s_iMeshEntities++;
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
		s_iMeshEntities = 0;
		s_iMissingTraceable = 0;
		s_aFailures = new array<string>();

		vector center = "256 0 256";
		GetGame().GetWorld().QueryEntitiesBySphere(center, 500, InspectMeshEntity);

		ctx.True(s_iMeshEntities > 0, "DebugWorld collision scan found mesh entities");
		ctx.Equal(0, s_iMissingTraceable, "DebugWorld mesh entities are traceable: " + s_aFailures.ToString());
	}
}
