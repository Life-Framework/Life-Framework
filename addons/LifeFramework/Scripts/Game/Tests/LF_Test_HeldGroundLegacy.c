// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_HeldGroundLegacy,...`:
// LF_Hideout.ApplyAll keeping the live held ground (a site with no row) failed `[LFTEST] FAIL persist/held-ground-legacy`,
// `- the site loads unheld (version 33 load 1) (expected '', got '<org id>')`, `- the site loads with no points
// (version 23 load 1) (expected 0, got 30)`. Restored; green.
// tier: PERSISTENCE
//! #751: a save from before the "heldground" payload (the version before it, and version 23, the e2e
//! legacy save) has no held-ground rows. Each loads, twice, with every site unheld (no holder, no hold
//! time, no points) and the "hideout" lease and lock unchanged. The test puts back the stores it found.
class LF_Test_HeldGroundLegacy : LF_Test
{
	protected static const string SITE = "held-ground-legacy-site";
	protected static const string FOUNDER = "held-ground-legacy-founder";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/held-ground-legacy";
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
		LF_Organization holder = LF_OrganizationManager.Create(FOUNDER, "Legacy Holders");
		ctx.NotNull(holder, "the holding organization is founded");
		if (holder)
		{
			ctx.True(LF_Hideout.Assign(SITE, holder.GetId()), "the organization leases the site");
			ctx.True(LF_Hideout.TryUnlock(FOUNDER, SITE), "the founder opens the site");
			// The version before "heldground" is the newest save without it.
			CheckVersion(ctx, persistence, holder.GetId(), LF_PersistenceComponentSerializer.PayloadSinceVersion("heldground") - 1);
			CheckVersion(ctx, persistence, holder.GetId(), 23);
		}

		LF_Hideout.Release(SITE);
		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckVersion(LF_TestContext ctx, LF_PersistenceComponent persistence, string holderId, int version)
	{
		// The live site is held; a save written in the old shape cannot hold it.
		LF_Hideout.SetHeldGround(SITE, holderId, 7200, 30);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, saveContext, version);
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(SITE), string.Format("the version %1 save carries the hideout lease", version));
		ctx.False(saved.Contains("m_iHoldPoints"), string.Format("the version %1 save has no held-ground rows", version));

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (version %1 load %2)", version, pass);
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the old save parses" + tag);
			LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);

			LF_HeldGroundRow held = LF_Hideout.HeldGroundOf(SITE);
			ctx.NotNull(held, "the site is back" + tag);
			if (!held)
				return;

			ctx.EqualStr("", held.m_sHolderOrgId, "the site loads unheld" + tag);
			ctx.Equal(0, held.m_iHoldSeconds, "the site loads with no hold time" + tag);
			ctx.Equal(0, held.m_iHoldPoints, "the site loads with no points" + tag);
			ctx.EqualStr(holderId, LF_Hideout.GetOwner(SITE), "the hideout lease is unchanged" + tag);
			ctx.False(LF_Hideout.IsLocked(SITE), "the hideout lock is unchanged" + tag);
			ctx.Equal(0, LF_Hideout.ExportHeldGround().Count(), "no site is held after the old load" + tag);
		}
	}
}
