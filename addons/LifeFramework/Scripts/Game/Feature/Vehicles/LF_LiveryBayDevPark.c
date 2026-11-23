//------------------------------------------------------------------------------------------------
//! Dev park at the respray bay (#4589): a session proof cannot drive a car, so `baypark` sets the caller's
//! newest car in the world on the pad of the bay they stand beside, the same spot a respray brings it back
//! to. The server runs it for an administrator or a harness build (`LF_DevGate`, as the dev impound does);
//! a client only sends the ask.
class LF_LiveryBayDevPark
{
	//------------------------------------------------------------------------------------------------
	//! Sets the owner's newest car in the world on the free pad of the bay `user` stands beside.
	//! \return The vehicle id parked, "" when no bay is in reach, the owner has no car in the world or no pad is free.
	static string ParkLatest(IEntity user)
	{
		IEntity bay = LF_LiveryBay.BayNear(user);
		if (!bay)
		{
			LF_Debug.Info(LF_LiveryBay.FEATURE, "dev park: no respray bay within reach");
			return "";
		}

		LF_VehicleSpawnPointComponent pad = LF_Component<LF_VehicleSpawnPointComponent>.Find(bay);
		string vehicleId = LF_VehicleOwnership.LatestInWorld(LF_Utils.GetPlayerUid(user));
		IEntity vehicle = LF_VehicleOwnership.GetVehicle(vehicleId);
		if (!pad || !vehicle)
		{
			LF_Debug.Info(LF_LiveryBay.FEATURE, "dev park: the caller has no car in the world");
			return "";
		}

		vector origin;
		vector angles;
		if (!LF_VehicleSpawnPointComponent.FindFreePad(pad.GetShopId(), origin, angles))
			return "";

		origin[1] = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]) + 0.5;
		vehicle.SetOrigin(origin);
		vehicle.SetAngles(angles);
		vehicle.Update();
		Physics physics = vehicle.GetPhysics();
		if (physics)
		{
			physics.SetVelocity(vector.Zero);
			physics.SetAngularVelocity(vector.Zero);
		}

		LF_Debug.Info(LF_LiveryBay.FEATURE, string.Format("dev park: %1 plate=%2 is on the pad %3", vehicleId, LF_VehicleOwnership.GetPlate(vehicleId), pad.GetShopId()));
		return vehicleId;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one dev park request from a player.
	static bool ServerRun(int playerId)
	{
		if (!Replication.IsServer())
			return false;

		if (!LF_DevGate.HarnessBuild() && !LF_DevGate.IsAllowedForPlayer(playerId))
		{
			LF_Debug.Warn(LF_LiveryBay.FEATURE, string.Format("dev park refused for player %1: administrators only", playerId));
			return false;
		}

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		return !ParkLatest(character).IsEmpty();
	}
};

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_AskDevBayPark()
	{
		if (Replication.IsServer())
			RpcAsk_LF_DevBayPark();
		else
			Rpc(RpcAsk_LF_DevBayPark);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_DevBayPark()
	{
		LF_LiveryBayDevPark.ServerRun(GetPlayerId());
	}
};

//------------------------------------------------------------------------------------------------
//! Harness `baypark`: this client asks the server to set its newest car on the bay beside it.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "baypark")
			return super.Handle(cmd);

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_controller");
			return true;
		}

		controller.LF_AskDevBayPark();
		LF_AgentHarness.Answer(cmd.Id, "ok", "asked baypark");
		return true;
	}
};
