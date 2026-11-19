//------------------------------------------------------------------------------------------------
//! Dev impound (#3491): a car only reaches the impound lot at a restart or a tow, so a session proof of the
//! impound desk's collect had no car to collect. `LF_VehicleDevImpound.TakeLatest` takes the caller's newest
//! car off the street the way a tow does (cargo captured, entity deleted), which is the impound state
//! (`LF_VehicleOwnership.IsImpounded`); the desk then collects it for the fee. The server runs it for an
//! administrator or a harness build (`LF_DevGate`, the same gate as /licence); a client only sends the ask.
class LF_VehicleDevImpound
{
	//------------------------------------------------------------------------------------------------
	//! Takes the owner's newest car in the world off the street.
	//! \return The vehicle id now at the impound lot, "" when the owner has no car in the world.
	static string TakeLatest(string ownerUid)
	{
		string vehicleId = LF_VehicleOwnership.LatestInWorld(ownerUid);
		if (vehicleId.IsEmpty())
			return "";

		if (!LF_VehicleOwnership.TakeOffStreet(vehicleId, LF_VehicleOwnership.GetVehicle(vehicleId)))
			return "";

		if (!LF_VehicleOwnership.IsImpounded(vehicleId))
			return "";

		return vehicleId;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one dev impound request from a player.
	//! \return True when a car of the player's is now at the impound lot.
	static bool ServerRun(int playerId)
	{
		if (!Replication.IsServer())
			return false;

		if (!LF_DevGate.HarnessBuild() && !LF_DevGate.IsAllowedForPlayer(playerId))
		{
			LF_Debug.Warn("Vehicle", string.Format("dev impound refused for player %1: administrators only", playerId));
			return false;
		}

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		string uid = LF_Utils.GetPlayerUid(character);
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Vehicle", string.Format("dev impound refused: player %1 has no UID", playerId));
			return false;
		}

		string vehicleId = TakeLatest(uid);
		if (vehicleId.IsEmpty())
		{
			LF_Debug.Info("Vehicle", string.Format("dev impound: %1 has no car in the world", uid));
			return false;
		}

		LF_Debug.Info("Vehicle", string.Format("dev impound: %1 plate=%2 owner=%3 is at the impound lot", vehicleId, LF_VehicleOwnership.GetPlate(vehicleId), uid));
		return true;
	}
};

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Asks the server to impound this player's newest car. The player owns their controller, so the RPC is allowed.
	void LF_AskDevImpound()
	{
		if (Replication.IsServer())
			RpcAsk_LF_DevImpound();
		else
			Rpc(RpcAsk_LF_DevImpound);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_DevImpound()
	{
		LF_VehicleDevImpound.ServerRun(GetPlayerId());
	}
};

//------------------------------------------------------------------------------------------------
//! Harness `impound`: this client asks the server to put its newest car at the impound lot, so the
//! `LF_ImpoundCollectAction` step of a flow has a real car to collect. The server's gate decides and logs.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "impound")
			return super.Handle(cmd);

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_controller");
			return true;
		}

		controller.LF_AskDevImpound();
		LF_AgentHarness.Answer(cmd.Id, "ok", "asked impound");
		return true;
	}
};
