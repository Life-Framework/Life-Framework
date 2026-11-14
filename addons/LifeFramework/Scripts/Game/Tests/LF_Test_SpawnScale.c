// red-proof: 2026-11-11, with LF_Utils.SpawnEntityPrefab's scale forced to 1 (before #740),
//   cli test --no-build --only LF_Test_SpawnScale:
//   [LFTEST]   - {9CB52FAF71DA568F}Prefabs/Crafting/Gunpowder.et is its designed size (largest side 12.2721 m, want 0.02 to 0.3)
//   [LFTEST]   - {F0F6577FD74790D2}Prefabs/Crafting/Linen.et is its designed size (largest side 0.678101 m, want 0.05 to 0.4)
//   [LFTEST]   - {65DE48DCC1D4DD01}Prefabs/Items/Roleplay/LF_MoneyBag.et is its designed size (largest side 1.36045 m, want 0.4 to 0.6)
// Restored; green (gunpowder 0.15 m heap, linen 0.2 m, bag 0.55 m, drill 0.51 m).
// tier: WORLD
//! spawn/prefab-scale: every mod prefab with its own root scale comes out of LF_Utils.SpawnEntityPrefab at that
//! scale and its designed size. Until #740 a spawn reset the scale to 1: the money bag (0.4) was a 1.36 m sack,
//! gunpowder (a vanilla gravel pile at 0.012) would have dropped as a full gravel heap, linen (a sandbag at 0.3)
//! as a full sandbag. These are all the scaled prefabs the addon spawns; the scaled vanilla gravel piles, bush
//! and poplar are only ever placed in layers.
class LF_Test_SpawnScale : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "spawn/prefab-scale";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		vector at = Vector(300, world.GetSurfaceY(300, 205), 205);
		Check(ctx, "{9CB52FAF71DA568F}Prefabs/Crafting/Gunpowder.et", at, 0.02, 0.3);
		Check(ctx, "{F0F6577FD74790D2}Prefabs/Crafting/Linen.et", at + "1.5 0 0", 0.05, 0.4);
		Check(ctx, LF_Bank.BAG_PREFAB, at + "3 0 0", 0.4, 0.6);
		Check(ctx, LF_Bank.DRILL_PREFAB, at + "4.5 0 0", 0.45, 0.7);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns the prefab, checks its scale and that its largest side is between least and most metres.
	protected void Check(LF_TestContext ctx, ResourceName prefab, vector at, float least, float most)
	{
		IEntity spawned = LF_Utils.SpawnEntityPrefab(prefab, at);
		ctx.NotNull(spawned, string.Format("%1 spawns", prefab));
		if (!spawned)
			return;

		float scale = LF_Utils.PrefabScale(Resource.Load(prefab));
		ctx.EqualFloat(scale, spawned.GetScale(), 0.001, string.Format("%1 keeps its prefab scale", prefab));
		vector mins;
		vector maxs;
		spawned.GetWorldBounds(mins, maxs);
		vector size = maxs - mins;
		float largest = Math.Max(size[0], Math.Max(size[1], size[2]));
		LF_Debug.Info("Test", string.Format("spawned %1 scale %2 size %3", prefab, spawned.GetScale(), size));
		ctx.True(largest >= least && largest <= most, string.Format("%1 is its designed size (largest side %2 m, want %3 to %4)", prefab, largest, least, most));
		SCR_EntityHelper.DeleteEntityAndChildren(spawned);
	}
}
