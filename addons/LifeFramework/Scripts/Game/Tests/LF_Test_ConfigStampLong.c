// red-proof: 2026-11-17 `--red` RED-OK; on the unfixed StampVersion `config/stamp-long` failed "an early key keeps every char (expected 24051, got 8209)", the same for a late key (got 8193) and an added key (got 8210); green after the piecewise join (--only).
// tier: LOGIC
//! LF_ConfigMigrator.StampVersion on a config past the 8191-char Enforce string cap (#2671,
//! docs/architecture/enforce-string-limits.md site 1). Substring clamps a long result silently, so
//! the old splice lost the tail of any config over 8 KB at its first migration. The stamped text
//! must equal the input except for the version.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ConfigStampLong : LF_Test
{
	override string GetName()
	{
		return "config/stamp-long";
	}

	//! A string of about size chars built by + from short pieces, which never cuts.
	protected static string Filler(int size)
	{
		string row = "\"k\":\"0123456789012345678901234567890123456789012345678901234567890123456789\",";
		string result;
		while (result.Length() < size)
		{
			result += row;
		}
		return result;
	}

	override void Run(LF_TestContext ctx)
	{
		string body = Filler(24000);
		ctx.True(body.Length() > 20000, "the fixture is past 20 KB");

		// The key is first: the tail (the old Substring(j, len - j)) is past the cap.
		string early = "{\"schemaVersion\":1," + body + "\"end\":1}";
		string earlyWant = "{\"schemaVersion\":2," + body + "\"end\":1}";
		string stamped = early;
		if (!RedProof())
			stamped = LF_ConfigMigrator.StampVersion(early, 2);
		ctx.Equal(earlyWant.Length(), stamped.Length(), "an early key keeps every char");
		ctx.True(stamped == earlyWant, "an early key leaves the tail byte-identical");

		// The key is last: the head (the old Substring(0, i)) is past the cap.
		string late = "{" + body + "\"schemaVersion\":1}";
		string lateWant = "{" + body + "\"schemaVersion\":2}";
		stamped = LF_ConfigMigrator.StampVersion(late, 2);
		ctx.Equal(lateWant.Length(), stamped.Length(), "a late key keeps every char");
		ctx.True(stamped == lateWant, "a late key leaves the head byte-identical");

		// No key: it is added after the first brace and the rest is kept.
		string bare = "{" + body + "\"end\":1}";
		string bareWant = "{\"schemaVersion\":3," + body + "\"end\":1}";
		stamped = LF_ConfigMigrator.StampVersion(bare, 3);
		ctx.Equal(bareWant.Length(), stamped.Length(), "an added key keeps every char");
		ctx.True(stamped == bareWant, "an added key leaves the rest byte-identical");
		ctx.Equal(3, LF_ConfigMigrator.PeekVersion(stamped), "the long stamped document still parses at v3");

		// A small config is unchanged in behaviour.
		ctx.True(LF_ConfigMigrator.StampVersion("{\"schemaVersion\":1,\"a\":1}", 2) == "{\"schemaVersion\":2,\"a\":1}", "a small config stamps as before");
		ctx.True(LF_ConfigMigrator.StampVersion("{\"a\":1}", 2) == "{\"schemaVersion\":2,\"a\":1}", "a small config gets the key as before");
		ctx.True(LF_ConfigMigrator.StampVersion("{}", 2) == "{\"schemaVersion\":2}", "an empty object gets the key without a comma");
	}
};

