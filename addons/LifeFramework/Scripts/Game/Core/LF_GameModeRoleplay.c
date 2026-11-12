[EntityEditorProps(category: "EveronLife/Core", description: "Core gamemode")]
class LF_GameModeRoleplayClass: SCR_BaseGameModeClass
{
}

class LF_GameModeRoleplay: SCR_BaseGameMode
{
	//! Server opt-in for dev tools (debug menu, agent-harness RPCs). Off on the production prefab;
	//! on in GameMode_Roleplay_Debug.et (DebugWorld). Administrators pass regardless - see LF_DevGate.
	[Attribute("0", desc: "Let every player use dev tools (debug menu, agent harness). Development worlds only; administrators are always allowed.")]
	protected bool m_bDevToolsEnabled;

	//! Test worlds: the convoy drop is the cage a short drive from the barn (Roadmap_Convoy_Drop_Near)
	//! instead of the pub on the square (Roadmap_Convoy_Drop), so a run can be tried on the spot.
	//! The flagship prefab leaves it off and keeps the real distance.
	[Attribute("0", desc: "Convoy drop is the near cage by the barn, not the pub on the square. Test worlds only.")]
	protected bool m_bConvoyNearDrop;

	//! The DebugWorld open-key listener is registered once per local player spawn so the menu
	//! is reachable in a dev session without a dedicated UI component on the prefab.
	protected bool m_bDebugMenuListenerRegistered;

	//! Connected on-duty police, written by the server (LF_AccountReplication) so client-side
	//! robbery offers can apply the minimum-police gate. The game mode is always replicated.
	[RplProp()]
	protected int m_iLF_PoliceOnDuty;

	protected bool m_bLF_AccountRefreshRunning;

	//------------------------------------------------------------------------------------------------
	//! \return true when this game mode opts every player into dev tools and the mission header
	//! (LF_MissionHeader, e.g. the playtest mission) does not close them.
	bool AreDevToolsEnabled()
	{
		return LF_MissionHeader.ResolveDevToolsEnabled(m_bDevToolsEnabled, GetGame().GetMissionHeader());
	}

	//------------------------------------------------------------------------------------------------
	bool IsConvoyNearDrop()
	{
		return m_bConvoyNearDrop;
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		// Owner config.json over the shipped .conf defaults (server); features read through it.
		LF_ConfigService.Boot();
		// The effective client-relevant values, for the config stream to joining clients (#285).
		if (Replication.IsServer())
			LF_ConfigStream.Publish();
		LF_HostStop.StartIfRequested();
		LF_IncidentLog.Install();
		LF_AimIntent.Start();
		LF_DevGameMaster.Install(AreDevToolsEnabled());
		LF_ServiceCallDispatch.Install();
		LF_ServiceBoard.Install();
		LF_ChainTrace.Install();
		// The replicated DELIVERY level follows every job level change (server only, #749).
		LF_JobLevelSync.EnsureSubscribed();
		// Interaction menu role slots, on every machine (offer on the client, decide on the server).
		LF_OrgInviteRole.Register();
		LF_HoldUpRole.Register();
		LF_WitnessRole.Register();
		// The money ledger listens for every cash and bank change (server only).
		LF_Ledger.EnsureSubscribed();
		// The hidden crime record counts every completed crime (server only).
		LF_CrimeRecord.EnsureSubscribed();
		// The serial book stamps every tool the fence sells (server only).
		LF_FenceSerials.EnsureSubscribed();
		// The wanted board's posters rebuild on a crime, a wanted level, an identification (server only).
		LF_WantedPosters.Install();
		// Playtime per account and job, credited to online players (server only, #692).
		LF_Playtime.Start();
	}

	//------------------------------------------------------------------------------------------------
	void ~LF_GameModeRoleplay()
	{
		LF_PlayerAccountManager.Reset();
		// World end: drop process-global feature state so a reload starts clean (see LF_StaticReset).
		LF_StaticReset.ResetAll();
		UnregisterDebugMenuListener();
		if (m_bLF_AccountRefreshRunning && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(LF_RefreshAccountReplication);
		if (m_bLF_AccountRefreshRunning && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(LF_Perf.Report);
	}

	//------------------------------------------------------------------------------------------------
	//! JIP: the config stream rides after the base game mode's state (#285). Positional: RplLoad
	//! reads exactly what this writes, in this order.
	override bool RplSave(ScriptBitWriter writer)
	{
		if (!super.RplSave(writer))
			return false;

		LF_ConfigStream.EnsurePublished();
		writer.WriteInt(LF_ConfigStream.FORMAT_VERSION);
		writer.WriteInt(LF_ConfigStream.GetPublishedHash());
		writer.WriteString(LF_ConfigStream.GetPublishedPayload());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		if (!super.RplLoad(reader))
			return false;

		int format;
		int hash;
		string payload;
		if (!reader.ReadInt(format) || !reader.ReadInt(hash) || !reader.ReadString(payload))
		{
			LF_Debug.Error(LF_ConfigService.FEATURE, "stream: the join snapshot carries no config stream; the client keeps the shipped config values");
			return false;
		}

		LF_ConfigStream.OnArrived(format, hash, payload);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rebuilds the config stream and sends it to every connected client when it changed.
	//! Joining clients get it through RplSave.
	void LF_PublishConfigStream()
	{
		if (!Replication.IsServer())
			return;

		bool first = !LF_ConfigStream.IsPublished();
		if (!LF_ConfigStream.Publish() || first)
			return;

		Rpc(RpcDo_LF_ConfigStream, LF_ConfigStream.FORMAT_VERSION, LF_ConfigStream.GetPublishedHash(), LF_ConfigStream.GetPublishedPayload());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_ConfigStream(int format, int hash, string payload)
	{
		LF_ConfigStream.OnArrived(format, hash, payload);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: bumps only when the count changes.
	void LF_SetPoliceOnDuty(int count)
	{
		if (!Replication.IsServer() || count == m_iLF_PoliceOnDuty)
			return;

		m_iLF_PoliceOnDuty = count;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	int LF_GetPoliceOnDuty()
	{
		return m_iLF_PoliceOnDuty;
	}

	//------------------------------------------------------------------------------------------------
	//! Releases per-player server state the player can no longer act on: escort holds where they
	//! are officer or prisoner, and a running convoy (fails it and drops the vehicle watch).
	//! Jail sentences are kept on purpose: they persist and resume on reconnect. The carried
	//! inventory is captured before the base game deletes the character.
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (Replication.IsServer())
		{
			LF_BleedOut.OnPlayerDisconnected(playerId);
			LF_PhoneCalls.OnPlayerDisconnected(playerId);
			ReleaseDisconnectedPlayer(playerId);
		}

		super.OnPlayerDisconnected(playerId, cause, timeout);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReleaseDisconnectedPlayer(int playerId)
	{
		string uid = LF_Utils.GetPlayerUID(playerId);
		IEntity controlled;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
			controlled = playerManager.GetPlayerControlledEntity(playerId);

		if (uid.IsEmpty() && !controlled)
		{
			LF_Debug.Warn("Session", string.Format("disconnect cleanup skipped: player %1 has no uid and no entity", playerId));
			return;
		}

		int escorts = LF_JailEscort.ReleasePlayer(uid);

		if (LF_StorageSnapshot.CapturePlayer(playerId))
			LF_Debug.Info(LF_StorageSnapshot.FEATURE, string.Format("carried inventory captured on disconnect for player %1", playerId));

		// Unpaid store goods leave with the snapshot; their tags wait for the next spawn (#479).
		LF_Stores.ParkCarrier(uid, controlled);

		string characterId;
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts && !uid.IsEmpty())
		{
			LF_PlayerAccount account = accounts.GetAccount(uid);
			if (account && account.GetActiveCharacter())
				characterId = account.GetActiveCharacter().GetId();
		}

		LF_Contraband.ReleasePlayer(characterId, controlled);
		LF_Debug.Log("Session", string.Format("disconnect cleanup player=%1 escorts=%2", playerId, escorts));
	}

	//------------------------------------------------------------------------------------------------
	//! Every character death, players and NPCs. Murder is decided here (LF_Murder).
	protected override void OnControllableDestroyedEx(notnull SCR_InstigatorContextData instigatorContextData)
	{
		super.OnControllableDestroyedEx(instigatorContextData);

		if (!Replication.IsServer())
			return;

		IEntity victim = instigatorContextData.GetVictimEntity();
		int victimPlayerId;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager && victim)
			victimPlayerId = playerManager.GetPlayerIdFromControlledEntity(victim);

		LF_Murder.OnCharacterKilled(victim, instigatorContextData.GetKillerEntity(), victimPlayerId, instigatorContextData.GetKillerPlayerID());
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);

		LF_AccountReplication.PublishForPlayer(playerId, controlledEntity);

		LF_WhitelistRespawnHandlerComponent whitelist = LF_WhitelistRespawnHandlerComponent.Cast(FindComponent(LF_WhitelistRespawnHandlerComponent));
		if (whitelist)
			whitelist.OnPlayerSpawned(playerId, controlledEntity);

		RegisterDebugMenuListener(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Client-side debug menu open. Registered once per local player spawn (the listener is
	//! process-global, so a second spawn must not double-register). On a dedicated server there
	//! is no local input, so the listener never fires there. The key itself is gated at press time
	//! (OnOpenDebugMenu) because a player's admin role can change mid-session; the server-side
	//! LF_DevGate check in RpcAsk_LF_DebugAction is the real authority.
	protected void RegisterDebugMenuListener(int playerId)
	{
		if (m_bDebugMenuListenerRegistered)
			return;

		PlayerController local = GetGame().GetPlayerController();
		if (!local || local.GetPlayerId() != playerId)
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		input.AddActionListener("LF_OpenDebugMenu", EActionTrigger.DOWN, OnOpenDebugMenu);
		m_bDebugMenuListenerRegistered = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void UnregisterDebugMenuListener()
	{
		if (!m_bDebugMenuListenerRegistered)
			return;

		InputManager input = GetGame().GetInputManager();
		if (input)
			input.RemoveActionListener("LF_OpenDebugMenu", EActionTrigger.DOWN, OnOpenDebugMenu);

		m_bDebugMenuListenerRegistered = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOpenDebugMenu()
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		if (menuManager.FindMenuByPreset(ChimeraMenuPreset.DebugMenu))
		{
			menuManager.CloseMenuByPreset(ChimeraMenuPreset.DebugMenu);
			return;
		}

		// Cosmetic client gate on the key: the server refuses the actions anyway.
		PlayerController local = GetGame().GetPlayerController();
		int localPlayerId = 0;
		if (local)
			localPlayerId = local.GetPlayerId();
		if (!LF_DevGate.IsAllowedForPlayer(localPlayerId))
		{
			LF_Debug.Warn("DebugMenu", "open refused: dev tools are disabled in this game mode and the player is not an administrator");
			return;
		}

		menuManager.OpenMenu(ChimeraMenuPreset.DebugMenu);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);

		LF_CharacterCreationManager creation = ResolveConnectManagers();
		creation.OnPlayerConnected(playerId);

		// Self-healing account replication pass (server only, started once).
		if (Replication.IsServer() && !m_bLF_AccountRefreshRunning)
		{
			GetGame().GetCallqueue().CallLater(LF_RefreshAccountReplication, LF_AccountReplication.REFRESH_INTERVAL_MS, true);
			m_bLF_AccountRefreshRunning = true;

			// -lfPerf: print the server cost counters once a minute (LF_Perf).
			if (LF_Perf.IsEnabled())
				GetGame().GetCallqueue().CallLater(LF_Perf.Report, 60000, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_RefreshAccountReplication()
	{
		LF_AccountReplication.RefreshAll();
	}

	//------------------------------------------------------------------------------------------------
	//! The manager half of OnPlayerConnected. Every manager here is a self-owned singleton, so
	//! this only resolves them through GetInstance() and never constructs one. A `new` here
	//! replaced the bank cache the save load had already filled, which wiped every balance on
	//! the first connect after a restart. Public and static so a LOGIC test drives the same path.
	//! \return The character creation manager the connecting player's flow runs on.
	static LF_CharacterCreationManager ResolveConnectManagers()
	{
		LF_ATMManager.GetInstance();
		LF_JobManager.GetInstance();
		return LF_CharacterCreationManager.GetInstance();
	}
}
