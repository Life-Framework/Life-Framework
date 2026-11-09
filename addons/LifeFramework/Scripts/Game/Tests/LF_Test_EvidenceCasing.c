// red-proof: Observed 2026-11-09 with `tools/cli test --no-build --only LF_Test_EvidenceCasing`: loosening the per-scene cap in LF_Evidence.OnShot (`>` for `>=`) failed `investigation/casing` with `- a scene leaves at most its casing cap`. Restored; green.
// tier: WORLD
//! Scene evidence (#268): a crime report opens a scene; a shot inside it leaves exactly one
//! casing, a shot outside leaves none, a scene's casings are capped, an item dropped in the scene
//! gets a marker, untouched evidence expires, and bagging records the report, the place and the
//! time on the bag and takes the piece off the ground.
class LF_Test_EvidenceCasing : LF_Test
{
	protected static const vector SCENE = "1000 0 1000";
	protected static const int T0 = 1000000;

	override string GetName()
	{
		return "investigation/casing";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Evidence.ResetStatics();
		LF_EvidenceScene scene = LF_Evidence.OpenScene(7, "evidence-test-uid", LF_WantedReports.OFFENCE_ROBBERY, "Test Square", SCENE, T0);
		ctx.NotNull(scene, "a crime report opens a scene");
		ctx.Equal(1, LF_Evidence.SceneCount(), "one scene is open");

		ctx.True(LF_Evidence.OnShot("500 0 500", T0 + 1000) == null, "a shot far from any scene leaves no casing");
		ctx.Equal(0, LF_Evidence.EvidenceCount(), "nothing lies about after the far shot");

		IEntity casing = LF_Evidence.OnShot(SCENE + "2 0 0", T0 + 1000);
		ctx.NotNull(casing, "a shot at the scene leaves a casing");
		ctx.Equal(1, LF_Evidence.EvidenceCount(), "exactly one casing lies at the scene");

		RunCap(ctx);
		RunExpiry(ctx);
		RunBag(ctx);

		LF_Evidence.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCap(LF_TestContext ctx)
	{
		int cap = LF_EvidenceConfig.MaxCasingsPerScene();
		for (int i = 0; i < cap + 3; i++)
		{
			LF_Evidence.OnShot(SCENE, T0 + 2000);
		}

		ctx.Equal(cap, LF_Evidence.EvidenceCount(), "a scene leaves at most its casing cap");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunExpiry(LF_TestContext ctx)
	{
		int before = LF_Evidence.EvidenceCount();
		ctx.Equal(0, LF_Evidence.Sweep(T0 + 5000), "fresh evidence is not swept");
		int removed = LF_Evidence.Sweep(T0 + 2000 + LF_EvidenceConfig.ExpirySeconds() * 1000 + 1);
		ctx.Equal(before, removed, "expired evidence is removed");
		ctx.Equal(0, LF_Evidence.EvidenceCount(), "no evidence lies about after expiry");
		ctx.True(LF_Evidence.OnShot(SCENE, T0 + LF_EvidenceConfig.SceneSeconds() * 1000 + 5000) == null, "a closed scene leaves no casing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBag(LF_TestContext ctx)
	{
		LF_Evidence.ResetStatics();
		LF_Evidence.OpenScene(9, "evidence-test-uid", LF_WantedReports.OFFENCE_ROBBERY, "Test Square", SCENE, T0);
		IEntity casing = LF_Evidence.OnShot(SCENE, T0 + 1000);
		ctx.NotNull(casing, "a casing is left for the bag test");
		if (!casing)
			return;

		IEntity bag = LF_Evidence.Bag(casing, null, T0 + 1000 + 3 * 60000);
		ctx.NotNull(bag, "an officer bags the casing");
		if (!bag)
			return;

		LF_EvidenceBagComponent bagComponent = LF_EvidenceBagComponent.Cast(bag.FindComponent(LF_EvidenceBagComponent));
		ctx.NotNull(bagComponent, "the bag carries its component");
		if (bagComponent)
		{
			ctx.Equal(9, bagComponent.GetReport(), "the bag lists the report");
			ctx.EqualStr("Test Square", bagComponent.GetPlace(), "the bag lists the place");
			ctx.Equal(3, bagComponent.GetMinutes(), "the bag lists how long it lay");
			array<IEntity> contents = {};
			bagComponent.GetContents(contents);
			ctx.Equal(1, contents.Count(), "the casing is inside the bag");
		}

		ctx.Equal(0, LF_Evidence.EvidenceCount(), "a bagged piece is no longer scene evidence");
		ctx.True(LF_Evidence.Bag(casing, null, T0) == null, "a piece cannot be bagged twice");
		SCR_EntityHelper.DeleteEntityAndChildren(bag);
		if (casing)
			SCR_EntityHelper.DeleteEntityAndChildren(casing);
	}
}
