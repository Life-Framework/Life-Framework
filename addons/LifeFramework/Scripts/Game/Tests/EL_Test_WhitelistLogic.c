// red-proof: swap the expected enum value (e.g. expect CONNECT where POLICE is set)
// or insert a UUID that is not in the list, then run `tools\cli test --tier fast`
// and watch [ELTEST] FAIL whitelist/logic.

// tier: LOGIC
class EL_Test_WhitelistLogic : EL_Test
{
	override string GetName()
	{
		return "whitelist/logic";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.Equal(1, EL_WhitelistType.CONNECT, "CONNECT is bit 0");
		ctx.Equal(2, EL_WhitelistType.POLICE, "POLICE is bit 1");
		ctx.Equal(4, EL_WhitelistType.MEDIC, "MEDIC is bit 2");

		ctx.True(SCR_Enum.HasPartialFlag(EL_WhitelistType.POLICE | EL_WhitelistType.MEDIC, EL_WhitelistType.POLICE), "partial flag match with two flags set");
		ctx.True(SCR_Enum.HasPartialFlag(EL_WhitelistType.POLICE | EL_WhitelistType.MEDIC, EL_WhitelistType.MEDIC), "partial flag match picks the other flag");
		ctx.False(SCR_Enum.HasPartialFlag(EL_WhitelistType.CONNECT, EL_WhitelistType.POLICE), "no match when the flag is absent");
		ctx.True(SCR_Enum.HasPartialFlag(EL_WhitelistType.CONNECT, EL_WhitelistType.CONNECT | EL_WhitelistType.POLICE), "partial flag matches when a required flag is present in flags");

		EL_Whitelist whitelist = new EL_Whitelist();
		whitelist.m_eType = EL_WhitelistType.POLICE;
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
class EL_Test_WhitelistCommandParse : EL_Test
{
	override string GetName()
	{
		return "whitelist/command-parse";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_WhitelistType type;
		string uuid;

		ctx.True(AdminChatChannel.TryParseWhitelistCommand("!wadd POLICE 544-242-544-242", type, uuid), "well-formed wadd command parses");
		ctx.True(type == EL_WhitelistType.POLICE, "parsed type is the requested whitelist");
		ctx.EqualStr("544-242-544-242", uuid, "parsed uuid matches the third token");

		ctx.True(AdminChatChannel.TryParseWhitelistCommand("!wadd MEDIC uid-9", type, uuid), "medic command parses");
		ctx.True(type == EL_WhitelistType.MEDIC, "parsed type is MEDIC");

		ctx.False(AdminChatChannel.TryParseWhitelistCommand("!wadd NOPE uid-9", type, uuid), "unknown type is rejected");
		ctx.False(AdminChatChannel.TryParseWhitelistCommand("!wadd POLICE", type, uuid), "missing uuid is rejected");
		ctx.False(AdminChatChannel.TryParseWhitelistCommand("hello world", type, uuid), "non-command message is rejected");
		ctx.False(AdminChatChannel.TryParseWhitelistCommand("", type, uuid), "empty message is rejected");
	}
}