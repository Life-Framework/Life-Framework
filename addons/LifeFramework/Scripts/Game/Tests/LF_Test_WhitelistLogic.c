// red-proof: swap the expected enum value (e.g. expect CONNECT where POLICE is set)
// or insert a UUID that is not in the list, then run `tools\cli test --tier fast`
// and watch [LFTEST] FAIL whitelist/logic.

// tier: LOGIC
class LF_Test_WhitelistLogic : LF_Test
{
	override string GetName()
	{
		return "whitelist/logic";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(1, LF_WhitelistType.CONNECT, "CONNECT is bit 0");
		ctx.Equal(2, LF_WhitelistType.POLICE, "POLICE is bit 1");
		ctx.Equal(4, LF_WhitelistType.MEDIC, "MEDIC is bit 2");

		ctx.True(SCR_Enum.HasPartialFlag(LF_WhitelistType.POLICE | LF_WhitelistType.MEDIC, LF_WhitelistType.POLICE), "partial flag match with two flags set");
		ctx.True(SCR_Enum.HasPartialFlag(LF_WhitelistType.POLICE | LF_WhitelistType.MEDIC, LF_WhitelistType.MEDIC), "partial flag match picks the other flag");
		ctx.False(SCR_Enum.HasPartialFlag(LF_WhitelistType.CONNECT, LF_WhitelistType.POLICE), "no match when the flag is absent");
		ctx.True(SCR_Enum.HasPartialFlag(LF_WhitelistType.CONNECT, LF_WhitelistType.CONNECT | LF_WhitelistType.POLICE), "partial flag matches when a required flag is present in flags");

		LF_Whitelist whitelist = new LF_Whitelist();
		whitelist.m_eType = LF_WhitelistType.POLICE;
		whitelist.m_aWhitelistUuids.Insert("uuid-alpha");
		whitelist.m_aWhitelistUuids.Insert("uuid-beta");

		ctx.True(whitelist.VerifyPlayer("uuid-alpha"), "known UUID verifies");
		ctx.True(whitelist.VerifyPlayer("uuid-beta"), "second known UUID verifies");
		ctx.False(whitelist.VerifyPlayer("uuid-unknown"), "unknown UUID is rejected");
		ctx.False(whitelist.VerifyPlayer(""), "empty UUID is rejected");
	}
}

//------------------------------------------------------------------------------------------------
// red-proof: expect a wrong type or token count (e.g. claim "!wadd POLICE" parses)
// and the fast tier fails on the command parse assertions.

// tier: LOGIC
class LF_Test_WhitelistCommandParse : LF_Test
{
	override string GetName()
	{
		return "whitelist/command-parse";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_WhitelistType type;
		string uuid;

		ctx.True(AdminChatChannel.TryParseWhitelistCommand("!wadd POLICE 544-242-544-242", type, uuid), "well-formed wadd command parses");
		ctx.True(type == LF_WhitelistType.POLICE, "parsed type is the requested whitelist");
		ctx.EqualStr("544-242-544-242", uuid, "parsed uuid matches the third token");

		ctx.True(AdminChatChannel.TryParseWhitelistCommand("!wadd MEDIC uid-9", type, uuid), "medic command parses");
		ctx.True(type == LF_WhitelistType.MEDIC, "parsed type is MEDIC");

		ctx.False(AdminChatChannel.TryParseWhitelistCommand("!wadd NOPE uid-9", type, uuid), "unknown type is rejected");
		ctx.False(AdminChatChannel.TryParseWhitelistCommand("!wadd POLICE", type, uuid), "missing uuid is rejected");
		ctx.False(AdminChatChannel.TryParseWhitelistCommand("hello world", type, uuid), "non-command message is rejected");
		ctx.False(AdminChatChannel.TryParseWhitelistCommand("", type, uuid), "empty message is rejected");
	}
}