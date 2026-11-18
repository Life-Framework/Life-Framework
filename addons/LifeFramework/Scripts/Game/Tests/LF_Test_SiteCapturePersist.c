// red-proof: 2026-11-18 `tools/cli test --no-build --red LF_Test_SiteCaptureHeld,LF_Test_SiteCapturePersist` printed RED PROOF OK: with the hook on (no capture) "one captured site before the save", "the captured site is still captured" and the old-save checks fail.
// tier: PERSISTENCE
//! persist/site-capture (#2816, "tenancy" payload): an organization that holds a base lease and a
//! captured site gets both back through the real save path, twice, with the captured one still
//! captured, the base still a base, and the captured site still no base place (the "hideout" load
//! gives every lease one, the tenancy load takes it back). A save from before the payload loads every
//! lease as a base lease. A row for a site with no lease is dropped.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_SiteCapturePersist : LF_Test
{
	protected static const string BASE_SITE = "site-capture-trip-base";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/site-capture";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		RunSeeded(ctx, persistence);
		LF_Hideout.Release(BASE_SITE);
		LF_Hideout.Release(LF_TestAnnex.HIDEOUT_ID);
		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSeeded(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		string captured = LF_TestAnnex.HIDEOUT_ID;
		string baseKind = LF_PlacementZones.ORG_BASE_KIND;
		LF_Organization org = LF_OrganizationManager.Create("site-capture-trip-founder", "Capture Trip Crew");
		ctx.NotNull(org, "the capturing organization is founded");
		if (!org)
			return;

		string orgId = org.GetId();
		ctx.True(LF_Hideout.Assign(BASE_SITE, orgId), "the organization leases its base");
		if (!RedProof())
			ctx.True(LF_Hideout.HandOver(captured, orgId, "captured"), "the organization captures a second site");

		ctx.Equal(1, LF_Hideout.SitesOf(orgId, true).Count(), "one captured site before the save");
		ctx.Equal(1, LF_Hideout.ExportTenancy().Count(), "exactly one tenancy row is written");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("m_eKind"), "the save carries the tenancy rows");

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (load %1)", pass);
			// Wipe, then load: the leases only come back from the save.
			LF_Hideout.Reset();
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses" + tag);
			LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);
			ctx.EqualStr(orgId, LF_Hideout.GetOwner(BASE_SITE), "the base lease is back" + tag);
			ctx.EqualStr(orgId, LF_Hideout.GetOwner(captured), "the captured lease is back" + tag);
			ctx.True(LF_Hideout.IsCaptured(captured), "the captured site is still captured" + tag);
			ctx.False(LF_Hideout.IsCaptured(BASE_SITE), "the base lease is still a base lease" + tag);
			ctx.True(LF_Places.Find(baseKind, orgId) == null, "the captured site holds no base place" + tag);
		}

		// A save from before the payload loads every lease as a base lease.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		int before = LF_PersistenceComponentSerializer.PayloadSinceVersion("tenancy") - 1;
		ctx.True(before > 0, "the payload is registered");
		ctx.True(LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, oldContext, before) == ESerializeResult.OK, "the store serializes as the version before the payload");
		LF_Hideout.Reset();
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		oldLoad.LoadFromString(oldContext.SaveToString());
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.EqualStr(orgId, LF_Hideout.GetOwner(captured), "the lease is back from the old save");
		ctx.False(LF_Hideout.IsCaptured(captured), "an old save loads every lease as a base lease");

		// A row for a site with no lease is dropped, and the others still apply.
		array<ref LF_HideoutTenancyRow> rows = {};
		rows.Insert(LF_HideoutTenancyRow.Create("site-capture-trip-nowhere", LF_EHideoutTenancy.CAPTURED));
		rows.Insert(LF_HideoutTenancyRow.Create(captured, LF_EHideoutTenancy.CAPTURED));
		ctx.Equal(1, LF_Hideout.ApplyTenancy(rows), "only the row with a lease applies");
		ctx.True(LF_Hideout.IsCaptured(captured), "the good row marks its site");
		ctx.Equal(0, LF_Hideout.ApplyTenancy(null), "an empty apply applies nothing");
		ctx.False(LF_Hideout.IsCaptured(captured), "an empty apply resets every site to a base lease");
	}
}
