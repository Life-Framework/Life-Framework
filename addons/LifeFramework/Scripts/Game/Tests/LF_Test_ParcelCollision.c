// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_ParcelCollision` and RestOnGround never lifting:
// FAIL a sunk parcel is lifted; the lifted parcel rests on the ground, not under it. Restored; green.

// red-proof: observed 2026-11-15 with `tools\cli test --no-build --only LF_Test_ParcelCollision` and the parcel body back on the
// old `ItemFireView` layer: FAIL the parcel is on the same collision layer as a vanilla box prop (box 10535680, parcel 20480). Restored; green.

// tier: WORLD
//! A courier parcel has a solid collider: a trace straight down onto it hits it. It is a small
//! box (longest side 0.15 to 0.5 m) on the same collision layer as a vanilla box prop, so a
//! walking player is blocked by it.
class LF_Test_ParcelCollision : LF_Test
{
	protected static const ResourceName VANILLA_BOX = "{2B1D0128ED660BE9}Prefabs/Props/Crates/BoxWooden_01.et";

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

		// A 1989 post parcel is a shoebox to small box: no side over half a metre (the old held
		// parcel read as 0.6 m and hung at knee height).
		vector mins, maxs;
		parcel.GetWorldBounds(mins, maxs);
		float longest = Math.Max(maxs[0] - mins[0], Math.Max(maxs[1] - mins[1], maxs[2] - mins[2]));
		float scale = parcel.GetScale();
		float side = longest * scale;
		ctx.True(side > 0.15 && side <= 0.5, string.Format("the parcel is a small box (longest side %1 m = bounds %2 x scale %3, want 0.15 to 0.5)", side, longest, scale));

		Physics physics = parcel.GetPhysics();
		ctx.True(physics != null, "the parcel has a physics body");
		LF_Debug.Info("Jobs", string.Format("parcel physics: %1 layer=%2", physics != null, LF_ParcelComponent.LayerOf(parcel)));

		// The parcel blocks a walking player like a vanilla small box prop: same collision layer.
		Resource boxResource = Resource.Load(VANILLA_BOX);
		ctx.True(boxResource.IsValid(), "the vanilla box prop loads");
		if (boxResource.IsValid())
		{
			IEntity box = GetGame().SpawnEntityPrefab(boxResource, world, params);
			ctx.True(box != null, "the vanilla box prop spawns");
			if (box)
			{
				int boxLayer = LF_ParcelComponent.LayerOf(box);
				int parcelLayer = LF_ParcelComponent.LayerOf(parcel);
				LF_Debug.Info("Jobs", string.Format("vanilla box layer=%1, parcel layer=%2", boxLayer, parcelLayer));
				ctx.True(boxLayer == parcelLayer, string.Format("the parcel is on the same collision layer as a vanilla box prop (box %1, parcel %2)", boxLayer, parcelLayer));
				SCR_EntityHelper.DeleteEntityAndChildren(box);
			}
		}

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
