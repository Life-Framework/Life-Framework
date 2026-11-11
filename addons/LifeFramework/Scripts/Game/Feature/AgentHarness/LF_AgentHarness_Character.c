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
		// A real death, not the downed state a lethal hit would now cause (LF_Downed.Finish).
		damage.EnableDamageHandling(true);
		LF_Downed.Finish(this, Instigator.CreateInstigator(this));
		LF_Debug.Info("AgentHarness", "character killed by the harness");
	}

	//------------------------------------------------------------------------------------------------
	//! Harness `scanner`: the server puts a police scanner in this character's inventory, behind the
	//! dev gate (the fence path needs a crime record and cash; the proof is about hearing).
	void LF_AskHarnessScanner(int commandId, string tool = "scanner")
	{
		if (Replication.IsServer())
		{
			LF_ApplyHarnessScanner(commandId, true, tool);
			return;
		}

		Rpc(RpcAsk_LF_HarnessScanner, commandId, tool);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessScanner(int commandId, string tool)
	{
		bool gateOpen;
		if (tool == "stash" || tool == "park" || tool == "apples")
		{
			// The playtest journey (#348) has no debug menu: the harness build's own gate lets the
			// stand-ins for dragging an item, driving a car and picking apples through.
			gateOpen = LF_DevGate.CheckHarnessRequest(this, tool);
		}
		else
		{
			gateOpen = LF_DevGate.CheckCharacterRequest(this, "AgentHarness", "scanner");
		}

		LF_ApplyHarnessScanner(commandId, gateOpen, tool);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ApplyHarnessScanner(int commandId, bool gateOpen, string tool)
	{
		if (!gateOpen)
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		if (tool == "apples")
		{
			// Raw goods for the chain stations (#417): eight apples, two loads of the example chain.
			int apples = LF_InventoryUtils.AddAmount(this, "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et", 8);
			LF_HarnessReply(commandId, "ok", string.Format("apples added=%1", apples));
			return;
		}

		if (tool == "stash")
		{
			// Stands in for dragging an apple into the stash on the inventory screen (#348): the same
			// move the screen asks of the server, so the member check in CanStoreItem decides.
			IEntity stash = GetGame().GetWorld().FindEntityByName("Roadmap_Hideout_Stash");
			BaseInventoryStorageComponent target;
			if (stash)
				target = LF_Component<LF_HideoutStashStorageComponent>.Find(stash);

			InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(this);
			if (!target || !manager)
			{
				LF_HarnessReply(commandId, "error", "no_stash");
				return;
			}

			array<IEntity> apples = LF_InventoryUtils.FindItemsByPrefab(manager, "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et");
			if (apples.IsEmpty())
			{
				LF_HarnessReply(commandId, "error", "no_apple");
				return;
			}

			if (manager.TryMoveItemToStorage(apples[0], target))
				LF_HarnessReply(commandId, "ok", "stash moved=1");
			else
				LF_HarnessReply(commandId, "ok", "stash moved=0");

			return;
		}

		if (tool == "park")
		{
			// Stands in for driving the bought car to the garage (#348): the owner's car in the world
			// is set down beside the Roadmap_Garage counter, where Store takes it.
			string vehicleId = LF_VehicleOwnership.LatestInWorld(LF_Utils.GetPlayerUid(this));
			IEntity vehicle = LF_VehicleOwnership.GetVehicle(vehicleId);
			IEntity garage = GetGame().GetWorld().FindEntityByName("Roadmap_Garage");
			if (!vehicle || !garage)
			{
				LF_HarnessReply(commandId, "error", "no_vehicle");
				return;
			}

			vector spot = garage.GetOrigin() + "8 0 0";
			spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]) + 0.5;
			vehicle.SetOrigin(spot);
			vehicle.Update();
			Physics physics = vehicle.GetPhysics();
			if (physics)
			{
				physics.SetVelocity(vector.Zero);
				physics.SetAngularVelocity(vector.Zero);
			}

			LF_HarnessReply(commandId, "ok", "park " + LF_VehicleOwnership.GetPlate(vehicleId));
			return;
		}

		if (tool == "moonshine")
		{
			// Raw goods for one mash of the moonshine chain (#420): four plums and two cabbages.
			int plums = LF_InventoryUtils.AddAmount(this, "{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et", 4);
			int cabbages = LF_InventoryUtils.AddAmount(this, "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et", 2);
			LF_HarnessReply(commandId, "ok", string.Format("moonshine plums=%1 cabbages=%2", plums, cabbages));
			return;
		}

		if (tool == "keys")
		{
			// Gives a house key and a vehicle key, and sets one of each on the ground ahead, to look
			// at the key models (#613).
			int houseKeys = LF_InventoryUtils.AddAmount(this, "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et", 1);
			int carKeys = LF_InventoryUtils.AddAmount(this, "{9D94B834AF514B4A}Prefabs/Items/Roleplay/VehicleKey.et", 1);
			vector xform[4];
			GetWorldTransform(xform);
			vector ahead = GetOrigin() + xform[2] * 0.8;
			ahead[1] = GetGame().GetWorld().GetSurfaceY(ahead[0], ahead[2]) + 0.05;
			LF_Utils.SpawnEntityPrefab("{3735389521255DBE}Prefabs/Items/HouseKeyItem.et", ahead - xform[0] * 0.1);
			LF_Utils.SpawnEntityPrefab("{9D94B834AF514B4A}Prefabs/Items/Roleplay/VehicleKey.et", ahead + xform[0] * 0.1);
			LF_HarnessReply(commandId, "ok", string.Format("keys house=%1 vehicle=%2", houseKeys, carKeys));
			return;
		}

		if (tool == "shelf")
		{
			// A shopper taking the first shelf item in reach of the physical store (#202), through the
			// same inventory insert a pickup uses. The server tags it unpaid on its next look.
			LF_HarnessReply(commandId, "ok", LF_Stores.HarnessTakeFromShelf(this));
			return;
		}

		if (tool == "crate")
		{
			// Chain goods (#419): one contraband crate, what the moonshine still makes.
			int crates = LF_InventoryUtils.AddAmount(this, LF_Contraband.ITEM, 1);
			LF_HarnessReply(commandId, "ok", string.Format("crate added=%1", crates));
			return;
		}

		if (tool == "drill" || tool == "keycard" || tool == "charge")
		{
			// Stands in for a fence sale: the drill or keycard is stamped and booked to this account.
			ResourceName toolPrefab = LF_Bank.DRILL_PREFAB;
			if (tool == "keycard")
				toolPrefab = LF_Bank.KEYCARD_PREFAB;
			if (tool == "charge")
				toolPrefab = LF_Breach.FENCE_CHARGE_PREFAB;

			int drills = LF_InventoryUtils.AddAmount(this, toolPrefab, 1);
			InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(this);
			string serial = "";
			if (drills > 0 && storage)
			{
				array<IEntity> found = LF_InventoryUtils.FindItemsByPrefab(storage, toolPrefab);
				LF_SerialComponent stamp;
				if (!found.IsEmpty())
					stamp = LF_SerialComponent.Cast(found[0].FindComponent(LF_SerialComponent));

				if (stamp)
				{
					serial = LF_FenceSerials.Issue(LF_Utils.GetPlayerUid(this), toolPrefab);
					stamp.SetSerial(serial);
				}
			}

			LF_HarnessReply(commandId, "ok", string.Format("%1 added=%2 serial=%3", tool, drills, serial));
			return;
		}

		if (tool == "lockpick")
		{
			// Stands in for a fence sale: five uses of pick, no record needed.
			int picks = LF_InventoryUtils.AddAmount(this, LF_Lockpick.PICK_PREFAB, 5);
			LF_HarnessReply(commandId, "ok", string.Format("lockpick added=%1", picks));
			return;
		}

		int added = LF_InventoryUtils.AddAmount(this, LF_Scanner.PREFAB, 1);
		LF_HarnessReply(commandId, "ok", string.Format("scanner added=%1", added));
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

		int downed = 0;
		if (LF_Downed.IsDowned(target))
			downed = 1;

		LF_Debug.Info("AgentHarness", string.Format("shot player %1: refused=%2 alive=%3 downed=%4", targetPlayerId, refused, alive, downed));
		LF_HarnessReply(commandId, "ok", string.Format("shot player=%1 refused=%2 alive=%3 downed=%4", targetPlayerId, refused, alive, downed));
	}

	//------------------------------------------------------------------------------------------------
	//! Harness `siren`: set a vehicle's siren mode by entity name, behind the dev gate.
	void LF_AskHarnessSiren(int commandId, string entityName, int mode)
	{
		if (Replication.IsServer())
		{
			LF_ApplyHarnessSiren(commandId, entityName, mode, true);
			return;
		}

		Rpc(RpcAsk_LF_HarnessSiren, commandId, entityName, mode);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessSiren(int commandId, string entityName, int mode)
	{
		bool gateOpen = LF_DevGate.CheckCharacterRequest(this, "AgentHarness", "siren");
		LF_ApplyHarnessSiren(commandId, entityName, mode, gateOpen);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ApplyHarnessSiren(int commandId, string entityName, int mode, bool gateOpen)
	{
		if (!gateOpen)
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		IEntity vehicle = GetGame().GetWorld().FindEntityByName(entityName);
		LF_SirenManagerComponent siren;
		if (vehicle)
			siren = LF_SirenManagerComponent.Cast(vehicle.FindComponent(LF_SirenManagerComponent));

		if (!siren)
		{
			LF_HarnessReply(commandId, "error", "no_siren");
			return;
		}

		// This character stands in for the driver, so the on-duty rule of the driver's key applies (#464).
		bool changed = siren.RequestBy(this, mode, GetGame().GetWorld().GetWorldTime());
		int on = 0;
		if (siren.IsOn())
			on = 1;

		LF_HarnessReply(commandId, "ok", string.Format("siren changed=%1 on=%2 broadcasts=%3", changed, on, siren.GetBroadcastCount()));
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
	//! Harness `time <hour>`: the server sets the world clock so the LOCAL hour (game clock plus the
	//! world's longitude offset) is `hour`, and forces clear weather. A clock set on a client is
	//! overridden by the server, so this is an authority-side request, behind the dev gate.
	void LF_AskHarnessTime(int commandId, float hour)
	{
		if (Replication.IsServer())
		{
			LF_ApplyHarnessTime(commandId, hour);
			return;
		}

		Rpc(RpcAsk_LF_HarnessTime, commandId, hour);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessTime(int commandId, float hour)
	{
		if (!LF_DevGate.CheckHarnessRequest(this, "time"))
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		LF_ApplyHarnessTime(commandId, hour);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ApplyHarnessTime(int commandId, float hour)
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		TimeAndWeatherManagerEntity clock;
		if (world)
			clock = world.GetTimeAndWeatherManager();

		if (!clock)
		{
			LF_Debug.Error("AgentHarness", "time: the world has no time and weather manager");
			LF_HarnessReply(commandId, "error", "no_clock");
			return;
		}

		// The sun follows the world's longitude: local solar time is the game clock plus longitude / 15 h.
		float offset = clock.GetTimeZoneOffset() + clock.GetCurrentLongitude() / 15;
		float game = hour - offset;
		while (game < 0)
			game += 24;
		while (game >= 24)
			game -= 24;

		clock.SetTimeOfTheDay(game, true);
		clock.ForceWeatherTo(false, "Clear", 0, 3600);
		string text = string.Format("local=%1 game=%2 offset=%3 now=%4 day_s=%5", hour, game, offset, clock.GetTimeOfTheDay(), clock.GetDayDuration());
		LF_Debug.Info("AgentHarness", "clock set: " + text);
		LF_HarnessReply(commandId, "ok", text);
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
