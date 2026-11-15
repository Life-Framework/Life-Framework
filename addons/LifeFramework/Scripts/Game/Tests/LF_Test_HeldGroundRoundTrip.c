// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_HeldGroundRoundTrip,...`:
// LF_Hideout.ExportHeldGround writing 0 for m_iHoldPoints failed `[LFTEST] FAIL persist/held-ground`,
// `- the banked points are kept (load 1) (expected 12, got 0)`. Restored; green.
// tier: PERSISTENCE
//! #751 through the real save path ("heldground" payload). A held site comes back with its holder,
//! hold seconds and points after the store is written and read, twice; a leased but unheld site
//! writes no row and loads unheld. The "hideout" leases and locks are unchanged.
class LF_Test_HeldGroundRoundTrip : LF_Test
{
	protected static const string HELD_SITE = "held-ground-trip-held";
	protected static const string QUIET_SITE = "held-ground-trip-quiet";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/held-ground";
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
		LF_Hideout.Release(HELD_SITE);
		LF_Hideout.Release(QUIET_SITE);
		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSeeded(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		string founder = "held-ground-trip-founder";
		LF_Organization holder = LF_OrganizationManager.Create(founder, "Round Trip Holders");
		LF_Organization quiet = LF_OrganizationManager.Create("held-ground-trip-quiet", "Round Trip Quiet");
		ctx.NotNull(holder, "the holding organization is founded");
		ctx.NotNull(quiet, "the quiet organization is founded");
		if (!holder || !quiet)
			return;

		ctx.True(LF_Hideout.Assign(HELD_SITE, holder.GetId()), "the holder leases its site");
		ctx.True(LF_Hideout.TryUnlock(founder, HELD_SITE), "the founder opens the held site");
		ctx.True(LF_Hideout.Assign(QUIET_SITE, quiet.GetId()), "the quiet organization leases its site");
		ctx.True(LF_Hideout.SetHeldGround(HELD_SITE, holder.GetId(), 3600, 12), "the holder holds its site an hour for 12 points");

		bool quietRow = false;
		foreach (LF_HeldGroundRow exported : LF_Hideout.ExportHeldGround())
		{
			if (exported && exported.m_sHideoutId == QUIET_SITE)
				quietRow = true;
		}
		ctx.False(quietRow, "an unheld site writes no row");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("m_iHoldPoints"), "the save carries the held-ground rows");

		// Change the live held ground so the load has to put it back.
		LF_Hideout.SetHeldGround(HELD_SITE, "", 0, 0);
		LF_Hideout.SetHeldGround(QUIET_SITE, quiet.GetId(), 99, 9);

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (load %1)", pass);
			JsonLoadContext versionContext = new JsonLoadContext(false);
			ctx.True(versionContext.LoadFromString(saved), "the save parses" + tag);
			int version;
			versionContext.ReadValue("version", version);
			ctx.Equal(LF_TestSaveVersion.CURRENT, version, "the save is written at the current version (heldground is the last payload)" + tag);

			JsonLoadContext loadContext = new JsonLoadContext(false);
			loadContext.LoadFromString(saved);
			LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);

			LF_HeldGroundRow held = LF_Hideout.HeldGroundOf(HELD_SITE);
			ctx.NotNull(held, "the held site is back" + tag);
			if (held)
			{
				ctx.EqualStr(holder.GetId(), held.m_sHolderOrgId, "the holder is kept" + tag);
				ctx.Equal(3600, held.m_iHoldSeconds, "the hold time is kept" + tag);
				ctx.Equal(12, held.m_iHoldPoints, "the banked points are kept" + tag);
			}

			LF_HeldGroundRow quietSite = LF_Hideout.HeldGroundOf(QUIET_SITE);
			ctx.NotNull(quietSite, "the quiet site is back" + tag);
			if (quietSite)
			{
				ctx.EqualStr("", quietSite.m_sHolderOrgId, "the unheld site loads unheld" + tag);
				ctx.Equal(0, quietSite.m_iHoldPoints, "the unheld site loads with no points" + tag);
			}

			ctx.EqualStr(holder.GetId(), LF_Hideout.GetOwner(HELD_SITE), "the held site keeps its lease" + tag);
			ctx.False(LF_Hideout.IsLocked(HELD_SITE), "the held site keeps its open lock" + tag);
			ctx.EqualStr(quiet.GetId(), LF_Hideout.GetOwner(QUIET_SITE), "the quiet site keeps its lease" + tag);
			ctx.True(LF_Hideout.IsLocked(QUIET_SITE), "the quiet site stays locked" + tag);
		}
	}
}
