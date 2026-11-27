// red-proof: with `RedProof()` on the parsers are not asked, so every parse keeps its wrong default (accepted, with a
// stray uid) and the "refused", "uid", "level" and "valid uid" checks fail.

//! police/staff-parse: the text rules behind a station-desk staff call. `!preject <uid>` is the only chat shape that
//! rejects an application, a staff roster line is `<uid> [level] [# comment]`, and a target uid is only letters, digits,
//! `-` and `_` up to 64 characters (so a field can never forge a roster or application record line).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PoliceStaffParse : LF_Test
{
	override string GetName()
	{
		return "police/staff-parse";
	}

	//------------------------------------------------------------------------------------------------
	protected string Repeat(string piece, int count)
	{
		string text = "";
		for (int i = 0; i < count; i++)
		{
			text = text + piece;
		}

		return text;
	}

	override void Run(LF_TestContext ctx)
	{
		bool plain = false;
		bool spaced = false;
		bool bare = true;
		bool extra = true;
		bool otherVerb = true;
		bool empty = true;
		bool upperVerb = true;
		string plainUid = "-";
		string spacedUid = "-";
		string bareUid = "kept";
		bool onlyUid = false;
		bool withLevel = false;
		bool tabbed = false;
		bool commentOnly = true;
		bool blank = true;
		bool hashGlued = false;
		bool padded = false;
		string onlyUidUid = "-";
		string onlyUidLevel = "-";
		string withLevelUid = "-";
		string withLevelLevel = "-";
		string tabbedUid = "-";
		string tabbedLevel = "-";
		string hashGluedUid = "-";
		string hashGluedLevel = "-";
		string paddedUid = "-";
		string paddedLevel = "-";
		bool okMixed = false;
		bool okSingle = false;
		bool okMax = false;
		bool emptyUid = true;
		bool tooLong = true;
		bool spaceUid = true;
		bool slashUid = true;
		bool quoteUid = true;
		bool newlineUid = true;
		if (!RedProof())
		{
			plain = LF_PoliceApplicationRules.TryParseReject("!preject abc-123", plainUid);
			spaced = LF_PoliceApplicationRules.TryParseReject("  !preject    xyz  ", spacedUid);
			bare = LF_PoliceApplicationRules.TryParseReject("!preject", bareUid);
			string extraUid;
			extra = LF_PoliceApplicationRules.TryParseReject("!preject abc def", extraUid);
			string otherUid;
			otherVerb = LF_PoliceApplicationRules.TryParseReject("!papprove abc", otherUid);
			empty = LF_PoliceApplicationRules.TryParseReject("", otherUid);
			upperVerb = LF_PoliceApplicationRules.TryParseReject("!PREJECT abc", otherUid);

			onlyUid = LF_PoliceApplicationStaff.ParseRosterLine("76561198000000001", onlyUidUid, onlyUidLevel);
			withLevel = LF_PoliceApplicationStaff.ParseRosterLine("uid-2 admin", withLevelUid, withLevelLevel);
			tabbed = LF_PoliceApplicationStaff.ParseRosterLine("uid-3\tmod  # trial", tabbedUid, tabbedLevel);
			string commentUid;
			string commentLevel;
			commentOnly = LF_PoliceApplicationStaff.ParseRosterLine("# only a comment", commentUid, commentLevel);
			blank = LF_PoliceApplicationStaff.ParseRosterLine("   \t ", commentUid, commentLevel);
			hashGlued = LF_PoliceApplicationStaff.ParseRosterLine("uid-4#note", hashGluedUid, hashGluedLevel);
			padded = LF_PoliceApplicationStaff.ParseRosterLine("   uid-5   lead   ", paddedUid, paddedLevel);

			okMixed = LF_PoliceApplicationStaff.IsValidUid("Abc-123_xyz");
			okSingle = LF_PoliceApplicationStaff.IsValidUid("a");
			okMax = LF_PoliceApplicationStaff.IsValidUid(Repeat("a", LF_PoliceApplicationStaff.UID_MAX_LENGTH));
			emptyUid = LF_PoliceApplicationStaff.IsValidUid("");
			tooLong = LF_PoliceApplicationStaff.IsValidUid(Repeat("a", LF_PoliceApplicationStaff.UID_MAX_LENGTH + 1));
			spaceUid = LF_PoliceApplicationStaff.IsValidUid("ab cd");
			slashUid = LF_PoliceApplicationStaff.IsValidUid("ab/cd");
			quoteUid = LF_PoliceApplicationStaff.IsValidUid("ab\"cd");
			newlineUid = LF_PoliceApplicationStaff.IsValidUid("ab\ncd");
		}

		ctx.True(plain, "`!preject <uid>` parses");
		ctx.EqualStr("abc-123", plainUid, "the applicant is the second word");
		ctx.True(spaced, "extra spaces around the words are ignored");
		ctx.EqualStr("xyz", spacedUid, "the applicant is still the second word");
		ctx.False(bare, "`!preject` with no uid is refused");
		ctx.False(extra, "`!preject` with two uids is refused (not a guess which one)");
		ctx.False(otherVerb, "another verb is not a reject");
		ctx.False(empty, "an empty message is not a reject");
		ctx.False(upperVerb, "the verb is exact: upper case is not the command");

		ctx.True(onlyUid, "a roster line with only a uid is a roster line");
		ctx.EqualStr("76561198000000001", onlyUidUid, "its uid is the whole line");
		ctx.EqualStr("", onlyUidLevel, "it has no level");
		ctx.True(withLevel, "a uid and a level parse");
		ctx.EqualStr("uid-2", withLevelUid, "the uid is the first word");
		ctx.EqualStr("admin", withLevelLevel, "the level is the rest of the line");
		ctx.True(tabbed, "a tab separates the level too");
		ctx.EqualStr("uid-3", tabbedUid, "the tab cuts the uid");
		ctx.EqualStr("mod", tabbedLevel, "the comment is cut from the level and the level trimmed");
		ctx.False(commentOnly, "a comment-only line is not a roster line");
		ctx.False(blank, "a blank line is not a roster line");
		ctx.True(hashGlued, "a comment glued to the uid still leaves the uid");
		ctx.EqualStr("uid-4", hashGluedUid, "the uid stops at the #");
		ctx.EqualStr("", hashGluedLevel, "the glued comment is not a level");
		ctx.True(padded, "leading and trailing blanks are trimmed");
		ctx.EqualStr("uid-5", paddedUid, "the uid is the first word after the padding");
		ctx.EqualStr("lead", paddedLevel, "the level has no trailing blanks");

		ctx.True(okMixed, "letters, digits, - and _ are a valid uid");
		ctx.True(okSingle, "one character is long enough");
		ctx.True(okMax, "the longest uid is accepted");
		ctx.False(emptyUid, "an empty uid is refused");
		ctx.False(tooLong, "one character past the longest is refused");
		ctx.False(spaceUid, "a space cannot be in a uid");
		ctx.False(slashUid, "a slash cannot be in a uid");
		ctx.False(quoteUid, "a quote cannot be in a uid (it would break the record line)");
		ctx.False(newlineUid, "a newline cannot be in a uid (it would forge a second record)");
	}
}
