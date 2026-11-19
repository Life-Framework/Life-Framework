// red-proof: Observed 2026-11-19 with the batched red run `--no-build --red LF_Test_WheatPatchMeshes`: `RED-OK harvest/wheat-patch-meshes (fails with the hook on, 7 assertions)`; the hook leaves the prefab empty, so nothing spawns.
//! harvest/wheat-patch-meshes (#3290): the wheat patch and the stubble patch are real meshes of the right size
//! (a 3 m cell, about a metre of wheat, about 30 cm of stubble), and the field config draws them in place of the
//! first-cut placeholder plants. The test deletes both entities.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_WheatPatchMeshes : LF_Test
{
	protected static const ResourceName PATCH = "{C2438FA7D428E467}Prefabs/Harvest/LF_WheatPatch_01.et";
	protected static const ResourceName STUBBLE = "{CCEFC0D6DC31ABB3}Prefabs/Harvest/LF_WheatStubble_01.et";
	protected static const vector SPOT = "40 0 40";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "harvest/wheat-patch-meshes";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckPatch(ctx, PATCH, "the wheat patch", 0.8, 1.4);
		CheckPatch(ctx, STUBBLE, "the stubble patch", 0.15, 0.5);

		LF_WheatFieldsConfig config = LF_WheatFieldsConfig.Load();
		ctx.NotNull(config, "the wheat field config loads");
		if (config)
		{
			ctx.True(config.m_PatchPrefab == PATCH, "a standing cell draws the wheat patch prefab");
			ctx.True(config.m_StubblePrefab == STUBBLE, "a cut cell draws the stubble patch prefab");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the prefab on the ground, checks it has a mesh no wider than a cell and a believable height, deletes it.
	protected void CheckPatch(LF_TestContext ctx, ResourceName prefab, string label, float minHeight, float maxHeight)
	{
		if (RedProof())
			prefab = "";

		vector at = SPOT;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity entity = LF_Utils.SpawnEntityPrefab(prefab, at);
		ctx.NotNull(entity, label + " spawns");
		if (!entity)
			return;

		ctx.True(entity.GetVObject() != null, label + " has a real mesh (a MeshObject without an .xob is invisible)");

		vector mins;
		vector maxs;
		entity.GetWorldBounds(mins, maxs);
		float width = maxs[0] - mins[0];
		float depth = maxs[2] - mins[2];
		float height = maxs[1] - mins[1];
		ctx.True(width > 2.8 && width < 3.6, string.Format("%1 is about one 3 m cell wide (%2 m)", label, width));
		ctx.True(depth > 2.8 && depth < 3.6, string.Format("%1 is about one 3 m cell deep (%2 m)", label, depth));
		ctx.True(height > minHeight && height < maxHeight, string.Format("%1 stands %2 m tall, expected %3 to %4", label, height, minHeight, maxHeight));
		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}
}
