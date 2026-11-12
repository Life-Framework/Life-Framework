//------------------------------------------------------------------------------------------------
//! /configreload (#285): the server reads config.json again, re-prices with it, and streams the new
//! values to every connected client. Administrators only on a live server (LF_DevGate: development
//! worlds and harness builds open it). The client only asks; the server decides and reads its own file.
class LF_ConfigReload
{
	//------------------------------------------------------------------------------------------------
	//! Server: the gate.
	static bool IsAllowed(int playerId)
	{
		return LF_DevGate.HarnessBuild() || LF_DevGate.IsAllowedForPlayer(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one reload request.
	//! \return True when the config was read again.
	static bool ServerRun(int playerId)
	{
		if (!Replication.IsServer())
			return false;

		if (!IsAllowed(playerId))
		{
			LF_Debug.Warn(LF_ConfigService.FEATURE, string.Format("reload refused for player %1: administrators only", playerId));
			return false;
		}

		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("reload asked by player %1", playerId));
		LF_ConfigService.Boot();
		LF_ConfigStream.ApplyToFeatures();

		LF_GameModeRoleplay gameMode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.LF_PublishConfigStream();
		return true;
	}
};

//------------------------------------------------------------------------------------------------
modded class SCR_ChatPanelManager
{
	//------------------------------------------------------------------------------------------------
	override protected void InitDefaultChatCommands()
	{
		super.InitDefaultChatCommands();
		ChatCommandInvoker invoker = GetCommandInvoker("configreload");
		if (invoker)
			invoker.Insert(LF_OnConfigReloadCommand);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnConfigReloadCommand(SCR_ChatPanel panel, string data)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_ConfigService.FEATURE, "reload not sent: no local player controller");
			return;
		}

		controller.LF_AskConfigReload();
	}
};

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Asks the server to read config.json again. The player owns their controller, so the RPC is allowed.
	void LF_AskConfigReload()
	{
		if (Replication.IsServer())
			RpcAsk_LF_ConfigReload();
		else
			Rpc(RpcAsk_LF_ConfigReload);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_ConfigReload()
	{
		LF_ConfigReload.ServerRun(GetPlayerId());
	}
};
