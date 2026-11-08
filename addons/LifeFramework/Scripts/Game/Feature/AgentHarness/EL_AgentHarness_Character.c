// Server-side stand for the agent harness. The client names an entity that
// already exists in the loaded world; the server moves only the requesting
// player's character to just beside it. That is how a play session reaches a
// DebugWorld layer without injecting movement keys. The interaction that
// follows still goes through the normal action checks. Every RPC here passes
// EL_DevGate first: the debug game mode opts in, otherwise administrators only.

// A body posed 18 m up lands in about 2 s; the last pose of a shot is
// reapplied 0.7 s after the first.
const int EL_HARNESS_POSE_SAFE_MS = 8000;

modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! \param entityName World entity to stand beside. Resolved on the server.
	void EL_AskHarnessStand(string entityName)
	{
		Rpc(RpcAsk_EL_HarnessStand, entityName);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_HarnessStand(string entityName)
	{
		// A client could otherwise teleport itself or fire an action out of reach.
		if (!EL_DevGate.CheckCharacterRequest(this, "AgentHarness", "stand"))
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		IEntity target = world.FindEntityByName(entityName);
		if (!target)
		{
			EL_Debug.Error("AgentHarness", string.Format("stand refused: %1 is not in the world", entityName));
			return;
		}

		// One metre to the side keeps the character inside a 3m action radius
		// without planting them inside the prop.
		SetOrigin(target.GetOrigin() + Vector(1.2, 0, 0));
		EL_Debug.Log("AgentHarness", string.Format("stood beside %1", entityName));
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
			return;

		EL_AgentHarness.ApplyUse(this, commandId, className, entityName);
	}
}
