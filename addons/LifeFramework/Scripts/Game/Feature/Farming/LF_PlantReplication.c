//------------------------------------------------------------------------------------------------
//! One planted crop as a client needs it to draw it (#2819): id, crop, position and stage. The
//! clocks and the owner key stay on the server.
class LF_PlantViewRow : Managed
{
	string m_sPlantId;
	string m_sCropId;
	vector m_vPosition;
	int m_iStage;

	//------------------------------------------------------------------------------------------------
	LF_PlantViewRow Copy()
	{
		LF_PlantViewRow copy = new LF_PlantViewRow();
		copy.m_sPlantId = m_sPlantId;
		copy.m_sCropId = m_sCropId;
		copy.m_vPosition = m_vPosition;
		copy.m_iStage = m_iStage;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! Planted crops reach every client (#2819). LF_Plants rows live on the server and its meshes are
//! local copies, so a dedicated server's clients saw nothing. The server keeps a carrier table of what
//! it last told clients (id, crop, position, stage): a plant, a stage change and a removal are each one
//! broadcast RPC on the game mode (no per-tick traffic: LF_Plants calls Publish on its tick and only a
//! changed stage sends), and a late joiner gets the whole table in the game mode's RplSave/RplLoad.
//! A client keeps a mirror of the table and draws one local mesh per row. A listen host or Workbench
//! draws through LF_Plants itself, so this mirror stays empty there (a broadcast never runs on its
//! sender, and RplLoad never runs on the server).
class LF_PlantReplication
{
	static const string FEATURE = "Farming";
	//! RplSave/RplLoad layout; bump it when the fields change.
	static const int FORMAT_VERSION = 1;
	static const int MAX_ROWS = 1024;	// lf-fixed: a sanity cap on a corrupt count, far above the 300 plant server cap

	protected static ref map<string, ref LF_PlantViewRow> s_mTable;
	protected static ref map<string, ref LF_PlantViewRow> s_mMirror;
	protected static ref map<string, EntityID> s_mClientVisuals;
	protected static bool s_bNoCarrierLogged;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_PlantViewRow> Table()
	{
		if (!s_mTable)
			s_mTable = new map<string, ref LF_PlantViewRow>();

		return s_mTable;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_PlantViewRow> Mirror()
	{
		if (!s_mMirror)
			s_mMirror = new map<string, ref LF_PlantViewRow>();

		return s_mMirror;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, EntityID> ClientVisuals()
	{
		if (!s_mClientVisuals)
			s_mClientVisuals = new map<string, EntityID>();

		return s_mClientVisuals;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		ResetClient();
		s_mTable = null;
		s_bNoCarrierLogged = false;
	}

	//------------------------------------------------------------------------------------------------
	//! The game mode that carries the RPC and the join snapshot; null when the mode is not ours.
	protected static LF_GameModeRoleplay Carrier()
	{
		LF_GameModeRoleplay carrier = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (!carrier && !s_bNoCarrierLogged)
		{
			s_bNoCarrierLogged = true;
			LF_Debug.Warn(FEATURE, "plant replication: the game mode is not LF_GameModeRoleplay, clients are not told about plants");
		}

		return carrier;
	}

	//------------------------------------------------------------------------------------------------
	// Server
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: the plant's stage as clients should draw it, or -1 when it has nothing to draw.
	protected static int StageFor(LF_PlantRow row, out LF_CropRow crop)
	{
		crop = LF_Plants.FindCrop(row.m_sCropId);
		if (!crop || !crop.m_aStagePrefabs || crop.m_aStagePrefabs.IsEmpty())
			return -1;

		int stage = LF_PlantRules.StageOf(crop, LF_Plants.StateOf(row, crop));
		if (stage < 0)
			return -1;

		if (stage >= crop.m_aStagePrefabs.Count())
			stage = crop.m_aStagePrefabs.Count() - 1;

		return stage;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tells clients about a plant when it is new or its stage changed; cheap when nothing did.
	static void Publish(LF_PlantRow row)
	{
		if (!row || !Replication.IsServer())
			return;

		LF_CropRow crop;
		int stage = StageFor(row, crop);
		if (stage < 0)
		{
			Retract(row.m_sPlantId);
			return;
		}

		LF_PlantViewRow sent = Table().Get(row.m_sPlantId);
		if (sent && sent.m_iStage == stage)
			return;

		LF_PlantViewRow view = new LF_PlantViewRow();
		view.m_sPlantId = row.m_sPlantId;
		view.m_sCropId = row.m_sCropId;
		view.m_vPosition = row.m_vPosition;
		view.m_iStage = stage;
		Table().Set(view.m_sPlantId, view);
		LF_Debug.Info(FEATURE, string.Format("plant replication: %1 (%2) is at stage %3, told to clients", view.m_sPlantId, view.m_sCropId, stage));
		LF_GameModeRoleplay carrier = Carrier();
		if (carrier)
			carrier.LF_PlantBroadcastUpsert(view.m_sPlantId, view.m_sCropId, view.m_vPosition, view.m_iStage);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a plant is gone (harvested, cleared); clients delete its mesh.
	static void Retract(string plantId)
	{
		if (!Replication.IsServer() || !Table().Contains(plantId))
			return;

		Table().Remove(plantId);
		LF_Debug.Info(FEATURE, string.Format("plant replication: %1 removed, told to clients", plantId));
		LF_GameModeRoleplay carrier = Carrier();
		if (carrier)
			carrier.LF_PlantBroadcastRemove(plantId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every plant is gone at once (the saved plants are being applied over the live ones).
	static void RetractAll()
	{
		if (!Replication.IsServer() || Table().IsEmpty())
			return;

		Table().Clear();
		LF_Debug.Info(FEATURE, "plant replication: table cleared, told to clients");
		LF_GameModeRoleplay carrier = Carrier();
		if (carrier)
			carrier.LF_PlantBroadcastClear();
	}

	//------------------------------------------------------------------------------------------------
	//! What a late joiner is sent: a copy of every row in the table.
	static array<ref LF_PlantViewRow> Collect()
	{
		array<ref LF_PlantViewRow> rows = new array<ref LF_PlantViewRow>();
		foreach (string id, LF_PlantViewRow row : Table())
			rows.Insert(row.Copy());

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! What this machine shows, for the harness: `rows=N meshes=M <id>:<stage> ...`, sorted by id. A
	//! dedicated client reads its mirror; the server (or a listen host) reads the table it sends.
	static string DescribeLocal()
	{
		map<string, ref LF_PlantViewRow> rows = Mirror();
		string side = "client";
		if (Replication.IsServer())
		{
			rows = Table();
			side = "server";
		}

		array<string> ids = new array<string>();
		foreach (string id, LF_PlantViewRow row : rows)
			ids.Insert(id);

		ids.Sort();
		string text = string.Format("side=%1 rows=%2 meshes=%3", side, ids.Count(), ClientVisuals().Count());
		foreach (string id : ids)
			text += string.Format(" %1:%2", id, rows.Get(id).m_iStage);	// lf-bounded: the proof plants one or two crops

		return text;
	}

	//------------------------------------------------------------------------------------------------
	static void Write(notnull ScriptBitWriter writer, notnull array<ref LF_PlantViewRow> rows)
	{
		writer.WriteInt(FORMAT_VERSION);
		writer.WriteInt(rows.Count());
		foreach (LF_PlantViewRow row : rows)
		{
			writer.WriteString(row.m_sPlantId);
			writer.WriteString(row.m_sCropId);
			writer.WriteVector(row.m_vPosition);
			writer.WriteInt(row.m_iStage);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return false when the stream is short or corrupt (the snapshot is then refused). A format this
	//! build does not know is read to keep the stream aligned and gives no rows.
	static bool Read(notnull ScriptBitReader reader, notnull array<ref LF_PlantViewRow> rows)
	{
		int version;
		int count;
		if (!reader.ReadInt(version) || !reader.ReadInt(count))
			return false;

		if (count < 0 || count > MAX_ROWS)
		{
			LF_Debug.Error(FEATURE, string.Format("plant replication: the snapshot claims %1 plants; refused", count));
			return false;
		}

		for (int i = 0; i < count; i++)
		{
			LF_PlantViewRow row = new LF_PlantViewRow();
			if (!reader.ReadString(row.m_sPlantId) || !reader.ReadString(row.m_sCropId) || !reader.ReadVector(row.m_vPosition) || !reader.ReadInt(row.m_iStage))
				return false;

			rows.Insert(row);
		}

		if (version != FORMAT_VERSION)
		{
			LF_Debug.Error(FEATURE, string.Format("plant replication: plants dropped: the server sends format %1, this build reads %2", version, FORMAT_VERSION));
			rows.Clear();
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Client
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Client: forget every row and delete every mesh.
	static void ResetClient()
	{
		ClearAllClientVisuals();
		s_mMirror = null;
	}

	//------------------------------------------------------------------------------------------------
	static int ClientCount()
	{
		return Mirror().Count();
	}

	//------------------------------------------------------------------------------------------------
	static int ClientVisualCount()
	{
		return ClientVisuals().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The stage the client shows for a plant, or -1.
	static int ClientStageOf(string plantId)
	{
		LF_PlantViewRow row = Mirror().Get(plantId);
		if (!row)
			return -1;

		return row.m_iStage;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the plant whose stage mesh is this entity, or empty (#3177, the stage prefab's actions).
	static string ClientPlantOf(EntityID visual)
	{
		foreach (string id, EntityID drawn : ClientVisuals())
		{
			if (drawn == visual)
				return id;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the whole table replaces the mirror (a join snapshot, or the server cleared).
	//! \return how many plants were kept.
	static int ClientApplyAll(array<ref LF_PlantViewRow> rows)
	{
		ResetClient();
		if (!rows)
			return 0;

		foreach (LF_PlantViewRow row : rows)
			ClientUpsert(row);

		return Mirror().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one plant is new or at a new stage. A crop this client does not know, or a stage with
	//! no prefab, is logged and left out.
	static void ClientUpsert(LF_PlantViewRow row)
	{
		if (!row || row.m_sPlantId.IsEmpty())
			return;

		LF_CropRow crop = LF_Plants.FindCrop(row.m_sCropId);
		if (!crop || !crop.m_aStagePrefabs || crop.m_aStagePrefabs.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("plant replication: %1 grows %2, which this client's crop catalogue does not draw", row.m_sPlantId, row.m_sCropId));
			ClientRemove(row.m_sPlantId);
			return;
		}

		LF_PlantViewRow copy = row.Copy();
		copy.m_iStage = LF_Utils.ClampInt(copy.m_iStage, 0, crop.m_aStagePrefabs.Count() - 1);
		LF_PlantViewRow shown = Mirror().Get(copy.m_sPlantId);
		bool drawn = ClientVisuals().Contains(copy.m_sPlantId);
		Mirror().Set(copy.m_sPlantId, copy);
		if (shown && drawn && shown.m_iStage == copy.m_iStage && vector.Distance(shown.m_vPosition, copy.m_vPosition) < 0.01)
			return;

		LF_Debug.Info(FEATURE, string.Format("plant replication: this client shows %1 (%2) at stage %3", copy.m_sPlantId, copy.m_sCropId, copy.m_iStage));
		DrawClient(copy, crop);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one plant is gone.
	static void ClientRemove(string plantId)
	{
		ClearClientVisual(plantId);
		if (s_mMirror)
			s_mMirror.Remove(plantId);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DrawClient(LF_PlantViewRow row, LF_CropRow crop)
	{
		ClearClientVisual(row.m_sPlantId);
		Resource resource = Resource.Load(crop.m_aStagePrefabs[row.m_iStage]);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error(FEATURE, string.Format("plant replication: %1: stage prefab does not load: %2", row.m_sPlantId, crop.m_aStagePrefabs[row.m_iStage]));
			return;
		}

		ArmaReforgerScripted game = GetGame();
		if (!game || !game.GetWorld())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = row.m_vPosition;
		IEntity visual = game.SpawnEntityPrefabLocal(resource, game.GetWorld(), params);
		if (!visual)
		{
			LF_Debug.Error(FEATURE, string.Format("plant replication: %1: stage prefab failed to spawn", row.m_sPlantId));
			return;
		}

		ClientVisuals().Set(row.m_sPlantId, visual.GetID());
	}

	//------------------------------------------------------------------------------------------------
	protected static void ClearClientVisual(string plantId)
	{
		if (!s_mClientVisuals)
			return;

		EntityID id;
		if (!s_mClientVisuals.Find(plantId, id))
			return;

		s_mClientVisuals.Remove(plantId);
		ArmaReforgerScripted game = GetGame();
		if (!game || !game.GetWorld())
			return;

		IEntity visual = game.GetWorld().FindEntityByID(id);
		if (visual)
			SCR_EntityHelper.DeleteEntityAndChildren(visual);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ClearAllClientVisuals()
	{
		if (!s_mClientVisuals)
			return;

		array<string> ids = new array<string>();
		foreach (string id, EntityID entityId : s_mClientVisuals)
			ids.Insert(id);

		foreach (string id : ids)
			ClearClientVisual(id);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PlantReplication's statics (LF_StaticReset finds it by reflection).
class LF_PlantReplication_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PlantReplication";
	}

	override void Apply()
	{
		LF_PlantReplication.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The game mode is the replicated owner that carries planted crops: three broadcast RPCs for the
//! changes and, after whatever the chain below writes, the whole table for a late joiner (positional:
//! RplLoad reads exactly what RplSave writes, in this order, after super).
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	//! Server: a plant is new or at a new stage.
	void LF_PlantBroadcastUpsert(string plantId, string cropId, vector position, int stage)
	{
		Rpc(RpcDo_LF_PlantUpsert, plantId, cropId, position, stage);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a plant is gone.
	void LF_PlantBroadcastRemove(string plantId)
	{
		Rpc(RpcDo_LF_PlantRemove, plantId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every plant is gone.
	void LF_PlantBroadcastClear()
	{
		Rpc(RpcDo_LF_PlantClear);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_PlantUpsert(string plantId, string cropId, vector position, int stage)
	{
		LF_PlantViewRow row = new LF_PlantViewRow();
		row.m_sPlantId = plantId;
		row.m_sCropId = cropId;
		row.m_vPosition = position;
		row.m_iStage = stage;
		LF_PlantReplication.ClientUpsert(row);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_PlantRemove(string plantId)
	{
		LF_PlantReplication.ClientRemove(plantId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_PlantClear()
	{
		LF_Debug.Info(LF_PlantReplication.FEATURE, string.Format("plant replication: the server cleared every plant; this client drops %1", LF_PlantReplication.ClientCount()));
		LF_PlantReplication.ResetClient();
	}

	//------------------------------------------------------------------------------------------------
	override bool RplSave(ScriptBitWriter writer)
	{
		if (!super.RplSave(writer))
			return false;

		array<ref LF_PlantViewRow> rows = LF_PlantReplication.Collect();
		LF_PlantReplication.Write(writer, rows);
		if (!rows.IsEmpty())
			LF_Debug.Info(LF_PlantReplication.FEATURE, string.Format("plant replication: join snapshot carries %1 plant(s)", rows.Count()));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		if (!super.RplLoad(reader))
			return false;

		array<ref LF_PlantViewRow> rows = new array<ref LF_PlantViewRow>();
		if (!LF_PlantReplication.Read(reader, rows))
		{
			LF_Debug.Error(LF_PlantReplication.FEATURE, "plant replication: the join snapshot carries no plant table; plants appear as they change");
			return false;
		}

		int applied = LF_PlantReplication.ClientApplyAll(rows);
		LF_Debug.Info(LF_PlantReplication.FEATURE, string.Format("plant replication: join snapshot drew %1 of %2 plant(s)", applied, rows.Count()));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Harness verbs for plant-late-proof (#3178):
//!   plantsync state                 what this machine shows (DescribeLocal)
//!   plantsync plant [cropId]        the server gives this character one seed and plants it 2 m ahead
//!   plantsync grow                  the server's farming clock moves on one stage of that crop (default cabbage)
//!   plantsync reset                 the server runs the whole world reset (#2820)
//! A client asks the server through its character (the harness gate allows it on a dev build only).
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "plantsync")
			return super.Handle(cmd);

		if (cmd.Arg1 == "state")
		{
			LF_AgentHarness.Answer(cmd.Id, "ok", LF_PlantReplication.DescribeLocal());
			return true;
		}

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if ((cmd.Arg1 != "plant" && cmd.Arg1 != "grow" && cmd.Arg1 != "reset") || !character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: plantsync state | plant [cropId] | grow | reset (plant, grow and reset need a character)");
			return true;
		}

		character.LF_AskHarnessPlantSync(cmd.Id, cmd.Arg1, cmd.Arg2);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskHarnessPlantSync(int commandId, string action, string cropId)
	{
		if (Replication.IsServer())
		{
			LF_HarnessPlantSyncDo(commandId, action, cropId);
			return;
		}

		Rpc(RpcAsk_LF_HarnessPlantSync, commandId, action, cropId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessPlantSync(int commandId, string action, string cropId)
	{
		if (!LF_DevGate.CheckHarnessRequest(this, "plantsync"))
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		LF_HarnessPlantSyncDo(commandId, action, cropId);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_HarnessPlantSyncDo(int commandId, string action, string cropId)
	{
		if (action == "reset")
		{
			LF_HarnessReply(commandId, "ok", LF_WorldReset.RunAll("harness plantsync") + " | " + LF_PlantReplication.DescribeLocal());
			return;
		}

		if (cropId.IsEmpty())
			cropId = "cabbage";

		if (action == "grow")
		{
			LF_CropRow grown = LF_Plants.FindCrop(cropId);
			if (!grown)
			{
				LF_HarnessReply(commandId, "error", "plantsync grow: no crop " + cropId + " in the loaded catalogue");
				return;
			}

			int stages = LF_PlantRules.BuildStages(grown).Count();
			float seconds = grown.m_iGrowthSeconds / (stages - 1) + 1;	// lf-float: one stage of farming-clock seconds
			LF_Plants.AdvanceClock(seconds);
			LF_HarnessReply(commandId, "ok", string.Format("advanced %1 s | %2", seconds, LF_PlantReplication.DescribeLocal()));
			return;
		}

		LF_CropRow crop = LF_Plants.FindCrop(cropId);
		if (!crop)
		{
			LF_HarnessReply(commandId, "error", "plantsync plant: no crop " + cropId + " in the loaded catalogue");
			return;
		}

		LF_InventoryUtils.AddAmount(this, crop.m_SeedPrefab, 1, true);
		vector mat[4];
		GetTransform(mat);
		vector spot = GetOrigin() + mat[2] * 2;
		string plantId;
		LF_EPlantPlace place;
		LF_EPlantResult result = LF_Plants.Plant(this, crop.m_SeedPrefab, spot, plantId, place);
		if (result != LF_EPlantResult.OK)
		{
			LF_HarnessReply(commandId, "error", string.Format("plantsync plant refused: %1 %2", typename.EnumToString(LF_EPlantResult, result), typename.EnumToString(LF_EPlantPlace, place)));
			return;
		}

		LF_HarnessReply(commandId, "ok", string.Format("planted=%1 | %2", plantId, LF_PlantReplication.DescribeLocal()));
	}
}
