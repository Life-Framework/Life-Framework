// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_OrgWarPersist`: `RED-OK org/war-persist` with the store read skipped, `- three wars after the load (load 1) (expected 3, got 0)`. Green with the read.
// tier: PERSISTENCE
//! org/war-persist (#2672, M7 gang war W6): the "orgwars" payload round trips an accepted war, a
//! pending declaration and an ended war still inside its pair cooldown through the real store write
//! and read, with the same ids, states and times; a lapsed declaration is not saved; a second load
//! gives the same set; a row naming a deleted organization is dropped; a save from the version before
//! this payload loads no wars. Across a restart (`tools\cli wt e2e`): the save boot stores an accepted
//! war; the reload boot finds it with the same end time. The test puts back the tables it found.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_OrgWarPersist : LF_Test
{
	protected static const string E2E_FOUNDER_A = "org-war-e2e-founder-a";
	protected static const string E2E_FOUNDER_B = "org-war-e2e-founder-b";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "org/war-persist";
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

		int since = LF_PersistenceComponentSerializer.PayloadSinceVersion("orgwars");
#ifdef LF_E2E_LOAD_BOOT
		LF_Organization e2eA = LF_OrganizationManager.GetByCharacter(E2E_FOUNDER_A);
		LF_Organization e2eB = LF_OrganizationManager.GetByCharacter(E2E_FOUNDER_B);
		if (LF_SaveStamp.BootSaveVersion() < since)
		{
			bool none = !e2eA || !e2eB || !LF_OrgWars.IsAtWar(e2eA.GetId(), e2eB.GetId());
			ctx.True(none, "a save older than the orgwars payload loads no war");
			return;
		}

		ctx.NotNull(e2eA, "the e2e org A is back after the restart");
		ctx.NotNull(e2eB, "the e2e org B is back after the restart");
		if (e2eA && e2eB)
		{
			ctx.True(LF_OrgWars.IsAtWar(e2eA.GetId(), e2eB.GetId()), "the war is still on after the restart");
			array<ref LF_OrgWarRecord> active = LF_OrgWars.ActiveFor(e2eA.GetId());
			ctx.Equal(1, active.Count(), "one active war for the e2e org after the restart");
			if (active.Count() == 1)
				ctx.True(active[0].m_iEndsAt > System.GetUnixTime(), "the war still ends in the future after the restart");
		}

		// The war is this test's own; it is not saved again.
		LF_OrgWars.ResetStatics();
		return;
#endif

		ctx.True(since > 0, "orgwars is a registered payload");
		if (since <= 1)
			return;

		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		LF_OrgWars.ResetStatics();

		LF_Organization a = LF_OrganizationManager.Create("org-war-persist-a", "War Persist Alpha");
		LF_Organization b = LF_OrganizationManager.Create("org-war-persist-b", "War Persist Bravo");
		LF_Organization c = LF_OrganizationManager.Create("org-war-persist-c", "War Persist Charlie");
		LF_Organization d = LF_OrganizationManager.Create("org-war-persist-d", "War Persist Delta");
		bool allOrgs = a != null && b != null && c != null && d != null;
		ctx.True(allOrgs, "four test organizations are registered");
		if (allOrgs)
			RoundTrip(ctx, persistence, since, a.GetId(), b.GetId(), c.GetId(), d.GetId());

		// Puts back the organizations, then the wars (ApplyAll drops a war whose organization is not there).
		kept.Restore();
#ifdef LF_E2E_HOST_STOP
		// The save boot: the shutdown save carries one accepted war between two orgs, a week long.
		LF_Organization hostA = LF_OrganizationManager.GetByCharacter(E2E_FOUNDER_A);
		if (!hostA)
			hostA = LF_OrganizationManager.Create(E2E_FOUNDER_A, "War E2E Alpha");
		LF_Organization hostB = LF_OrganizationManager.GetByCharacter(E2E_FOUNDER_B);
		if (!hostB)
			hostB = LF_OrganizationManager.Create(E2E_FOUNDER_B, "War E2E Bravo");
		bool bothOrgs = hostA != null && hostB != null;
		ctx.True(bothOrgs, "the save boot registered the e2e orgs");
		if (bothOrgs)
		{
			LF_OrgWarRules e2eRules = Rules(600, 604800);
			int nowE2e = System.GetUnixTime();
			LF_OrgWarRecord e2eWar;
			LF_OrgWars.Declare(hostA.GetId(), hostB.GetId(), nowE2e, e2eRules, e2eWar);
			if (e2eWar)
				LF_OrgWars.Accept(e2eWar.m_sWarId, nowE2e, e2eRules);
			ctx.True(LF_OrgWars.IsAtWar(hostA.GetId(), hostB.GetId()), "the save boot stored the e2e war");
		}
#endif
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_OrgWarRules Rules(int answer, int length)
	{
		LF_OrgWarRules rules = new LF_OrgWarRules();
		rules.m_iFee = 0;
		rules.m_iAnswerSeconds = answer;
		rules.m_iLengthSeconds = length;
		rules.m_iCooldownSeconds = 7200;
		// No lapse cooldown here: a lapsed declaration with none running is not saved (#2769 saves one that has).
		rules.m_iLapseCooldownSeconds = 0;
		rules.m_iMinMembers = 1;
		return rules;
	}

	//------------------------------------------------------------------------------------------------
	protected void RoundTrip(LF_TestContext ctx, LF_PersistenceComponent persistence, int since, string a, string b, string c, string d)
	{
		int t = System.GetUnixTime();
		LF_OrgWarRecord war;

		// war-1: C on D, never answered: lapses, not saved.
		LF_OrgWars.Declare(c, d, t - 1000, Rules(600, 60), war);
		// war-2: B on C, accepted, a 60 s war that ended at t-30: inside its pair cooldown, saved.
		LF_OrgWars.Declare(b, c, t - 100, Rules(600, 60), war);
		if (war)
			LF_OrgWars.Accept(war.m_sWarId, t - 90, Rules(600, 60));
		// war-3: A on B, accepted and running until t+3600.
		LF_OrgWars.Declare(a, b, t, Rules(600, 3600), war);
		if (war)
			LF_OrgWars.Accept(war.m_sWarId, t, Rules(600, 3600));
		// war-4: A on C, pending until t+600.
		LF_OrgWars.Declare(a, c, t, Rules(600, 3600), war);

		ctx.Equal(4, LF_OrgWars.Count(), "four wars before the save");
		LF_OrgWarRecord lapsed = LF_OrgWars.Get("war-1");
		LF_OrgWarRecord ended = LF_OrgWars.Get("war-2");
		ctx.NotNull(lapsed, "war-1 exists before the save");
		if (lapsed)
			ctx.Equal(LF_EOrgWarState.LAPSED, lapsed.m_eState, "war-1 lapsed before the save");
		ctx.NotNull(ended, "war-2 exists before the save");
		if (ended)
			ctx.Equal(LF_EOrgWarState.ENDED, ended.m_eState, "war-2 ended before the save");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext);
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("war-3"), "the save carries the accepted war");
		ctx.False(saved.Contains("war-1"), "the save leaves out the lapsed declaration");

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (load %1)", pass);
			// The live table empties before each load, so only the save can bring the wars back.
			LF_OrgWars.ResetStatics();
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses" + tag);
			if (!RedProof())
				LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);

			ctx.Equal(3, LF_OrgWars.Count(), "three wars after the load" + tag);
			ctx.True(LF_OrgWars.Get("war-1") == null, "the lapsed declaration stays gone" + tag);

			LF_OrgWarRecord endedBack = LF_OrgWars.Get("war-2");
			ctx.NotNull(endedBack, "the ended war is back for its cooldown" + tag);
			if (endedBack)
			{
				ctx.Equal(LF_EOrgWarState.ENDED, endedBack.m_eState, "the ended war stays ended" + tag);
				ctx.Equal(t - 30 + 7200, endedBack.m_iCooldownUntil, "the ended war keeps its cooldown end" + tag);
			}

			LF_OrgWarRecord acceptedBack = LF_OrgWars.Get("war-3");
			ctx.NotNull(acceptedBack, "the accepted war is back" + tag);
			if (acceptedBack)
			{
				ctx.Equal(LF_EOrgWarState.ACCEPTED, acceptedBack.m_eState, "the accepted war stays accepted" + tag);
				ctx.EqualStr(a, acceptedBack.m_sDeclarerOrgId, "the accepted war keeps its declarer" + tag);
				ctx.EqualStr(b, acceptedBack.m_sTargetOrgId, "the accepted war keeps its target" + tag);
				ctx.Equal(t, acceptedBack.m_iDeclaredAt, "the accepted war keeps its declaration time" + tag);
				ctx.Equal(t, acceptedBack.m_iAcceptedAt, "the accepted war keeps its accept time" + tag);
				ctx.Equal(t + 3600, acceptedBack.m_iEndsAt, "the accepted war keeps its end time" + tag);
			}

			LF_OrgWarRecord pendingBack = LF_OrgWars.Get("war-4");
			ctx.NotNull(pendingBack, "the pending declaration is back" + tag);
			if (pendingBack)
			{
				ctx.Equal(LF_EOrgWarState.DECLARED, pendingBack.m_eState, "the declaration stays pending" + tag);
				ctx.Equal(600, pendingBack.m_iAnswerWindowSec, "the declaration keeps its answer window" + tag);
			}

			ctx.True(LF_OrgWars.IsAtWar(a, b), "A and B are still at war" + tag);
		}

		// The cooldown survived: B may not declare on C again yet. The id counter is past war-4.
		LF_OrgWarRecord refused;
		ctx.EqualStr(LF_OrgWars.REFUSE_COOLDOWN, LF_OrgWars.Declare(c, b, t, Rules(600, 60), refused), "the loaded cooldown refuses a new war on the pair");
		LF_OrgWarRecord fresh;
		LF_OrgWars.Declare(d, b, t, Rules(600, 60), fresh);
		ctx.NotNull(fresh, "a new pair may still declare after the load");
		if (fresh)
			ctx.EqualStr("war-5", fresh.m_sWarId, "the next war id follows the loaded ones");

		// A row naming an organization that is gone is dropped on apply.
		array<ref LF_OrgWarRecord> rows = LF_OrgWars.ExportAll();
		LF_OrgWarRecord orphan = new LF_OrgWarRecord();
		orphan.m_sWarId = "war-9";
		orphan.m_sDeclarerOrgId = a;
		orphan.m_sTargetOrgId = "org-war-persist-gone";
		orphan.m_eState = LF_EOrgWarState.ACCEPTED;
		orphan.m_iEndsAt = t + 3600;
		rows.Insert(orphan);
		LF_OrgWars.ApplyAll(rows);
		ctx.True(LF_OrgWars.Get("war-9") == null, "a war naming a deleted organization is dropped");
		ctx.Equal(4, LF_OrgWars.Count(), "the other wars stay after the drop");

		// The newest war is always saved, closed or not, so its id is never handed out again.
		LF_OrgWarRecord closedNewest = new LF_OrgWarRecord();
		closedNewest.m_sWarId = "war-7";
		closedNewest.m_sDeclarerOrgId = a;
		closedNewest.m_sTargetOrgId = b;
		closedNewest.m_eState = LF_EOrgWarState.LAPSED;
		array<ref LF_OrgWarRecord> closedRows = {};
		closedRows.Insert(closedNewest);
		LF_OrgWars.ApplyAll(closedRows);
		ctx.Equal(1, LF_OrgWars.ExportForSave(t).Count(), "the newest war is saved even when closed");
		LF_OrgWars.ApplyAll(rows);

		// A save from the version before this payload holds no wars and leaves the live table alone.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, oldContext, since - 1);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains("war-3"), "the old save has no war rows");
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(4, LF_OrgWars.Count(), "the old save leaves the live war table untouched");
	}
}
