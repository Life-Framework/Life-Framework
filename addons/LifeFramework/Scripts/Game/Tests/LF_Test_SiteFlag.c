// red-proof: `test --no-build --red LF_Test_SiteFlag` (RedProof() skips the capture events): the board stays unclaimed and "a capture sets the holder name" fails; before LF_SiteFlagComponent existed the file did not compile.
// tier: WORLD
//! sites/site-flag (#2685, gang war #1022): the flag pole of a contested site holds its holder's
//! organisation name in one replicated string. A capture on its site sets it; a second capture
//! by the same holder changes nothing and does not bump replication; another site's capture is
//! ignored; a released lease clears it on the next reconcile. The prefab spawns with its
//! plate child, so the board has a face to paint.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SiteFlag : LF_Test
{
	static const ResourceName FLAG_PREFAB = "{DDE764E40C17C5DC}Prefabs/Sites/LF_SiteFlag.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "sites/site-flag";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		RunWiped(ctx);
		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWiped(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_SiteFlagComponent.ResetStatics();

		string site = LF_TestAnnex.HIDEOUT_ID;
		LF_Organization orgA = LF_OrganizationManager.Create("flag-founder-a", "Flag Crew A");
		LF_Organization orgB = LF_OrganizationManager.Create("flag-founder-b", "Flag Crew B");
		ctx.NotNull(orgA, "organization A exists");
		ctx.NotNull(orgB, "organization B exists");
		if (!orgA || !orgB)
			return;

		IEntity entity = LF_ExploitGuardsWorldFixture.Spawn(FLAG_PREFAB, "0 0 0");
		ctx.NotNull(entity, "the site flag prefab spawns");
		if (!entity)
			return;

		LF_SiteFlagComponent flag = LF_SiteFlagComponent.Cast(entity.FindComponent(LF_SiteFlagComponent));
		ctx.NotNull(flag, "the flag has its component");
		ctx.NotNull(entity.GetChildren(), "the flag has a plate child to paint");
		if (!flag)
		{
			LF_ExploitGuardsWorldFixture.Despawn(entity);
			return;
		}

		flag.BindHideout(site);
		ctx.EqualStr("", flag.GetHolderName(), "an unleased site reads unclaimed");
		ctx.Equal(0, flag.GetBumpCount(), "nothing was bumped yet");

		string idA = orgA.GetId();
		string idB = orgB.GetId();
		ctx.True(LF_Hideout.Assign(site, idA), "A leases the hideout");

		// 1. A capture event sets the holder name.
		if (!RedProof())
			LF_EventBus.Publish(LF_OnSiteCaptured.Create(site, "", idA));

		ctx.EqualStr("Flag Crew A", flag.GetHolderName(), "a capture sets the holder name");
		ctx.Equal(1, flag.GetBumpCount(), "the change bumped once");

		// 2. The same holder again: no change, no bump.
		if (!RedProof())
			LF_EventBus.Publish(LF_OnSiteCaptured.Create(site, "", idA));

		ctx.EqualStr("Flag Crew A", flag.GetHolderName(), "the same holder keeps the name");
		ctx.Equal(1, flag.GetBumpCount(), "the same holder does not bump");

		// 3. Another site's capture does not touch this board.
		if (!RedProof())
			LF_EventBus.Publish(LF_OnSiteCaptured.Create("some-other-site", idA, idB));

		ctx.EqualStr("Flag Crew A", flag.GetHolderName(), "another site's capture is ignored");
		ctx.Equal(1, flag.GetBumpCount(), "another site's capture does not bump");

		// 4. A hand-over to B renames the board.
		ctx.True(LF_Hideout.HandOver(site, idB, "captured"), "B takes the hideout");
		if (!RedProof())
			LF_EventBus.Publish(LF_OnSiteCaptured.Create(site, idA, idB));

		ctx.EqualStr("Flag Crew B", flag.GetHolderName(), "the new holder's name is on the board");
		ctx.Equal(2, flag.GetBumpCount(), "the second holder bumped once more");
		ctx.False(flag.Reconcile(), "reconciling with the lessee changes nothing");

		// 5. A released lease reads unclaimed on the next reconcile (a lapse or give-up has no capture).
		ctx.True(LF_Hideout.Release(site, "test"), "B's lease ends");
		ctx.True(flag.Reconcile(), "the reconcile sees the released lease");
		ctx.EqualStr("", flag.GetHolderName(), "a released site reads unclaimed");
		ctx.Equal(3, flag.GetBumpCount(), "the release bumped once");

		LF_ExploitGuardsWorldFixture.Despawn(entity);
		LF_SiteFlagComponent.ResetStatics();
		LF_Hideout.Reset();
		LF_OrganizationManager.Reset();
	}
}
