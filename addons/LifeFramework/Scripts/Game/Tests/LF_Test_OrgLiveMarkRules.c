// red-proof: 2026-11-23, cli test --no-build --red LF_Test_OrgLiveMarkRules: RED PROOF OK (RedProof() skips the rule calls; "a member who left loses their mark at once", "a member who joined another organization loses the old mark" and "a key that is not a live key is never kept" fail).
//! gangs/live-mark-rules (#4222, card C11, N25): a downed or jailed member's mark is grey, a viewer in
//! custody sees no marks, and a member who leaves loses their mark at once. Pure rules, no world.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OrgLiveMarkRules : LF_Test
{
	protected static const int LIVE = 5;
	protected static const int GREY = 12;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "gangs/live-mark-rules";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Grey: downed or in custody, never when up and free.
		bool upFree;
		bool downed;
		bool jailed;
		bool both;
		if (!RedProof())
		{
			upFree = LF_OrgLiveMarkRules.IsGrey(false, false);
			downed = LF_OrgLiveMarkRules.IsGrey(true, false);
			jailed = LF_OrgLiveMarkRules.IsGrey(false, true);
			both = LF_OrgLiveMarkRules.IsGrey(true, true);
		}

		ctx.True(!upFree, "a member who is up and free keeps a live mark");
		ctx.True(downed, "a downed member's mark is grey");
		ctx.True(jailed, "a jailed member's mark is grey");
		ctx.True(both, "downed and jailed is still grey");

		int liveColour;
		int greyColour;
		if (!RedProof())
		{
			liveColour = LF_OrgLiveMarkRules.ColourFor(false, LIVE, GREY);
			greyColour = LF_OrgLiveMarkRules.ColourFor(true, LIVE, GREY);
		}

		ctx.Equal(LIVE, liveColour, "a live mark takes the live colour");
		ctx.Equal(GREY, greyColour, "a grey mark takes the grey colour");

		// Viewer: own organization's marks only, none in custody.
		bool member;
		bool outsider;
		bool loner;
		bool custody;
		bool unmarked;
		if (!RedProof())
		{
			member = LF_OrgLiveMarkRules.ViewerSees("org-a", "org-a", false);
			outsider = LF_OrgLiveMarkRules.ViewerSees("org-a", "org-b", false);
			loner = LF_OrgLiveMarkRules.ViewerSees("org-a", "", false);
			custody = LF_OrgLiveMarkRules.ViewerSees("org-a", "org-a", true);
			unmarked = LF_OrgLiveMarkRules.ViewerSees("", "", false);
		}

		ctx.True(member, "a member sees their own organization's marks");
		ctx.True(!outsider, "a member of another organization sees none");
		ctx.True(!loner, "a player in no organization sees none");
		ctx.True(!custody, "a viewer in custody sees no marks, not even their own organization's");
		ctx.True(!unmarked, "two empty organization ids never match");

		// Leaving: the mark goes when the character is no longer in the key's organization.
		string key = LF_OrgLiveMarks.KeyOf("org-a", "char-1");
		string keyOrg;
		string keyChar;
		bool dropWhenStaying;
		bool dropWhenLeft;
		bool dropWhenMoved;
		bool dropForeignKey;
		if (!RedProof())
		{
			keyOrg = LF_OrgLiveMarkRules.OrgIdOfKey(key);
			keyChar = LF_OrgLiveMarkRules.CharacterIdOfKey(key);
			dropWhenStaying = LF_OrgLiveMarkRules.DropKey(key, "org-a");
			dropWhenLeft = LF_OrgLiveMarkRules.DropKey(key, "");
			dropWhenMoved = LF_OrgLiveMarkRules.DropKey(key, "org-b");
			dropForeignKey = LF_OrgLiveMarkRules.DropKey("org:org-a:3", "org-a");
		}

		ctx.EqualStr("org-a", keyOrg, "the key names its organization");
		ctx.EqualStr("char-1", keyChar, "the key names its character");
		ctx.True(!dropWhenStaying, "a member who stays keeps their mark");
		ctx.True(dropWhenLeft, "a member who left loses their mark at once");
		ctx.True(dropWhenMoved, "a member who joined another organization loses the old mark");
		ctx.True(dropForeignKey, "a key that is not a live key is never kept");
		ctx.EqualStr("", LF_OrgLiveMarkRules.OrgIdOfKey("orglive:"), "a key with no organization names none");
		ctx.EqualStr("", LF_OrgLiveMarkRules.CharacterIdOfKey("orglive:org-a:"), "a key with no character names none");
	}
}
