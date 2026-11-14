//------------------------------------------------------------------------------------------------
//! /licence <NAME> (#988): grants the caller one licence by its LF_ELicenseType name, so a tester can
//! try a gated outlet with and without it (no licence desk exists yet). Administrators only on a live
//! server (LF_DevGate: development worlds and harness builds open it), the same gate as /configreload.
//! The client sends only the name; the server checks it and the gate.
class LF_LicenceGrant
{
	//------------------------------------------------------------------------------------------------
	//! Server: one grant request.
	//! \return True when the licence is now held.
	static bool ServerRun(int playerId, string licenceName)
	{
		if (!Replication.IsServer())
			return false;

		if (!LF_DevGate.HarnessBuild() && !LF_DevGate.IsAllowedForPlayer(playerId))
		{
			LF_Debug.Warn("License", string.Format("/licence refused for player %1: administrators only", playerId));
			return false;
		}

		string name = licenceName.Trim();
		name.ToUpper();
		LF_ELicenseType licence;
		if (!LF_ItemCatalogue.ParseLicence(name, licence))
		{
			LF_Debug.Warn("License", string.Format("/licence refused for player %1: '%2' is not a licence name", playerId, licenceName));
			return false;
		}

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(character);
		if (!licences)
		{
			LF_Debug.Error("License", string.Format("/licence: player %1 has no licence manager", playerId));
			return false;
		}

		if (licences.HasLicense(licence))
		{
			LF_Debug.Info("License", string.Format("/licence: player %1 already holds %2", playerId, name));
			return true;
		}

		bool granted = licences.UnlockLicense(licence, true);
		LF_Debug.Info("License", string.Format("/licence: player %1 granted %2: %3", playerId, name, granted));
		return granted;
	}
};

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
modded class SCR_ChatPanelManager
{
	//------------------------------------------------------------------------------------------------
	override protected void InitDefaultChatCommands()
	{
		super.InitDefaultChatCommands();
		ChatCommandInvoker invoker = GetCommandInvoker("licence");
		if (invoker)
			invoker.Insert(LF_OnLicenceCommand);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnLicenceCommand(SCR_ChatPanel panel, string data)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error("License", "/licence not sent: no local player controller");
			return;
		}

		controller.LF_AskLicenceGrant(data);
	}
};

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Asks the server for one licence by name. The player owns their controller, so the RPC is allowed.
	void LF_AskLicenceGrant(string licenceName)
	{
		if (Replication.IsServer())
			RpcAsk_LF_LicenceGrant(licenceName);
		else
			Rpc(RpcAsk_LF_LicenceGrant, licenceName);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_LicenceGrant(string licenceName)
	{
		LF_LicenceGrant.ServerRun(GetPlayerId(), licenceName);
	}
};
