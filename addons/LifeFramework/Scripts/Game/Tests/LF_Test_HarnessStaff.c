// red-proof: `test --no-build --red LF_Test_HarnessStaff` (the admin approve skipped by RedProof): 'RED PROOF OK (all 1 listed test(s) fail with the hook on)', 2026-11-15
// tier: LOGIC
class LF_Test_HarnessStaff : LF_Test
{
	override string GetName()
	{
		return "harness/staff-approve";
	}

	override void Run(LF_TestContext ctx)
	{
		string appsPath = "$profile:LifeFramework/test_harness_staff_apps.txt";
		string rosterPath = "$profile:LifeFramework/test_harness_staff_roster.txt";
		string whitelistPath = "$profile:LifeFramework/test_harness_staff_whitelist.txt";
		LF_PoliceApplications.SetSavePath(appsPath);
		LF_PoliceApplicationStaff.SetStaffPath(rosterPath);
		LF_WhitelistManager.SetSavePath(whitelistPath);
		LF_WhitelistManager.Reset();
		FileIO.DeleteFile(appsPath);
		FileIO.DeleteFile(rosterPath);

		string civilian = "test-hs-civilian";
		string ann = "test-hs-ann";
		ctx.EqualStr("", LF_PoliceApplications.Submit(ann, "Ann Lee", "To help", "Evenings", false), "ann's application is filed");

		// The staff API still decides: a sender that is neither an engine administrator nor on staff.txt is refused.
		ctx.EqualStr("error #LF-PoliceApp_NotAdmin", LF_HarnessStaff.Decide(civilian, false, ann), "a non-admin staffapprove is refused");
		ctx.True(LF_PoliceApplications.HasOpen(ann), "the refused call left the application open");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(ann, LF_EFactionType.POLICE), "and off the police roster");
		ctx.EqualStr("error #LF-PoliceApp_NotAdmin", LF_HarnessStaff.Decide("", false, ann), "a sender with no account yet is refused");

		// The engine administrator approves it.
		string approved = "";
		if (!RedProof())
			approved = LF_HarnessStaff.Decide(civilian, true, ann);

		ctx.EqualStr("ok approved " + ann, approved, "an engine administrator's staffapprove is accepted");
		ctx.False(LF_PoliceApplications.HasOpen(ann), "ann is no longer open");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(ann, LF_EFactionType.POLICE), "approval puts ann on the police roster");
		ctx.EqualStr("error #LF-PoliceApp_NoneOpen", LF_HarnessStaff.Decide(civilian, true, ann), "a second approve finds nothing open");
		ctx.EqualStr("error #LF-PoliceApp_Error_NoAccount", LF_HarnessStaff.Decide(civilian, true, "a|b"), "a malformed target is refused");

		// The verb's plumbing: it is recognised, and it needs a body to find the sender.
		ctx.True(LF_HarnessStaff.IsVerb("staffapprove " + ann), "staffapprove <uid> is this harness verb");
		ctx.False(LF_HarnessStaff.IsVerb("itemgive {A}x.et"), "another verb is not");
		ctx.EqualStr("error no_character", LF_HarnessStaff.Run(null, "staffapprove " + ann), "no character, no sender");
		ctx.EqualStr("error usage_staffapprove_uid", LF_HarnessStaff.Run(null, "staffapprove a b"), "two targets are a usage error");

		// harness-admins.txt: the account list a session seeds for --admin-client.
		string admins = "# session identities\n00bbbddd-5e55-0000-0000-000000000001  # client 1\n\n";
		ctx.True(LF_HarnessStaff.ListHas(admins, "00bbbddd-5e55-0000-0000-000000000001"), "a listed identity is found, a trailing comment ignored");
		ctx.False(LF_HarnessStaff.ListHas(admins, "00bbbddd-5e55-0000-0000-000000000002"), "an identity that is not listed is not");
		ctx.False(LF_HarnessStaff.ListHas(admins, "client"), "a comment is not an entry");
		ctx.False(LF_HarnessStaff.ListHas(admins, ""), "an empty token is never listed");
		ctx.True(LF_HarnessStaff.ListHas("a\r\nb\r\n", "b"), "CRLF lines are read");
		ctx.True(LF_HarnessStaff.ShouldGrant("00bbbddd-5e55-0000-0000-000000000001", admins, false), "a listed account without the role is granted it");
		ctx.False(LF_HarnessStaff.ShouldGrant("00bbbddd-5e55-0000-0000-000000000001", admins, true), "one that already has it is not granted twice");
		ctx.False(LF_HarnessStaff.ShouldGrant("00bbbddd-5e55-0000-0000-000000000002", admins, false), "an unlisted account is not granted");
		ctx.False(LF_HarnessStaff.ShouldGrant("", admins, false), "an account that has not resolved is not granted");
		ctx.False(LF_HarnessStaff.ShouldGrant("00bbbddd-5e55-0000-0000-000000000001", "", false), "no seed file grants nobody");

		// harness-flags.txt: the gameplay defaults change only in a harness build that asked.
		string flags = "no-instant-approval\nskip-playtime-gate\n";
		ctx.False(LF_HarnessStaff.InstantApprovalFrom(true, flags, true), "--no-instant-approval turns approve-on-submit off in a harness build");
		ctx.True(LF_HarnessStaff.InstantApprovalFrom(true, "", true), "no flag leaves the playtest mission approving on submit");
		ctx.True(LF_HarnessStaff.InstantApprovalFrom(true, flags, false), "a build without LF_HARNESS ignores the flag file");
		ctx.False(LF_HarnessStaff.InstantApprovalFrom(false, "", true), "a mission that never approved on submit still does not");
		ctx.Equal(0, LF_HarnessStaff.MinPlaytimeFrom(50, flags, true), "--skip-playtime-gate checks no minimum in a harness build");
		ctx.Equal(50, LF_HarnessStaff.MinPlaytimeFrom(50, "no-instant-approval\n", true), "without the flag the configured hours stand");
		ctx.Equal(50, LF_HarnessStaff.MinPlaytimeFrom(50, flags, false), "a build without LF_HARNESS keeps the configured hours");
		ctx.False(LF_DevGate.HarnessBuild(), "the test suite is not a harness build, so none of this is reachable in a hosted game");

		// The chat verb: channel words, the default channel, and what it refuses before touching the chat.
		ctx.EqualStr("Admin", LF_HarnessChat.ChannelOf("ADMIN"), "admin names the Admin channel, any case");
		ctx.EqualStr("OOCGlobal", LF_HarnessChat.ChannelOf("ooc"), "ooc names OOCGlobal");
		ctx.EqualStr("", LF_HarnessChat.ChannelOf("handled"), "another word is not a channel");
		ctx.EqualStr("Admin", LF_HarnessChat.DefaultChannelFor("!handled 3"), "a ! line goes to the admin channel");
		ctx.EqualStr("Global", LF_HarnessChat.DefaultChannelFor("hello"), "plain text goes to global");
		string chatChannel;
		string chatText;
		array<string> words = {"!handled", "3"};
		ctx.True(LF_HarnessChat.Split(words, chatChannel, chatText), "a line with no channel word splits");
		ctx.EqualStr("Admin", chatChannel, "and takes the default channel");
		ctx.EqualStr("!handled 3", chatText, "and keeps its words");
		words = {"local", "hi", "there"};
		ctx.True(LF_HarnessChat.Split(words, chatChannel, chatText), "a line with a channel word splits");
		ctx.EqualStr("Local", chatChannel, "the channel word is taken");
		ctx.EqualStr("hi there", chatText, "and is not part of the text");
		words = {"admin"};
		ctx.True(LF_HarnessChat.Split(words, chatChannel, chatText), "a lone channel word is the text, not a channel");
		ctx.EqualStr("admin", chatText, "so it is sent as text");
		words = {};
		ctx.False(LF_HarnessChat.Split(words, chatChannel, chatText), "no words, no line");
		ctx.EqualStr("error no_text", LF_HarnessChat.Send("Admin", ""), "an empty line is refused");
		ctx.EqualStr("error slash_is_a_chat_box_command", LF_HarnessChat.Send("Global", "/g hi"), "a chat box command is not a message");

		FileIO.DeleteFile(appsPath);
		FileIO.DeleteFile(rosterPath);
		FileIO.DeleteFile(whitelistPath);
		LF_PoliceApplications.SetSavePath("");
		LF_PoliceApplicationStaff.SetStaffPath("");
		LF_WhitelistManager.SetSavePath("");
	}
}
