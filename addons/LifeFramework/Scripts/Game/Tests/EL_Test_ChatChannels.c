// red-proof: expect the OOC global prefix to be "/oocl" (that prefix belongs to
// OOC Local). The suite prints [ELTEST] FAIL chat/channels on
// "ooc global channel is reachable via /ooc". Restored to "/ooc".

// tier: LOGIC
class EL_Test_ChatChannels : EL_Test
{
	override string GetName()
	{
		return "chat/channels";
	}

	override void Run(EL_TestContext ctx)
	{
		ref LF_RoleplayChatStyle global = LF_RoleplayChat.Describe(LF_RoleplayChat.ResolveStyle(BaseChatChannel, "Global"));
		ctx.EqualStr("Twatter", global.m_sName, "global channel is branded Twatter");
		ctx.EqualStr("twatter", global.m_sIconName, "global channel uses the twatter icon");
		ctx.True(global.m_sPrefix.IsEmpty(), "global channel has no prefix");
		ctx.True(global.m_bColoredBackground, "global channel uses a colored background");

		ref LF_RoleplayChatStyle oocGlobal = LF_RoleplayChat.Describe(LF_RoleplayChat.ResolveStyle(BaseChatChannel, "OOCGlobal"));
		ctx.EqualStr("OOC", oocGlobal.m_sName, "ooc global channel is branded OOC");
		ctx.EqualStr("/ooc", oocGlobal.m_sPrefix, "ooc global channel is reachable via /ooc");
		ctx.EqualStr("ooc", oocGlobal.m_sIconName, "ooc global channel uses the ooc icon");
		ctx.False(oocGlobal.m_bColoredBackground, "ooc global channel has no colored background");

		ref LF_RoleplayChatStyle local = LF_RoleplayChat.Describe(LF_RoleplayChat.ResolveStyle(LocalChatChannel, "Local"));
		ctx.EqualStr("/oocl", local.m_sPrefix, "local channel is reachable via /oocl");
		ctx.EqualStr("OOC Local", local.m_sName, "local channel is branded OOC Local");
		ctx.EqualStr("ooc", local.m_sIconName, "local channel shares the ooc icon with the ooc global channel");

		ref LF_RoleplayChatStyle admin = LF_RoleplayChat.Describe(LF_RoleplayChat.ResolveStyle(AdminChatChannel, "Admin"));
		ctx.EqualStr("/admin", admin.m_sPrefix, "admin channel is reachable via /admin");
		ctx.EqualStr("admin", admin.m_sIconName, "admin channel uses the admin icon");
		ctx.EqualStr("Admin", admin.m_sName, "admin channel is branded Admin");
		ctx.True(admin.m_bColoredBackground, "admin channel uses a colored background");

		ctx.True(LF_RoleplayChat.ResolveStyle(BaseChatChannel, "OOC") == LF_ERoleplayChatStyle.SYSTEM, "ooc requires the OOCGlobal channel name");
		ctx.True(LF_RoleplayChat.ResolveStyle(AdminChatChannel, "Global") == LF_ERoleplayChatStyle.ADMIN, "admin styling follows the channel class");

		ctx.False(LF_RoleplayChat.ShowsVonDisplayName(), "VoN does not show display names");
		ctx.False(LF_RoleplayChat.ArmaVisionAllowed(false), "ArmaVision is closed to non-admins");
		ctx.True(LF_RoleplayChat.ArmaVisionAllowed(true), "ArmaVision is open to admins");
	}
}
