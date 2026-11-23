// red-proof: `test --no-build --red LF_Test_SiteFlagCloth` RED-OK (RedProof() skips the captures and reconciles): "a released site flies NONE (expected 0, got 12)"; before #4451 GetFlag did not exist and the file did not compile.
// tier: WORLD
//! sites/site-flag-cloth (#4451, gangs C12): a contested site's pole flies the holding gang's flag.
//! A capture sets the pole's replicated flag number to the holder's LF_OrgFlags flag; a hand-over
//! flies the new holder's; a new flag picked at the wall reaches the pole on the next reconcile; a
//! release sets NONE (the plain cloth). The pole prefab carries its plain cloth beside the plate.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SiteFlagCloth : LF_Test
{
	static const ResourceName FLAG_PREFAB = "{DDE764E40C17C5DC}Prefabs/Sites/LF_SiteFlag.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "sites/site-flag-cloth";
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
		LF_Organization orgA = LF_OrganizationManager.Create("cloth-founder-a", "Cloth Crew A");
		LF_Organization orgB = LF_OrganizationManager.Create("cloth-founder-b", "Cloth Crew B");
		ctx.NotNull(orgA, "organization A exists");
		ctx.NotNull(orgB, "organization B exists");
		if (!orgA || !orgB)
			return;

		string idA = orgA.GetId();
		string idB = orgB.GetId();
		ctx.True(LF_OrgFlags.Set(idA, 3), "A flies flag 3");
		ctx.True(LF_OrgFlags.Set(idB, 7), "B flies flag 7");

		IEntity entity = LF_ExploitGuardsWorldFixture.Spawn(FLAG_PREFAB, "0 0 0");
		ctx.NotNull(entity, "the site flag prefab spawns");
		if (!entity)
			return;

		LF_SiteFlagComponent flag = LF_SiteFlagComponent.Cast(entity.FindComponent(LF_SiteFlagComponent));
		ctx.NotNull(flag, "the pole has its component");
		ctx.NotNull(LF_SiteFlagCloth.PlainCloth(entity), "the pole hangs a plain cloth");
		IEntity plate = LF_SiteFlagCloth.Plate(entity);
		ctx.True(plate && !LF_SiteFlagCloth.IsCloth(plate), "the plate is found beside the cloth");
		ctx.True(LF_SiteFlagCloth.PrefabFor(LF_OrgFlags.NONE).IsEmpty(), "NONE flies the plain cloth");
		if (!flag)
		{
			LF_ExploitGuardsWorldFixture.Despawn(entity);
			return;
		}

		flag.BindHideout(site);
		ctx.Equal(LF_OrgFlags.NONE, flag.GetFlag(), "an unheld site flies no gang flag");

		// 1. A capture flies the holder's flag, in one bump with the name.
		ctx.True(LF_Hideout.Assign(site, idA), "A leases the hideout");
		if (!RedProof())
			LF_EventBus.Publish(LF_OnSiteCaptured.Create(site, "", idA));

		ctx.Equal(3, flag.GetFlag(), "a capture flies the holder's flag");
		ctx.EqualStr("Cloth Crew A", flag.GetHolderName(), "the board names the holder");
		ctx.Equal(1, flag.GetBumpCount(), "name and flag went out in one bump");

		// 2. A hand-over flies the new holder's flag.
		ctx.True(LF_Hideout.HandOver(site, idB, "captured"), "B takes the hideout");
		if (!RedProof())
			LF_EventBus.Publish(LF_OnSiteCaptured.Create(site, idA, idB));

		ctx.Equal(7, flag.GetFlag(), "the new holder's flag flies");

		// 3. B's leader picks another flag at the wall: the pole follows on the next reconcile.
		array<string> online = {};
		string holderName;
		LF_EOrgFlagWallResult picked = LF_OrgFlagWall.Pick("cloth-founder-b", 12, online, holderName);
		ctx.Equal(LF_EOrgFlagWallResult.PICKED, picked, "B's leader picks flag 12 at the wall");
		ctx.Equal(7, flag.GetFlag(), "the pole keeps the old flag until the reconcile");
		if (!RedProof())
			ctx.True(flag.Reconcile(), "the reconcile sees the new flag");

		ctx.Equal(12, flag.GetFlag(), "the pole flies the newly picked flag");
		ctx.EqualStr("Cloth Crew B", flag.GetHolderName(), "the name is unchanged");
		ctx.False(flag.Reconcile(), "a second reconcile changes nothing");

		// 4. A released site flies the plain cloth.
		ctx.True(LF_Hideout.Release(site, "test"), "B's lease ends");
		if (!RedProof())
			ctx.True(flag.Reconcile(), "the reconcile sees the release");

		ctx.Equal(LF_OrgFlags.NONE, flag.GetFlag(), "a released site flies NONE");
		ctx.EqualStr("", flag.GetHolderName(), "a released site reads unclaimed");

		LF_ExploitGuardsWorldFixture.Despawn(entity);
		LF_SiteFlagComponent.ResetStatics();
		LF_Hideout.Reset();
		LF_OrganizationManager.Reset();
	}
}
