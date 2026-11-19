// red-proof: Observed 2026-11-19 with `tools/cli test --no-build --red LF_Test_PrisonBreak`: RED PROOF OK, with the break never started and the section load skipped, e.g. "the saved remaining time is back (expected 80, got 0)" and "loading twice does not duplicate the row (expected 1, got 0)". The unhooked run passes. Review 2026-11-19: "the escapee's clock holds while out (expected 80, got 50)" failed on the unfixed LF_Jail.TickOnline, passes with the escaped skip.

//! The break (#3421): a member who passes the rules (#3420) forces the jail door, the window opens and
//! dispatch radios the officers; a prisoner who walks out in it is `escaped` with the remaining time
//! kept, a prisoner who stays serves on, the door locks at the end of the window. The escaped list is
//! the "prisonbreak" save section: it round-trips, loads twice to the same state, and a save from
//! before it leaves nobody escaped. Restart: the e2e prisoner is still escaped after `wt e2e`.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_PrisonBreak : LF_Test
{
	static const string E2E_KEY = "e2e-prison-break-escapee";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/prison-break";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

#ifdef LF_E2E_LOAD_BOOT
		if (LF_E2E.RequiresSection(LF_PrisonBreakSavePayload.NAME))
		{
			ctx.True(LF_Jail.IsServing(E2E_KEY), "the e2e escapee still has a sentence after the restart");
			ctx.True(LF_PrisonBreak.IsEscaped(E2E_KEY), "the e2e escapee is still escaped after the restart");
		}

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_PrisonBreakSavePayload.NAME), "prisonbreak is a registered save section");

		string officerUid = "prison-break-officer";
		string starterUid = "prison-break-starter";
		string runnerUid = "prison-break-runner";
		string stayerUid = "prison-break-stayer";
		string outsiderUid = "prison-break-outsider";
		string doorId = "prison-break-door";
		vector placed = "48 0 64";
		vector outside = "48 0 90";

		LF_PrisonBreakRules.ResetStatics();
		LF_PrisonBreak.ResetStatics();

		LF_PrisonBreakConfig config = LF_PrisonBreakConfig.CreateDefault();
		config.m_bEnabled = true;
		config.m_iMinOfficersOnDuty = 0;
		LF_PrisonBreakConfig.UseForTest(config);

		LF_Jail.Configure(40, 200, 1, 4);
		LF_Jail.SetCellOrigin(placed);
		LF_Jail.SetDoorTarget(doorId);
		LF_Breach.Configure(doorId, 0, "");

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		LF_PlayerAccountManager.GetOrCreate(starterUid).SetWantedLevel(0);
		LF_PlayerAccountManager.GetOrCreate(outsiderUid).SetWantedLevel(0);
		LF_PlayerAccountManager.GetOrCreate(runnerUid).SetWantedLevel(2);
		LF_PlayerAccountManager.GetOrCreate(stayerUid).SetWantedLevel(2);

		LF_Organization org = LF_OrganizationManager.Create(starterUid, "Breakout Crew");
		ctx.NotNull(org, "the starter founds an organisation");
		if (!org)
			return;

		ctx.True(LF_OrganizationManager.JoinDirect(org.GetId(), runnerUid, LF_OrganizationConfig.RANK_MEMBER), "the runner is a member");
		ctx.True(LF_OrganizationManager.JoinDirect(org.GetId(), stayerUid, LF_OrganizationConfig.RANK_MEMBER), "the stayer is a member");
		ctx.True(LF_Jail.Arrest(officerUid, runnerUid, null), "the runner is jailed");
		ctx.True(LF_Jail.Arrest(officerUid, stayerUid, null), "the stayer is jailed");
		ctx.EqualFloat(80, LF_Jail.GetRemaining(runnerUid), 0.01, "the runner serves two wanted steps");
		ctx.True(LF_Jail.Confines(runnerUid), "the locked door confines the runner");
		ctx.False(LF_PrisonBreak.IsWindowOpen(System.GetUnixTime()), "no window runs before a break");

		// A breach by someone outside every organisation is the plain break-in: no window, no escape status.
		ctx.True(LF_Breach.Arm(doorId, outsiderUid), "a non-member forces the door");
		ctx.False(LF_PrisonBreak.IsWindowOpen(System.GetUnixTime()), "a non-member's breach opens no window");
		LF_Jail.NoteEscape(runnerUid, outside, null);
		ctx.False(LF_PrisonBreak.IsEscaped(runnerUid), "walking out of a plain break-in is not the escaped status");
		LF_Jail.NoteEscape(runnerUid, placed, null);
		LF_Breach.Reset(doorId);
		ctx.True(LF_Jail.Confines(runnerUid), "the door is locked again");

		// The member breaches: the window opens and the officers are radioed.
		int reportsBefore = LF_WantedReports.CountFor(starterUid);
		// RedProof(): the break is never started, so no window opens and nobody can escape (`--red`).
		if (!RedProof())
			ctx.True(LF_Breach.Arm(doorId, starterUid), "a free member forces the door");

		int now = System.GetUnixTime();
		ctx.True(LF_PrisonBreak.IsWindowOpen(now), "the break window is open");
		ctx.True(LF_PrisonBreakRules.WindowSecondsLeft(now) > 0, "the window has time left");
		ctx.False(LF_Jail.Confines(stayerUid), "the open door confines nobody");
		ctx.True(LF_WantedReports.CountFor(starterUid) >= reportsBefore + 2, "the break is reported to dispatch beside the forced door");

		ctx.False(LF_PrisonBreak.OnWalkedOut(stayerUid, 80, now + 100000), "a walk-out after the window is not an escape");
		ctx.False(LF_Jail.NoteEscape(stayerUid, placed, null), "the stayer is inside the cell");
		ctx.True(LF_Jail.NoteEscape(runnerUid, outside, null), "the runner walks out through the open door");
		ctx.True(LF_PrisonBreak.IsEscaped(runnerUid), "the runner is escaped");
		ctx.EqualFloat(80, LF_PrisonBreak.EscapedRemaining(runnerUid), 0.01, "the escape keeps the remaining time");
		ctx.False(LF_PrisonBreak.IsEscaped(stayerUid), "the stayer is not escaped");

		// The window ends: the door locks; the runner stays out, the stayer is held.
		ctx.True(LF_PrisonBreak.Tick(now), "the window is still open inside its length");
		ctx.False(LF_PrisonBreak.Tick(now + 100000), "the window is over");
		ctx.True(LF_Breach.IsLocked(doorId), "the door closes at the end of the window");
		ctx.False(LF_PrisonBreakRules.IsWindowOpen(now + 100000), "the rules see the window closed");
		ctx.True(LF_Jail.Confines(stayerUid), "the stayer is confined again");
		ctx.True(SamePlace(LF_Jail.ResolvePosition(stayerUid, outside), placed), "the stayer is pulled back if they step out");
		ctx.False(LF_Jail.Confines(runnerUid), "the escaped runner is not confined");
		ctx.True(SamePlace(LF_Jail.ResolvePosition(runnerUid, outside), outside), "the escaped runner is not pulled back");
		ctx.EqualFloat(80, LF_Jail.GetRemaining(runnerUid), 0.01, "the runner's remaining time is kept");
		ctx.EqualFloat(80, LF_Jail.GetRemaining(stayerUid), 0.01, "the stayer serves on");

		// Out on the run the clock holds: an escapee online does not serve the rest free and get
		// released (teleported to the jail gate) wherever they are.
		array<string> runnerOnline = {runnerUid};
		LF_Jail.TickOnline(30, runnerOnline);
		ctx.True(LF_Jail.IsServing(runnerUid), "the escapee's sentence does not run out on the run");
		ctx.EqualFloat(80, LF_Jail.GetRemaining(runnerUid), 0.01, "the escapee's clock holds while out");

		// Save and load: the sentence is the jail payload, the status is the section.
		array<ref LF_JailRecord> sentences = LF_Jail.ExportAll();
		ctx.True(sentences.Count() >= 2, "both prisoners are in the jail payload (other tests may leave sentences too)");
		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(SectionKey()), "the save carries the prisonbreak section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_PrisonBreakSavePayload.NAME), "prisonbreak is not a legacy positional payload");

		// Restart: the session holds no sentences and no statuses.
		LF_Jail.ApplyAll(new array<ref LF_JailRecord>());
		LF_PrisonBreak.ResetStatics();
		ctx.False(LF_PrisonBreak.IsEscaped(runnerUid), "a restart forgets the status until the load");

		LF_Jail.ApplyAll(sentences);
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(loadContext, ctx);

		ctx.True(LF_PrisonBreak.IsEscaped(runnerUid), "the escaped status is back after the load");
		ctx.EqualFloat(80, LF_PrisonBreak.EscapedRemaining(runnerUid), 0.01, "the saved remaining time is back");
		ctx.EqualFloat(80, LF_Jail.GetRemaining(runnerUid), 0.01, "the jail payload keeps the remaining time");
		ctx.False(LF_PrisonBreak.IsEscaped(stayerUid), "the stayer is still not escaped after the load");
		ctx.False(LF_Jail.Confines(runnerUid), "the loaded escapee is not pulled back");

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		ReadSection(again, ctx);
		ctx.Equal(1, LF_PrisonBreak.ExportAll().Count(), "loading twice does not duplicate the row");

		array<ref LF_PrisonBreakRow> bad = {};
		bad.Insert(LF_PrisonBreakRow.Create("", 10));
		ctx.Equal(0, LF_PrisonBreak.ApplyAll(bad), "a row with no key is skipped");

		// A save from before the section: it is absent and the hook only logs.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_PrisonBreakSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		ctx.False(oldContext.SaveToString().Contains(SectionKey()), "the older save has no prisonbreak section");
		LF_SavePayload section = LF_SaveSections.Get(LF_PrisonBreakSavePayload.NAME);
		if (section)
			section.OnAbsent();

		// A sentence that ended drops the status: nothing left to serve, nothing to save.
		array<ref LF_PrisonBreakRow> runnerRows = {};
		runnerRows.Insert(LF_PrisonBreakRow.Create(runnerUid, 80));
		LF_PrisonBreak.ApplyAll(runnerRows);
		LF_Jail.Advance(runnerUid, 1000);
		ctx.False(LF_PrisonBreak.IsEscaped(runnerUid), "a finished sentence is not escaped");
		ctx.Equal(0, LF_PrisonBreak.ExportAll().Count(), "a finished sentence is not saved");

		// Clean up what this test made: the stayer's sentence ends and the crew disbands (other tests' orgs stay).
		LF_Jail.Advance(stayerUid, 1000);
		LF_OrganizationManager.LeaveDirect(runnerUid);
		LF_OrganizationManager.LeaveDirect(stayerUid);
		LF_OrganizationManager.LeaveDirect(starterUid);

		// Leave the e2e prisoner escaped in the store for the restart proof; end with a clean world.
		LF_PrisonBreak.ResetStatics();
		LF_PrisonBreakRules.ResetStatics();
		LF_Jail.SyncPlacedCell();
		LF_Jail.Imprison(E2E_KEY, 3600, 1, null);
		array<ref LF_PrisonBreakRow> e2eRows = {};
		e2eRows.Insert(LF_PrisonBreakRow.Create(E2E_KEY, 3600));
		LF_PrisonBreak.ApplyAll(e2eRows);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool SamePlace(vector a, vector b)
	{
		return vector.Distance(a, b) < 0.05;
	}

	//------------------------------------------------------------------------------------------------
	protected static string SectionKey()
	{
		return "\"" + LF_SaveSections.OBJECT_PREFIX + LF_PrisonBreakSavePayload.NAME + "\"";
	}

	//------------------------------------------------------------------------------------------------
	protected static void ReadSection(notnull JsonLoadContext context, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_PrisonBreakSavePayload.NAME);
		if (!section)
			return;

		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_PrisonBreakSavePayload.NAME), "the prisonbreak section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the prisonbreak section reads");
		context.EndObject();
	}
}
