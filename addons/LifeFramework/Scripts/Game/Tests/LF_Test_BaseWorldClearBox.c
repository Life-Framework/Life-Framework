// red-proof: 2026-11-21 `--red LF_Test_BaseWorldClearBox` with the hide skipped, "the pallet in the box is hidden" fails.
//! placement/base-world-clear-box (#3924): the outdoor clear hides the listed prefab inside its own box and leaves the
//! same prefab outside the box, and a prop of another prefab inside it, in place.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BaseWorldClearBox : LF_Test
{
	protected static const ResourceName PALLET = "{D7F69BF7D2DCA0A8}Prefabs/Props/Industrial/Pallets/Pallet_01.et";
	protected static const ResourceName CRATE = "{ABA40158860E5B6B}Prefabs/Props/Crates/Crate_01_brown.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "placement/base-world-clear-box";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		float y = world.GetSurfaceY(430, 90);
		IEntity inside = LF_Utils.SpawnEntityPrefab(PALLET, Vector(430, y, 90));
		IEntity otherKind = LF_Utils.SpawnEntityPrefab(CRATE, Vector(431, y, 91));
		IEntity outside = LF_Utils.SpawnEntityPrefab(PALLET, Vector(450, y, 90));
		ctx.True(inside && otherKind && outside, "the three props spawned");
		if (!inside || !otherKind || !outside)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(inside);
			SCR_EntityHelper.DeleteEntityAndChildren(otherKind);
			SCR_EntityHelper.DeleteEntityAndChildren(outside);
			return;
		}

		// A 6 x 6 m box turned 30 degrees, standing on the ground under (430, 90): the pallet and the crate are in it.
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(30, 0, 0), mat);
		mat[3] = Vector(430, y, 90);
		array<string> clear = {PALLET};
		int hidden = 0;
		if (!RedProof())
			hidden = LF_BaseWorldClear.HideInBox(world, Vector(-3, -1, -3), Vector(3, 4, 3), mat, clear, "test");

		ctx.Equal(1, hidden, "the pallet in the box is hidden");
		ctx.True(LF_FelledTreesReplication.IsHidden(inside), "the pallet in the box is not drawn");
		ctx.False(LF_FelledTreesReplication.IsHidden(otherKind), "a prop of another prefab in the box stays");
		ctx.False(LF_FelledTreesReplication.IsHidden(outside), "the same prefab outside the box stays");
		ctx.Equal(0, LF_BaseWorldClear.HideInBox(world, Vector(-3, -1, -3), Vector(3, 4, 3), mat, clear, "test"), "a second apply hides nothing more");

		SCR_EntityHelper.DeleteEntityAndChildren(inside);
		SCR_EntityHelper.DeleteEntityAndChildren(otherKind);
		SCR_EntityHelper.DeleteEntityAndChildren(outside);
	}
}
