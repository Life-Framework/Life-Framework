// red-proof: Observed 2026-11-19: `--no-build --only LF_Test_ReaperMesh` failed on the unchanged prefab (the CultivatorWreck_01 stand-in: width 2.64 m, drawbar 1.18 m), then passed with the new mesh; the batched `--red` run printed `RED-OK harvest/reaper-mesh (fails with the hook on, 2 assertions)`.
//! harvest/reaper-mesh (#3292): the reaper prefab is the sickle-bar mower built for it, not the wreck stand-in. A 3 m
//! cutter bar, a frame behind it, wheels on the ground and a drawbar that ends at the UAZ469's rear (the hitch offset
//! of Configs/Harvest/LF_Hitches.conf puts the origin 1.53 m past the vehicle's rear bound, and the mesh faces the
//! vehicle along +Z). The test deletes what it spawns.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ReaperMesh : LF_Test
{
	protected static const ResourceName REAPER = "{681247C1216EB815}Prefabs/Harvest/LF_Reaper_01.et";
	protected static const vector SPOT = "40 0 40";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "harvest/reaper-mesh";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ResourceName prefab = REAPER;
		if (RedProof())
			prefab = "";

		vector at = SPOT;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity entity = LF_Utils.SpawnEntityPrefab(prefab, at);
		ctx.NotNull(entity, "the reaper spawns");
		if (!entity)
			return;

		ctx.True(entity.GetVObject() != null, "the reaper has a real mesh (a MeshObject without an .xob is invisible)");

		vector mins;
		vector maxs;
		entity.GetWorldBounds(mins, maxs);
		float width = maxs[0] - mins[0];
		float depth = maxs[2] - mins[2];
		float height = maxs[1] - mins[1];
		ctx.True(width > 3.0 && width < 3.5, string.Format("the reaper is a 3 m bar plus its shoes wide (%1 m)", width));
		ctx.True(depth > 2.3 && depth < 3.1, string.Format("the reaper is bar, frame and drawbar deep (%1 m)", depth));
		ctx.True(height > 1.1 && height < 1.9, string.Format("the reaper is a hopper tall (%1 m)", height));

		// Wheels and skid shoes rest on the ground: nothing sinks, nothing floats.
		float lowest = mins[1] - at[1];
		ctx.True(lowest > -0.06 && lowest < 0.06, string.Format("the reaper rests on the ground, lowest point %1 m", lowest));

		// The drawbar points at the towing vehicle (+Z) and ends where the UAZ469's rear bumper is.
		float drawbar = maxs[2] - at[2];
		ctx.True(drawbar > 1.4 && drawbar < 1.65, string.Format("the drawbar ends %1 m ahead of the origin, at the hitch", drawbar));

		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}
}
