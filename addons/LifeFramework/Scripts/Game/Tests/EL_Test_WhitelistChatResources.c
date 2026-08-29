// red-proof: reference a resource path that does not exist (e.g. append
// "UI/Layouts/HUD/Chat/Missing.layout" to the list), then run
// `tools\cli test --tier all` and watch [ELTEST] FAIL whitelistchat/resources.
// The chat_imageset.edds / el_chat_48.imageset are not loaded here: the dedicated
// server registers no texture/image loader, so a direct load always fails headless
// (same convention as EL_Test_MoneyRework's texture files).

// tier: WORLD
class EL_Test_WhitelistChatResources : EL_Test
{
	override string GetName()
	{
		return "whitelistchat/resources";
	}

	override void Run(EL_TestContext ctx)
	{
		array<string> resources = {
			"{E67EAC45379783C0}Prefabs/MP/Modes/Roleplay/Chat_Roleplay.et",
			"{C6940227F886B8EF}Prefabs/Props/Signs/EL_WhitelistSign.et",
			"{D89BBA18F99053D6}UI/Layouts/HUD/Chat/ChatHud.layout",
			"{28C54D576A8997CF}UI/Layouts/HUD/Chat/ChatPanel.layout",
			"{973C90F6B6135A50}UI/Layouts/HUD/Chat/ChatMessageLine.layout",
			"{5980F59BAAC4DDDB}Configs/Chat/AdminChannel.conf",
			"{A054AAB5A57BE2DD}Configs/Chat/GlobalChannel.conf",
			"{4B57F6B08155F18A}Configs/Chat/GlobalOocChannel.conf",
			"{A6E8FE0ABB6C67DD}Configs/Chat/LocalChannel.conf"
		};

		foreach (string path : resources)
		{
			Resource res = Resource.Load(path);
			ctx.True(res.IsValid(), string.Format("resource loads: %1", path));
		}
	}
}