// Server-side stand, use, and state for the agent harness. The client names an
// entity that already exists in the loaded world; the server moves only the
// requesting player's character to just beside it. That is how a play session
// reaches a DebugWorld layer without injecting movement keys. The interaction
// that follows still goes through the normal action checks. Every RPC here
// passes LF_DevGate first: the debug game mode opts in, otherwise
// administrators only. On a dedicated server the result goes back to the
// asking client over an owner RPC, so a refusal is never a silent timeout.

// A body posed 18 m up lands in about 2 s; the last pose of a shot is
// reapplied 0.7 s after the first.
const int LF_HARNESS_POSE_SAFE_MS = 8000;

modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! \param commandId Result id the harness is waiting on.
	//! \param entityName World entity to stand beside. Resolved on the server.
	//! param side e (default), w, n or s: which side of the entity to stand on, facing it.
	void LF_AskHarnessStand(int commandId, string entityName, string side = "e")
	{
		if (Replication.IsServer())
		{
			LF_ApplyHarnessStand(commandId, entityName, true, side);
			return;
		}

		Rpc(RpcAsk_LF_HarnessStand, commandId, entityName, side);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessStand(int commandId, string entityName, string side)
	{
		// A client could otherwise teleport itself or fire an action out of reach.
		bool gateOpen = LF_DevGate.CheckHarnessRequest(this, "stand");
		LF_ApplyHarnessStand(commandId, entityName, gateOpen, side);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ApplyHarnessStand(int commandId, string entityName, bool gateOpen, string side)
	{
		vector destination;
		string error = LF_AgentHarness.ServerStand(this, entityName, gateOpen, destination, side);
		if (!error.IsEmpty())
		{
			LF_HarnessReply(commandId, "error", error);
			return;
		}

		// A remote client owns its character's movement and overwrites a server-side move on
		// its next update, so the owner moves (vanilla SCR_PlayersManagerEditorComponent does
		// the same). The listen host's own character is local.
		if (LF_Utils.GetLocalCharacter() == this)
		{
			LF_MoveHarnessBody(destination, side);
			LF_HarnessReply(commandId, "ok", entityName);
			return;
		}

		Rpc(RpcDo_LF_HarnessStand, commandId, entityName, destination, side);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_HarnessStand(int commandId, string entityName, vector destination, string side)
	{
		LF_MoveHarnessBody(destination, side);
		LF_AgentHarness.Reply(commandId, "ok", entityName);
	}

	//------------------------------------------------------------------------------------------------
	//! Stand at destination facing the entity (LF_AgentHarness.StandYaw), the way a player walks up
	//! to a counter, so the interaction key aims at it.
	protected void LF_MoveHarnessBody(vector destination, string side = "e")
	{
		float yaw = LF_AgentHarness.StandYaw(side);
		vector transform[4];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), transform);
		transform[3] = destination;
		Teleport(transform);
		LF_Debug.Log("AgentHarness", string.Format("stood at %1 facing %2", destination, yaw));
	}

	//------------------------------------------------------------------------------------------------
	//! Move this character to a world shot. The player camera follows the body,
	//! which SetCamera on a CameraBase does not.
	void LF_AskHarnessPose(float x, float y, float z, float yaw, float pitch)
	{
		if (Replication.IsServer())
		{
			LF_ApplyHarnessPose(x, y, z, yaw, pitch);
			return;
		}

		Rpc(RpcAsk_LF_HarnessPose, x, y, z, yaw, pitch);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessPose(float x, float y, float z, float yaw, float pitch)
	{
		// A client could otherwise teleport itself or fire an action out of reach.
		if (!LF_DevGate.CheckHarnessRequest(this, "pose"))
			return;

		LF_ApplyHarnessPose(x, y, z, yaw, pitch);

		// A remote client owns its character's movement and overwrites the server
		// pose on its next update (the same reason stand moves on the owner).
		if (LF_Utils.GetLocalCharacter() != this)
			Rpc(RpcDo_LF_HarnessPose, x, y, z, yaw);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_HarnessPose(float x, float y, float z, float yaw)
	{
		vector transform[4];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), transform);
		transform[3] = Vector(x, y, z);
		Teleport(transform);
		LF_Debug.Log("AgentHarness", string.Format("owner posed %1 %2 %3 facing %4", x, y, z, yaw));
	}

	//------------------------------------------------------------------------------------------------
	void LF_ApplyHarnessPose(float x, float y, float z, float yaw, float pitch)
	{
		// A shot pose lifts the body to the camera height (4 m by default, 18 m
		// for a roadmap row). It falls during the settle, and the landing killed
		// the character, which ended every later shot on the death screen.
		// Damage is off while posed and comes back once the body has landed.
		DamageManagerComponent damage = GetDamageManager();
		if (damage)
		{
			damage.EnableDamageHandling(false);
			GetGame().GetCallqueue().Remove(LF_EndHarnessPose);
			GetGame().GetCallqueue().CallLater(LF_EndHarnessPose, LF_HARNESS_POSE_SAFE_MS, false);
		}

		SetOrigin(Vector(x, y, z));
		SetYawPitchRoll(Vector(yaw, pitch, 0));
		LF_Debug.Log("AgentHarness", string.Format("posed %1 %2 %3 facing %4 %5", x, y, z, yaw, pitch));
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_EndHarnessPose()
	{
		DamageManagerComponent damage = GetDamageManager();
		if (!damage)
			return;

		damage.EnableDamageHandling(true);
		LF_Debug.Log("AgentHarness", "pose ended; damage handling restored");
	}

	//------------------------------------------------------------------------------------------------
	//! Perform a named action on the server. Client-side PerformAction no-ops
	//! when this peer does not own the prop, which dropped gathers on the floor.
	//! \param commandId Result id the harness is waiting on.
	//! \param className Action class, for example LF_GatherAction.
	//! \param entityName World entity that hosts the action. Empty searches nearby.
	void LF_AskHarnessUse(int commandId, string className, string entityName)
	{
		if (Replication.IsServer())
		{
			LF_AgentHarness.ApplyUse(this, commandId, className, entityName);
			return;
		}

		Rpc(RpcAsk_LF_HarnessUse, commandId, className, entityName);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessUse(int commandId, string className, string entityName)
	{
		// A client could otherwise teleport itself or fire an action out of reach.
		if (!LF_DevGate.CheckHarnessRequest(this, "use"))
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		LF_AgentHarness.ApplyUse(this, commandId, className, entityName);
	}

	//------------------------------------------------------------------------------------------------
	//! Harness `die`: the server kills this character, behind the dev gate.
	void LF_AskHarnessDie(int commandId)
	{
		if (Replication.IsServer())
		{
			LF_ApplyHarnessDie(commandId, true);
			return;
		}

		Rpc(RpcAsk_LF_HarnessDie, commandId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessDie(int commandId)
	{
		// A client could otherwise end its own life outside the game's damage path.
		bool gateOpen = LF_DevGate.CheckCharacterRequest(this, "AgentHarness", "die");
		LF_ApplyHarnessDie(commandId, gateOpen);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ApplyHarnessDie(int commandId, bool gateOpen)
	{
		if (!gateOpen)
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damage)
		{
			LF_HarnessReply(commandId, "error", "no_damage_manager");
			return;
		}

		// Reply before the kill: the owner RPC needs a live controlled body to land on.
		LF_HarnessReply(commandId, "ok", "died");
		damage.EnableDamageHandling(true);
		damage.Kill(Instigator.CreateInstigator(this));
		LF_Debug.Info("AgentHarness", "character killed by the harness");
	}

	//------------------------------------------------------------------------------------------------
	//! Harness `shoot`: this character deals one lethal hit to a player, behind the dev gate.
	void LF_AskHarnessShoot(int commandId, int targetPlayerId)
	{
		if (Replication.IsServer())
		{
			LF_ApplyHarnessShoot(commandId, targetPlayerId, true);
			return;
		}

		Rpc(RpcAsk_LF_HarnessShoot, commandId, targetPlayerId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessShoot(int commandId, int targetPlayerId)
	{
		bool gateOpen = LF_DevGate.CheckCharacterRequest(this, "AgentHarness", "shoot");
		LF_ApplyHarnessShoot(commandId, targetPlayerId, gateOpen);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a KINETIC hit of 1000 on the target's head through HandleDamage, with this
	//! character as the instigator. Replies with whether the zone refused it and whether the
	//! target still lives.
	protected void LF_ApplyHarnessShoot(int commandId, int targetPlayerId, bool gateOpen)
	{
		if (!gateOpen)
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		IEntity target = GetGame().GetPlayerManager().GetPlayerControlledEntity(targetPlayerId);
		if (!target || target == this)
		{
			LF_HarnessReply(commandId, "error", "no_target");
			return;
		}

		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(target.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damage)
		{
			LF_HarnessReply(commandId, "error", "no_damage_manager");
			return;
		}

		HitZone head = damage.GetHeadHitZone();
		if (!head)
			head = damage.GetDefaultHitZone();

		vector hitPosDirNorm[3];
		hitPosDirNorm[0] = target.GetOrigin() + Vector(0, 1.6, 0);
		hitPosDirNorm[1] = vector.Direction(GetOrigin(), target.GetOrigin()).Normalized();
		hitPosDirNorm[2] = vector.Up;
		SCR_DamageContext context = new SCR_DamageContext(EDamageType.KINETIC, 1000, hitPosDirNorm, target, head, Instigator.CreateInstigator(this), null, -1, -1);
		context.damageSource = this;

		int refusedBefore = LF_Zones.GetRefusedCount();
		damage.HandleDamage(context);
		int refused = LF_Zones.GetRefusedCount() - refusedBefore;
		int alive = 1;
		if (damage.GetState() == EDamageState.DESTROYED)
			alive = 0;

		LF_Debug.Info("AgentHarness", string.Format("shot player %1: refused=%2 alive=%3", targetPlayerId, refused, alive));
		LF_HarnessReply(commandId, "ok", string.Format("shot player=%1 refused=%2 alive=%3", targetPlayerId, refused, alive));
	}

	//------------------------------------------------------------------------------------------------
	//! Read this character's account, money, job, and organization on the server.
	//! Read-only, so it does not pass the dev gate: it reports what the player
	//! could already see in their own menus.
	void LF_AskHarnessState(int commandId)
	{
		if (Replication.IsServer())
		{
			LF_HarnessReply(commandId, "ok", LF_AgentHarness.StateLine(this));
			return;
		}

		Rpc(RpcAsk_LF_HarnessState, commandId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessState(int commandId)
	{
		LF_HarnessReply(commandId, "ok", LF_AgentHarness.StateLine(this));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: hand a harness result to the client that asked. The listen host's
	//! own character writes it directly (an RPC never runs on the sender).
	void LF_HarnessReply(int commandId, string status, string detail)
	{
		if (commandId <= 0)
			return;

		if (LF_Utils.GetLocalCharacter() == this)
		{
			LF_AgentHarness.Reply(commandId, status, detail);
			return;
		}

		Rpc(RpcDo_LF_HarnessResult, commandId, status, detail);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_HarnessResult(int commandId, string status, string detail)
	{
		LF_AgentHarness.Reply(commandId, status, detail);
	}

	//------------------------------------------------------------------------------------------------
	//! Harness melee swing at the current aim. The owning client has already raised
	//! its own melee flag (animation); the server raises it on its copy of the input
	//! context with the client's aim, so the hit is traced and applied where damage is
	//! authoritative. Behind the dev gate.
	void LF_AskHarnessMelee(float aimYaw, float aimPitch)
	{
		if (Replication.IsServer())
		{
			LF_ApplyHarnessMelee(aimYaw, aimPitch);
			return;
		}

		Rpc(RpcAsk_LF_HarnessMelee, aimYaw, aimPitch);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessMelee(float aimYaw, float aimPitch)
	{
		if (!LF_DevGate.CheckHarnessRequest(this, "melee"))
			return;

		LF_ApplyHarnessMelee(aimYaw, aimPitch);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ApplyHarnessMelee(float aimYaw, float aimPitch)
	{
		CharacterControllerComponent controller = GetCharacterController();
		if (!controller || !controller.GetInputContext())
			return;

		CharacterInputContext context = controller.GetInputContext();
		context.SetAimingAngles(Vector(aimYaw, aimPitch, 0));
		context.SetMeleeAttack(true);
		LF_Debug.Log("AgentHarness", string.Format("melee swing on the server, pitch %1", aimPitch * Math.RAD2DEG));
	}
}
