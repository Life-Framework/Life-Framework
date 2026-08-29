// red-proof: change an expected channel style value (e.g. expect the OOC prefix
// to be "/ooc" where the conf says "/oocl", or swap an icon name), then run
// `tools\cli test --tier fast` and watch [ELTEST] FAIL chat/channels.

// tier: LOGIC
class EL_Test_ChatChannels : EL_Test
{
	override string GetName()
	{
		return "chat/channels";
	}

	override void Run(EL_TestContext ctx)
	{
		SCR_ChatMessageStyle admin = new SCR_ChatMessageStyle();
		admin.m_sName = "Admin";
		admin.m_sPrefix = "/admin";
		admin.m_sIconName = "admin";

		ctx.EqualStr("/admin", admin.m_sPrefix, "admin channel is reachable via /admin");
		ctx.EqualStr("admin", admin.m_sIconName, "admin channel uses the admin icon");

		SCR_ChatMessageStyle global = new SCR_ChatMessageStyle();
		global.m_sName = "Twatter";
		global.m_sIconName = "twatter";

		ctx.EqualStr("Twatter", global.m_sName, "global channel is branded Twatter");
		ctx.EqualStr("twatter", global.m_sIconName, "global channel uses the twatter icon");
		ctx.True(global.m_sPrefix.IsEmpty(), "global channel has no prefix");

		SCR_ChatMessageStyle oocGlobal = new SCR_ChatMessageStyle();
		oocGlobal.m_sName = "OOC";
		oocGlobal.m_sPrefix = "/ooc";
		oocGlobal.m_sIconName = "ooc";

		ctx.EqualStr("/ooc", oocGlobal.m_sPrefix, "ooc global channel is reachable via /ooc");
		ctx.EqualStr("ooc", oocGlobal.m_sIconName, "ooc global channel uses the ooc icon");

		SCR_ChatMessageStyle local = new SCR_ChatMessageStyle();
		local.m_sName = "OOC Local";
		local.m_sPrefix = "/oocl";
		local.m_sIconName = "ooc";

		ctx.EqualStr("/oocl", local.m_sPrefix, "local channel is reachable via /oocl");
		ctx.EqualStr("OOC Local", local.m_sName, "local channel is branded OOC Local");

		ctx.EqualStr("ooc", local.m_sIconName, "local channel shares the ooc icon with the ooc global channel");
	}
}