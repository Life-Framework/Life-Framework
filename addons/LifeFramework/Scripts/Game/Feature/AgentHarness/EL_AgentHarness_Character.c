// Server-side stand, use, and state for the agent harness. The client names an
// entity that already exists in the loaded world; the server moves only the
// requesting player's character to just beside it. That is how a play session
// reaches a DebugWorld layer without injecting movement keys. The interaction
// that follows still goes through the normal action checks. Every RPC here
// passes EL_DevGate first: the debug game mode opts in, otherwise
// administrators only. On a dedicated server the result goes back to the
// asking client over an owner RPC, so a refusal is never a silent timeout.

// A body posed 18 m up lands in about 2 s; the last pose of a shot is
// reapplied 0.7 s after the first.
const int EL_HARNESS_POSE_SAFE_MS = 8000;

modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! \param commandId Result id the harness is waiting on.
	//! \param entityName World entity to stand beside. Resolved on the server.
	void EL_AskHarnessStand(int commandId, string entityName)
	{
		if (Replication.IsServer())
		{
			EL_ApplyHarnessStand(commandId, entityName, true);
			return;
		}

		Rpc(RpcAsk_EL_HarnessStand, commandId, entityName);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_HarnessStand(int commandId, string entityName)
	{
		// A client could otherwise teleport itself or fire an action out of reach.
		bool gateOpen = EL_DevGate.CheckCharacterRequest(this, "AgentHarness", "stand");
		EL_ApplyHarnessStand(commandId, entityName, gateOpen);
	}

	//------------------------------------------------------------------------------------------------
	protected void EL_ApplyHarnessStand(int commandId, string entityName, bool gateOpen)
	{
		vector destination;
		string error = EL_AgentHarness.ServerStand(this, entityName, gateOpen, destination);
		if (!error.IsEmpty())
		{
			EL_HarnessReply(commandId, "error", error);
			return;
		}

		// A remote client owns its character's movement and overwrites a server-side move on
		// its next update, so the owner moves (vanilla SCR_PlayersManagerEditorComponent does
		// the same). The listen host's own character is local.
		if (EL_Utils.GetLocalCharacter() == this)
		{
			EL_MoveHarnessBody(destination);
			EL_HarnessReply(commandId, "ok", entityName);
			return;
		}

		Rpc(RpcDo_EL_HarnessStand, commandId, entityName, destination);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_EL_HarnessStand(int commandId, string entityName, vector destination)
	{
		EL_MoveHarnessBody(destination);
		EL_AgentHarness.Reply(commandId, "ok", entityName);
	}

	//------------------------------------------------------------------------------------------------
	protected void EL_MoveHarnessBody(vector destination)
	{
		vector transform[4];
		GetWorldTransform(transform);
		transform[3] = destination;
		Teleport(transform);
		EL_Debug.Log("AgentHarness", string.Format("stood at %1", destination));
	}

	//------------------------------------------------------------------------------------------------
	//! Move this character to a world shot. The player camera follows the body,
	//! which SetCamera on a CameraBase does not.
	void EL_AskHarnessPose(float x, float y, float z, float yaw, float pitch)
	{
		if (Replication.IsServer())
		{
			EL_ApplyHarnessPose(x, y, z, yaw, pitch);
			return;
		}

		Rpc(RpcAsk_EL_HarnessPose, x, y, z, yaw, pitch);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_HarnessPose(float x, float y, float z, float yaw, float pitch)
	{
		// A client could otherwise teleport itself or fire an action out of reach.
		if (!EL_DevGate.CheckCharacterRequest(this, "AgentHarness", "pose"))
			return;

		EL_ApplyHarnessPose(x, y, z, yaw, pitch);

		// A remote client owns its character's movement and overwrites the server
		// pose on its next update (the same reason stand moves on the owner).
		if (EL_Utils.GetLocalCharacter() != this)
			Rpc(RpcDo_EL_HarnessPose, x, y, z, yaw);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_EL_HarnessPose(float x, float y, float z, float yaw)
	{
		vector transform[4];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), transform);
		transform[3] = Vector(x, y, z);
		Teleport(transform);
		EL_Debug.Log("AgentHarness", string.Format("owner posed %1 %2 %3 facing %4", x, y, z, yaw));
	}

	//------------------------------------------------------------------------------------------------
	void EL_ApplyHarnessPose(float x, float y, float z, float yaw, float pitch)
	{
		// A shot pose lifts the body to the camera height (4 m by default, 18 m
		// for a roadmap row). It falls during the settle, and the landing killed
		// the character, which ended every later shot on the death screen.
		// Damage is off while posed and comes back once the body has landed.
		DamageManagerComponent damage = GetDamageManager();
		if (damage)
		{
			damage.EnableDamageHandling(false);
			GetGame().GetCallqueue().Remove(EL_EndHarnessPose);
			GetGame().GetCallqueue().CallLater(EL_EndHarnessPose, EL_HARNESS_POSE_SAFE_MS, false);
		}

		SetOrigin(Vector(x, y, z));
		SetYawPitchRoll(Vector(yaw, pitch, 0));
		EL_Debug.Log("AgentHarness", string.Format("posed %1 %2 %3 facing %4 %5", x, y, z, yaw, pitch));
	}

	//------------------------------------------------------------------------------------------------
	protected void EL_EndHarnessPose()
	{
		DamageManagerComponent damage = GetDamageManager();
		if (!damage)
			return;

		damage.EnableDamageHandling(true);
		EL_Debug.Log("AgentHarness", "pose ended; damage handling restored");
	}

	//------------------------------------------------------------------------------------------------
	//! Perform a named action on the server. Client-side PerformAction no-ops
	//! when this peer does not own the prop, which dropped gathers on the floor.
	//! \param commandId Result id the harness is waiting on.
	//! \param className Action class, for example EL_GatherAction.
	//! \param entityName World entity that hosts the action. Empty searches nearby.
	void EL_AskHarnessUse(int commandId, string className, string entityName)
	{
		if (Replication.IsServer())
		{
			EL_AgentHarness.ApplyUse(this, commandId, className, entityName);
			return;
		}

		Rpc(RpcAsk_EL_HarnessUse, commandId, className, entityName);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_HarnessUse(int commandId, string className, string entityName)
	{
		// A client could otherwise teleport itself or fire an action out of reach.
		if (!EL_DevGate.CheckCharacterRequest(this, "AgentHarness", "use"))
		{
			EL_HarnessReply(commandId, "error", EL_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		EL_AgentHarness.ApplyUse(this, commandId, className, entityName);
	}

	//------------------------------------------------------------------------------------------------
	//! Read this character's account, money, job, and organization on the server.
	//! Read-only, so it does not pass the dev gate: it reports what the player
	//! could already see in their own menus.
	void EL_AskHarnessState(int commandId)
	{
		if (Replication.IsServer())
		{
			EL_HarnessReply(commandId, "ok", EL_AgentHarness.StateLine(this));
			return;
		}

		Rpc(RpcAsk_EL_HarnessState, commandId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_HarnessState(int commandId)
	{
		EL_HarnessReply(commandId, "ok", EL_AgentHarness.StateLine(this));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: hand a harness result to the client that asked. The listen host's
	//! own character writes it directly (an RPC never runs on the sender).
	void EL_HarnessReply(int commandId, string status, string detail)
	{
		if (commandId <= 0)
			return;

		if (EL_Utils.GetLocalCharacter() == this)
		{
			EL_AgentHarness.Reply(commandId, status, detail);
			return;
		}

		Rpc(RpcDo_EL_HarnessResult, commandId, status, detail);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_EL_HarnessResult(int commandId, string status, string detail)
	{
		EL_AgentHarness.Reply(commandId, status, detail);
	}
}
