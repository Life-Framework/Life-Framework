//------------------------------------------------------------------------------------------------
//! The /report chat command (#241): a player flags their recent incidents for staff in one press.
//! The command runs on the client and only asks the server; the server picks the incidents.
[BaseContainerProps(configRoot: true)]
modded class SCR_ChatPanelManager
{
	//------------------------------------------------------------------------------------------------
	override protected void InitDefaultChatCommands()
	{
		super.InitDefaultChatCommands();
		ChatCommandInvoker invoker = GetCommandInvoker("report");
		if (invoker)
			invoker.Insert(LF_OnReportCommand);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnReportCommand(SCR_ChatPanel panel, string data)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_IncidentFlags.FEATURE, "report not sent: no local player controller");
			return;
		}

		controller.LF_AskReport();
	}
}
