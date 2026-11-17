// red-proof: observed 2026-11-17 `toolsi test --no-build --red LF_Test_HideoutLeaseIdle` (the first SweepIdle skipped by RedProof()): RED PROOF OK, `- the lease idle for 15 days is released (expected '', got ...)`, `- a member online restarts the clock`. Hook off: green.
// tier: PERSISTENCE
//! #2573 (epic #2533): a hideout lease keeps the last real time a member of its organization was
//! connected, and a sweep releases a lease idle longer than LF_OrgConfig.HideoutLeaseIdleDays().
//! A lease past the limit is released, one with a member online is kept (and its clock restarts),
//! 0 days turns the lapse off, and the timestamp goes through the real save path ("leaseidle"
//! payload): a save without it seeds "now", and a saved stamp already past the limit (the server
//! was down) gets one grace day instead of lapsing before any member could join.
class LF_Test_HideoutLeaseIdle : LF_Test
{
	protected static const string SITE_IDLE = "hideout-idle-site-idle";
	protected static const string SITE_ONLINE = "hideout-idle-site-online";
	protected static const int DAY_S = 86400;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/hideout-lease-idle";
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
		LF_OrgConfig config = LF_OrgConfig.Get();
		ctx.NotNull(config, "the organization config loads");
		if (!persistence || !config)
			return;

		// The e2e save boot seeds an organization and a lease earlier in this tier; put them back at the end.
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		int keptDays = config.m_iHideoutLeaseIdleDays;
		RunIdle(ctx, persistence, config);
		config.m_iHideoutLeaseIdleDays = keptDays;
		if (!LF_Hideout.GetOwner(SITE_IDLE).IsEmpty())
			LF_Hideout.Release(SITE_IDLE);

		if (!LF_Hideout.GetOwner(SITE_ONLINE).IsEmpty())
			LF_Hideout.Release(SITE_ONLINE);

		kept.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunIdle(LF_TestContext ctx, LF_PersistenceComponent persistence, LF_OrgConfig config)
	{
		ctx.Equal(14, LF_OrgConfig.DEFAULT_HIDEOUT_LEASE_IDLE_DAYS, "the shipped limit is 14 days");
		config.m_iHideoutLeaseIdleDays = 14;
		ctx.Equal(14, LF_OrgConfig.HideoutLeaseIdleDays(), "the limit reads 14 days");

		string founderIdle = "hideout-idle-founder-idle";
		string founderOnline = "hideout-idle-founder-online";
		LF_Organization idleOrg = LF_OrganizationManager.Create(founderIdle, "Idle Hideout Crew");
		LF_Organization onlineOrg = LF_OrganizationManager.Create(founderOnline, "Online Hideout Crew");
		ctx.NotNull(idleOrg, "the idle organization is founded");
		ctx.NotNull(onlineOrg, "the online organization is founded");
		if (!idleOrg || !onlineOrg)
			return;

		int now = System.GetUnixTime();
		ctx.True(LF_Hideout.Assign(SITE_IDLE, idleOrg.GetId()), "the idle organization leases its site");
		ctx.True(LF_Hideout.Assign(SITE_ONLINE, onlineOrg.GetId()), "the online organization leases its site");
		ctx.True(LF_Hideout.LastActiveOf(SITE_IDLE) >= now, "a new lease starts its clock now");

		// One site past the limit with nobody connected, one past the limit with a member connected.
		LF_Hideout.SetLastActive(SITE_IDLE, now - 15 * DAY_S);
		LF_Hideout.SetLastActive(SITE_ONLINE, now - 15 * DAY_S);
		array<string> online = {founderOnline};
		array<string> nobody = {};
		int released;
		if (!RedProof())
			released = LF_Hideout.SweepIdle(now, online);

		ctx.Equal(1, released, "one sweep releases exactly the idle lease");
		ctx.EqualStr("", LF_Hideout.GetOwner(SITE_IDLE), "the lease idle for 15 days is released");
		ctx.EqualStr(onlineOrg.GetId(), LF_Hideout.GetOwner(SITE_ONLINE), "a lease with a member online is kept");
		ctx.Equal(now, LF_Hideout.LastActiveOf(SITE_ONLINE), "a member online restarts the clock");
		ctx.True(LF_Hideout.IsLocked(SITE_IDLE), "the released site is locked");

		// Under the limit is kept.
		ctx.True(LF_Hideout.Assign(SITE_IDLE, idleOrg.GetId()), "the idle organization leases again");
		LF_Hideout.SetLastActive(SITE_IDLE, now - 13 * DAY_S);
		ctx.Equal(0, LF_Hideout.SweepIdle(now, nobody), "a sweep over leases idle 13 days releases none");
		ctx.EqualStr(idleOrg.GetId(), LF_Hideout.GetOwner(SITE_IDLE), "13 idle days is under the limit");

		// 0 turns the lapse off.
		config.m_iHideoutLeaseIdleDays = 0;
		LF_Hideout.SetLastActive(SITE_IDLE, now - 400 * DAY_S);
		ctx.Equal(0, LF_Hideout.SweepIdle(now, nobody), "0 days never lapses a lease");
		ctx.EqualStr(idleOrg.GetId(), LF_Hideout.GetOwner(SITE_IDLE), "the lease survives with the lapse off");
		config.m_iHideoutLeaseIdleDays = 14;

		// The timestamp goes through the real save path.
		int stamp = now - 5 * DAY_S;
		LF_Hideout.SetLastActive(SITE_IDLE, stamp);
		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("m_iLastActive"), "the save carries the idle rows");

		LF_Hideout.SetLastActive(SITE_IDLE, now - 1 * DAY_S);
		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (load %1)", pass);
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses" + tag);
			LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);
			ctx.EqualStr(idleOrg.GetId(), LF_Hideout.GetOwner(SITE_IDLE), "the lease is back" + tag);
			ctx.Equal(stamp, LF_Hideout.LastActiveOf(SITE_IDLE), "the last-active time is kept" + tag);
		}

		// A save from before the payload seeds "now": no lease lapses on the first boot after the update.
		LF_Hideout.SetLastActive(SITE_IDLE, now - 100 * DAY_S);
		JsonSaveContext oldContext = new JsonSaveContext(false);
		int before = LF_PersistenceComponentSerializer.PayloadSinceVersion("leaseidle") - 1;
		ctx.True(before > 0, "the payload is registered");
		ctx.True(LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, oldContext, before) == ESerializeResult.OK, "the store serializes as the version before the payload");
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		oldLoad.LoadFromString(oldContext.SaveToString());
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.EqualStr(idleOrg.GetId(), LF_Hideout.GetOwner(SITE_IDLE), "the lease is back from the old save");
		ctx.True(LF_Hideout.LastActiveOf(SITE_IDLE) >= now, "an old save seeds the clock now");
		ctx.Equal(0, LF_Hideout.SweepIdle(System.GetUnixTime(), nobody), "no lease lapses on the first boot after the update");

		// A saved stamp past the limit means the server was down: one grace day, not an instant lapse.
		array<ref LF_HideoutIdleRow> late = {};
		late.Insert(LF_HideoutIdleRow.Create(SITE_IDLE, now - 20 * DAY_S));
		LF_Hideout.ApplyIdle(late);
		ctx.EqualStr(idleOrg.GetId(), LF_Hideout.GetOwner(SITE_IDLE), "a late row keeps the lease");
		ctx.Equal(0, LF_Hideout.SweepIdle(now, nobody), "a stamp past the limit at load gets a grace day");
		ctx.Equal(1, LF_Hideout.SweepIdle(now + 2 * DAY_S, nobody), "the grace day runs out");
		ctx.EqualStr("", LF_Hideout.GetOwner(SITE_IDLE), "the lease lapses after the grace");
	}
}
