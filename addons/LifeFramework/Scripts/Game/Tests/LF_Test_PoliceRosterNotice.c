// red-proof: observed 2026-11-15 with `tools\cli test --no-build --red LF_Test_PoliceRosterNotice`: RED-OK, skipping the spawn call failed it with `exactly one notice went out (expected 1, got 0)`; green with `--only` (passed=1 failed=0).
//------------------------------------------------------------------------------------------------
//! police/roster-notice (#1447): an approved recruit and an expired trial are told once at the first
//! spawn of a session; a second spawn, a civilian, a rejected or open application and an empty uid
//! hear nothing; the spawn event on the bus reaches the notice.
// tier: LOGIC
class LF_Test_PoliceRosterNotice : LF_Test
{
	override string GetName()
	{
		return "police/roster-notice";
	}

	override void Run(LF_TestContext ctx)
	{
		string appsPath = "$profile:LifeFramework/test_police_notice_apps.txt";
		string whitelistPath = "$profile:LifeFramework/test_police_notice_whitelist.txt";
		string toldPath = "$profile:LifeFramework/test_police_notice_told.txt";
		FileIO.DeleteFile(appsPath);
		FileIO.DeleteFile(toldPath);
		LF_PoliceApplications.SetSavePath(appsPath);
		LF_WhitelistManager.SetSavePath(whitelistPath);
		LF_WhitelistManager.Reset();
		LF_EventBus.ResetStatics();
		LF_PoliceRosterNotice.ResetStatics();
		LF_PoliceRosterNotice.SetToldPath(toldPath);
		LF_PoliceRosterNotice.SetQuiet(true);

		string ann = "test-notice-ann";
		string bo = "test-notice-bo";
		string cy = "test-notice-cy";
		string dee = "test-notice-dee";
		string eve = "test-notice-eve";
		string civilian = "test-notice-civilian";

		// A civilian with no application, and an open one, hear nothing.
		ctx.EqualStr("", LF_PoliceRosterNotice.OnSpawned(2, civilian, true), "a civilian with no application is told nothing");
		ctx.EqualStr("", LF_Test_PoliceRosterNotice.Submit(eve), "eve files an application");
		ctx.EqualStr("", LF_PoliceRosterNotice.OnSpawned(3, eve, true), "an open application is told nothing");
		ctx.True(LF_PoliceApplications.Reject(eve, true, 1000), "eve is rejected");
		ctx.EqualStr("", LF_PoliceRosterNotice.OnSpawned(3, eve, true), "a rejected application is told nothing");
		ctx.EqualStr("", LF_PoliceRosterNotice.OnSpawned(3, "", true), "an empty uid is told nothing");
		ctx.Equal(0, LF_PoliceRosterNotice.SentCount(), "so far no notice went out");

		// Approved while away: one notice with the approved key, at the first spawn only.
		LF_Test_PoliceRosterNotice.Submit(ann);
		ctx.True(LF_PoliceApplications.ApproveAt(ann, 1000, 0), "ann is approved");
		string told = "";
		if (!RedProof())
			told = LF_PoliceRosterNotice.OnSpawned(4, ann, true);

		ctx.EqualStr(LF_PoliceRosterNotice.KEY_APPROVED, told, "an approved recruit gets the approved key at the first spawn");
		ctx.Equal(1, LF_PoliceRosterNotice.SentCount(), "exactly one notice went out");
		ctx.EqualStr("4|" + LF_PoliceRosterNotice.KEY_APPROVED, LF_PoliceRosterNotice.SentAt(0), "it went to ann's player id with that key");
		ctx.EqualStr("", LF_PoliceRosterNotice.OnSpawned(4, ann, false), "a second spawn the same session gives none");
		ctx.EqualStr("", LF_PoliceRosterNotice.OnSpawned(4, ann, true), "the row was told already: not again, even as a first spawn");
		LF_PoliceRosterNotice.ResetStatics();
		LF_PoliceRosterNotice.SetToldPath(toldPath);
		LF_PoliceRosterNotice.SetQuiet(true);
		ctx.EqualStr("", LF_PoliceRosterNotice.OnSpawned(4, ann, true), "a restart keeps it: the told row is on file");
		ctx.Equal(0, LF_PoliceRosterNotice.SentCount(), "and nothing went out after the restart");

		// Not a first spawn: no notice yet, and the next first spawn still tells.
		LF_Test_PoliceRosterNotice.Submit(cy);
		ctx.True(LF_PoliceApplications.ApproveAt(cy, 1000, 0), "cy is approved");
		ctx.EqualStr("", LF_PoliceRosterNotice.OnSpawned(5, cy, false), "a respawn is told nothing");
		ctx.EqualStr(LF_PoliceRosterNotice.KEY_APPROVED, LF_PoliceRosterNotice.OnSpawned(5, cy, true), "the first spawn of the session tells");

		// A trial that ran out unpromoted: the expired key, once.
		LF_Test_PoliceRosterNotice.Submit(bo);
		ctx.True(LF_PoliceApplications.ApproveAt(bo, 1000, 1), "bo is approved on a one hour trial");
		ctx.Equal(1, LF_PoliceApplications.ExpireTrials(1000 + 3600 + 1), "bo's trial runs out");
		ctx.EqualStr(LF_PoliceRosterNotice.KEY_EXPIRED, LF_PoliceRosterNotice.OnSpawned(6, bo, true), "an expired trial gets the expired key");
		ctx.EqualStr("", LF_PoliceRosterNotice.OnSpawned(6, bo, true), "the expired notice is not repeated");

		// The bus: a spawn event reaches the notice.
		ctx.True(LF_PoliceRosterNotice.EnsureSubscribed(), "the notice subscribes to the spawn event");
		LF_Test_PoliceRosterNotice.Submit(dee);
		ctx.True(LF_PoliceApplications.ApproveAt(dee, 1000, 0), "dee is approved");
		int before = LF_PoliceRosterNotice.SentCount();
		LF_EventBus.Publish(LF_OnPlayerSpawnedEvent.Create(7, dee, true));
		ctx.Equal(before + 1, LF_PoliceRosterNotice.SentCount(), "a spawn event on the bus tells dee");
		ctx.EqualStr("7|" + LF_PoliceRosterNotice.KEY_APPROVED, LF_PoliceRosterNotice.SentAt(before), "with the approved key for dee's player id");
		LF_EventBus.Publish(LF_OnPlayerSpawnedEvent.Create(8, civilian, true));
		ctx.Equal(before + 1, LF_PoliceRosterNotice.SentCount(), "a civilian's spawn event tells nobody");

		LF_PoliceRosterNotice.ResetStatics();
		LF_EventBus.ResetStatics();
		LF_PoliceApplications.SetSavePath("");
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
		FileIO.DeleteFile(appsPath);
		FileIO.DeleteFile(whitelistPath);
		FileIO.DeleteFile(toldPath);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Submit(string uid)
	{
		return LF_PoliceApplications.Submit(uid, "Test Recruit", "To serve", "Evenings", false);
	}
}
