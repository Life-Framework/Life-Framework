// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_ParcelCollision` and RestOnGround never lifting:
// FAIL a sunk parcel is lifted; the lifted parcel rests on the ground, not under it. Restored; green.

// tier: WORLD
//! A courier parcel has a solid collider: a trace straight down onto it hits it.
class LF_Test_ParcelCollision : LF_Test
{
	override string GetName()
	{
		return "jobs/parcel-collision";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource resource = Resource.Load(LF_CivilianJob.ParcelPrefab());
		ctx.True(resource.IsValid(), "the parcel prefab loads");
		if (!resource.IsValid())
			return;

		BaseWorld world = GetGame().GetWorld();
		vector at = "200 0 200";
		at[1] = world.GetSurfaceY(at[0], at[2]) + 20;
		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = at;
		IEntity parcel = GetGame().SpawnEntityPrefab(resource, world, params);
		ctx.True(parcel != null, "the parcel spawns");
		if (!parcel)
			return;

		Physics physics = parcel.GetPhysics();
		ctx.True(physics != null, "the parcel has a physics body");
		LF_Debug.Info("Jobs", string.Format("parcel physics: %1 layer=%2", physics != null, LF_ParcelComponent.LayerOf(parcel)));

		TraceParam trace = new TraceParam();
		trace.Start = at + "0 3 0";
		trace.End = at - "0 3 0";
		trace.Flags = TraceFlags.ENTS;
		trace.LayerMask = -1;
		world.TraceMove(trace, null);
		ctx.True(trace.TraceEnt == parcel, "a trace down onto the parcel hits it");
		SCR_EntityHelper.DeleteEntityAndChildren(parcel);

		// A parcel that sank 0.6 m into the ground comes back up onto it.
		float ground = world.GetSurfaceY(200, 200);
		vector buried = Vector(200, ground - 0.6, 200);
		params.Transform[3] = buried;
		IEntity sunk = GetGame().SpawnEntityPrefab(resource, world, params);
		ctx.True(sunk != null, "the sunk parcel spawns");
		if (!sunk)
			return;

		ctx.True(LF_ParcelComponent.RestOnGround(sunk), "a sunk parcel is lifted");
		ctx.True(sunk.GetOrigin()[1] >= ground - 0.05, "the lifted parcel rests on the ground, not under it");
		ctx.False(LF_ParcelComponent.RestOnGround(sunk), "a resting parcel stays put");
		SCR_EntityHelper.DeleteEntityAndChildren(sunk);
	}
}
