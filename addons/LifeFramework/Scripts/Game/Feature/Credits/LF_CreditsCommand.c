//------------------------------------------------------------------------------------------------
//! The /credits chat command (#481): opens the credits screen for the player who typed it.
//! It runs on the client and asks nothing of the server.
[BaseContainerProps(configRoot: true)]
modded class SCR_ChatPanelManager
{
	//------------------------------------------------------------------------------------------------
	override protected void InitDefaultChatCommands()
	{
		super.InitDefaultChatCommands();
		ChatCommandInvoker invoker = GetCommandInvoker("credits");
		if (invoker)
			invoker.Insert(LF_OnCreditsCommand);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnCreditsCommand(SCR_ChatPanel panel, string data)
	{
		LF_CreditsSheet.Open();
	}
}
