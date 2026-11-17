// red-proof: Observed 2026-11-17 with `tools\cli test --no-build --only LF_Test_AdminWaddGrant` on the handler that only called the legacy component: FAIL chat/admin-wadd-grant, `- the uid is on the police roster`, `- MEDIC has no roster to write`, `a refused grant is not reported (expected 1, got 4)`. Green after the grant went through LF_WhitelistManager.

// tier: LOGIC
//! chat/admin-wadd-grant (#2548, #2412 item 1): `!wadd POLICE <uid>` stores the uid on the roster police duty
//! reads (LF_WhitelistManager) and only then logs and reports the grant to staff. An unknown or rosterless
//! type, and a uid already on the roster, store and report nothing.
class LF_Test_AdminWaddGrant : LF_Test
{
	override string GetName()
	{
		return "chat/admin-wadd-grant";
	}

	override void Run(LF_TestContext ctx)
	{
		string uid = "wadd-grant-target";
		string path = "$profile:TestResults/wadd-grant.txt";
		LF_WhitelistManager.SetSavePath(path);
		LF_WhitelistManager.Reset();
		if (FileIO.FileExists(path))
			FileIO.DeleteFile(path);

		array<ref LF_IncidentRecord> before = LF_IncidentLog.ExportAll();
		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.Install();

		// The command as an admin types it.
		LF_WhitelistType type;
		string parsedUid;
		ctx.True(AdminChatChannel.TryParseWhitelistCommand("!wadd POLICE " + uid, type, parsedUid), "!wadd POLICE <uid> parses");
		ctx.True(AdminChatChannel.ApplyWhitelistAdd(type, parsedUid, "admin-1"), "the grant is reported as stored");
		ctx.True(LF_WhitelistManager.GetWhitelistedFactions(uid).Contains(LF_EFactionType.POLICE), "the uid is on the police roster");
		ctx.Equal(1, LF_IncidentLog.Count(), "one staff report for the stored grant");

		// A restart reads the same roster.
		LF_WhitelistManager.Reset();
		LF_WhitelistManager.Load();
		ctx.True(LF_WhitelistManager.GetWhitelistedFactions(uid).Contains(LF_EFactionType.POLICE), "the grant is in the file");

		// Already on the roster: nothing new was stored, so nothing is reported.
		ctx.False(AdminChatChannel.ApplyWhitelistAdd(LF_WhitelistType.POLICE, uid, "admin-1"), "a second grant stores nothing");
		ctx.Equal(1, LF_IncidentLog.Count(), "a repeat grant is not reported again");

		// A type with no roster grants and reports nothing.
		string medicUid = "wadd-grant-medic";
		ctx.False(AdminChatChannel.ApplyWhitelistAdd(LF_WhitelistType.MEDIC, medicUid, "admin-1"), "MEDIC has no roster to write");
		ctx.False(AdminChatChannel.ApplyWhitelistAdd(LF_WhitelistType.NONE, medicUid, "admin-1"), "NONE has no roster to write");
		ctx.Equal(0, LF_WhitelistManager.GetWhitelistedFactions(medicUid).Count(), "a rosterless type leaves the uid off every roster");
		ctx.Equal(1, LF_IncidentLog.Count(), "a refused grant is not reported");

		// An unknown type name does not even parse.
		ctx.False(AdminChatChannel.TryParseWhitelistCommand("!wadd WIZARD " + medicUid, type, parsedUid), "an unknown type name does not parse");

		LF_IncidentLog.ResetStatics();
		LF_IncidentLog.ApplyAll(before);
		if (FileIO.FileExists(path))
			FileIO.DeleteFile(path);
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
	}
}
