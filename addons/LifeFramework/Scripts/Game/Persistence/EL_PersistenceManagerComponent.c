//------------------------------------------------------------------------------------------------
//! Server-side save orchestration: the "what triggers a save" half of persistence.
//!
//! The serializers bound in Configs/Systems/Persistence/LifeFramework.conf write the
//! account, bank, survival and quantity data; this component makes those writes actually
//! happen on a schedule, on shutdown, on demand, and exposes an honest save-finished
//! signal. Attached to the game-mode entity (GameMode_Roleplay.et).
//!
//! TWO ENGINE LAYERS SIT BEHIND THIS, do not conflate them:
//!  1. SCR_PersistenceSystem - server-only world system that tracks instances and
//!     serializes them. Configured by LifeFramework.conf, registered by
//!     ChimeraSystemsConfig.conf. Has no global save; its Save() takes one instance.
//!  2. SaveGameManager - the engine singleton that owns save POINTS (create / list /
//!     load / delete). Every save request in this component goes through it.
//!
//! SAVE TYPES. ESaveGameType is a BITMASK, not an ordinal: membership is tested with
//! (type & X) != 0, never ==. The autosave overwrite selection lives in EL_SaveSelection
//! (pure, tested) so that lesson is owned by a test.
//!
//! AUTOSAVE BOUNDING. GetSaves() returns every save for the mission across all
//! playthroughs. The autosave overwrites the latest AUTO save of the CURRENT
//! playthrough, else creates one, so a fresh campaign never scribbles over the previous
//! one.
//!
//! SHUTDOWN SAVE IS ONCE-PER-SESSION. The engine raises OnGameEnd twice on a dedicated
//! server shutdown (measured by the Overthrow mod, 2026-08-04). The once-only guard
//! makes the second pass a no-op instead of a rejected request after saving was
//! disabled.
//!
//! FAIL-SAFE. Every gate failure logs an EL_Debug message under the "Persistence"
//! feature and rejects that one request. A client, a Workbench editor world, or a
//! misconfigured server gets a greppable line, never a VME.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Persistence", description: "Server-side save orchestration over SCR_PersistenceSystem and SaveGameManager.")]
class EL_PersistenceManagerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_PersistenceManagerComponent : ScriptComponent
{
	[Attribute(defvalue: "1", desc: "Periodically save the campaign while it is running (server only).", category: "Auto-Save")]
	protected bool m_bEnableAutosave;

	[Attribute(defvalue: "600", desc: "Seconds between autosaves.", category: "Auto-Save")]
	protected float m_fAutosaveInterval;

	//! The vanilla persistence world system. Null on clients, and null on the server when
	//! the SCR_PersistenceSystem entry is missing from the systems config.
	protected SCR_PersistenceSystem m_PersistenceSystem;

	//! The game mode this component orchestrates for, for the OnGameStart / OnGameEnd
	//! invokers. Null before the game mode is available or after teardown.
	protected SCR_BaseGameMode m_GameMode;

	//! True once the repeating autosave timer has been scheduled (at most once per world).
	protected bool m_bAutosaveScheduled;

	//! True once the game mode reported its game started, so the shutdown save skips a
	//! session that never ran.
	protected bool m_bGameStarted;

	//! True once the shutdown save has been asked for. The engine can raise OnGameEnd
	//! twice per session; the second pass must not ask again.
	protected bool m_bShutdownSaveRequested;

	//! Cached answer to "does a save point exist for this mission?". Seeded by an async
	//! GetSaves() scan in OnPostInit and kept current by OnAfterSave, which fires for
	//! engine-initiated saves too. Between OnPostInit and the scan callback this is the
	//! boot-time default false.
	protected bool m_bHasSaveGame;

	//! True once m_bHasSaveGame reflects a real answer rather than the boot default.
	protected bool m_bSaveCacheSeeded;

	//! True between a save request made through this component and its completion callback.
	protected bool m_bSaveInProgress;

	//! True from LoadLatestSave() until the request has been handed to the engine or given
	//! up. A successful load transitions the game state, which tears this component down.
	protected bool m_bLoadInProgress;

	//! Why the last load request could not proceed. Empty while one is in flight.
	protected string m_sLastLoadDiagnostic;

	//! True from ReapplyLatestSaveData() until the persistence system reports its last result.
	protected bool m_bReapplyInProgress;

	//! Why the last re-application did not complete cleanly. Empty after a successful one.
	protected string m_sLastReapplyDiagnostic;

	//! How many save points requested through this component have completed successfully
	//! since the world loaded. The only honest "did MY save land?" answer, because
	//! IsSaveInProgress() cannot distinguish finished from never-started.
	protected int m_iCompletedSaves;

	//! How many instance re-applications have completed with OK since the world loaded.
	protected int m_iCompletedReapplies;

	//! Fired with (bool success) when a save requested through this component completes.
	protected ref ScriptInvoker m_OnSaveFinished = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	//! \return Invoker fired with (bool success) when a save requested through this
	//!         component ends. The honest save signal.
	ScriptInvoker GetOnSaveFinished()
	{
		return m_OnSaveFinished;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when a save point exists for the current mission (cached).
	bool HasSaveGame()
	{
		return m_bHasSaveGame;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True once HasSaveGame() is a real answer and no longer the boot default.
	bool IsSaveCacheSeeded()
	{
		return m_bSaveCacheSeeded;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while a save requested through this component is still in flight.
	bool IsSaveInProgress()
	{
		return m_bSaveInProgress;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Count of save points requested through this component that completed
	//!         successfully in this world. Read it before requesting a save and wait for
	//!         it to increase to know YOUR save landed.
	int GetCompletedSaveCount()
	{
		return m_iCompletedSaves;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while a load requested through LoadLatestSave() is still in flight.
	bool IsLoadInProgress()
	{
		return m_bLoadInProgress;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Why the last LoadLatestSave() could not proceed. Empty when a request is in
	//!         flight or none was made.
	string GetLastLoadDiagnostic()
	{
		return m_sLastLoadDiagnostic;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while a re-application requested through ReapplyLatestSaveData() is in flight.
	bool IsReapplyInProgress()
	{
		return m_bReapplyInProgress;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many instance re-applications have completed with OK in this world.
	int GetCompletedReapplyCount()
	{
		return m_iCompletedReapplies;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Why the last re-application did not complete cleanly. Non-empty exactly when
	//!         the most recent attempt failed and says how.
	string GetLastReapplyDiagnostic()
	{
		return m_sLastReapplyDiagnostic;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when THIS session was launched from a save point, as opposed to "some
	//!         save exists on disk". HasSaveGame() is deliberately not that signal: a save
	//!         can exist while the server started a brand new playthrough.
	bool IsPlayingLoadedSave()
	{
		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
			return false;

		return manager.GetActiveSave() != null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The live vanilla persistence system, or null when this machine has none.
	SCR_PersistenceSystem GetPersistenceSystem()
	{
		return m_PersistenceSystem;
	}

	//------------------------------------------------------------------------------------------------
	override event void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
		{
			EL_Debug.Info("Persistence", "manager inactive: not the authority (clients cannot write saves)");
			return;
		}

		m_PersistenceSystem = SCR_PersistenceSystem.GetScriptedInstance();
		if (!m_PersistenceSystem)
		{
			// Not fatal for the SaveGameManager half, but nothing will serialize. The
			// Workbench editor world legitimately has no game systems; an ERROR there reads
			// as a broken config, so it is a warning here.
			EL_Debug.Warn("Persistence", "no SCR_PersistenceSystem for this world - check the SCR_PersistenceSystem entry in Configs/Systems/ChimeraSystemsConfig.conf");
		}
		else
		{
			m_PersistenceSystem.GetOnStateChanged().Insert(OnPersistenceStateChanged);
			m_PersistenceSystem.GetOnBeforeSave().Insert(OnBeforeSave);
			m_PersistenceSystem.GetOnAfterSave().Insert(OnAfterSave);
			EL_Debug.Info("Persistence", "resolved SCR_PersistenceSystem, subscribed to save events");
		}

		m_GameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (m_GameMode)
		{
			m_GameMode.GetOnGameStart().Insert(OnGameModeStart);
			m_GameMode.GetOnGameEnd().Insert(OnGameEnd);
		}

		RefreshSaveCache();
	}

	//------------------------------------------------------------------------------------------------
	override event void OnDelete(IEntity owner)
	{
		if (m_bAutosaveScheduled)
			GetGame().GetCallqueue().Remove(OnAutosaveTimer);

		if (m_PersistenceSystem)
		{
			m_PersistenceSystem.GetOnStateChanged().Remove(OnPersistenceStateChanged);
			m_PersistenceSystem.GetOnBeforeSave().Remove(OnBeforeSave);
			m_PersistenceSystem.GetOnAfterSave().Remove(OnAfterSave);
			m_PersistenceSystem = null;
		}

		if (m_GameMode)
		{
			m_GameMode.GetOnGameStart().Remove(OnGameModeStart);
			m_GameMode.GetOnGameEnd().Remove(OnGameEnd);
			m_GameMode = null;
		}

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the game mode's OnGameStart invoker: starts the repeating autosave timer.
	//! Runs once per world no matter how many start paths fire.
	protected void OnGameModeStart()
	{
		if (m_bGameStarted)
			return;

		m_bGameStarted = true;
		StartAutosaves();
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the repeating autosave timer. Server only, once per world.
	void StartAutosaves()
	{
		if (m_bAutosaveScheduled || !m_bEnableAutosave || m_fAutosaveInterval <= 0)
			return;

		if (!Replication.IsServer())
			return;

		m_bAutosaveScheduled = true;
		GetGame().GetCallqueue().CallLater(OnAutosaveTimer, m_fAutosaveInterval * 1000, true);
		EL_Debug.Info("Persistence", string.Format("autosave scheduled every %1 seconds", m_fAutosaveInterval.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! Repeating timer body. A tick that lands while the save system is occupied is
	//! skipped, not queued; the next tick tries again.
	protected void OnAutosaveTimer()
	{
		if (m_bSaveInProgress || m_bLoadInProgress || m_bReapplyInProgress)
		{
			EL_Debug.Info("Persistence", "autosave tick skipped: a save or load is in progress");
			return;
		}

		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager || manager.IsBusy())
		{
			EL_Debug.Info("Persistence", "autosave tick skipped: save manager busy");
			return;
		}

		AutoSave();
	}

	//------------------------------------------------------------------------------------------------
	//! Requests an automatic save point, bounded to one AUTO slot per playthrough.
	//! Asynchronous in two stages because finding the slot needs the async GetSaves() list.
	void AutoSave()
	{
		if (!PassesSaveGates())
		{
			NotifySaveFinished(false);
			return;
		}

		SaveGameManager manager = GetGame().GetSaveGameManager();
		m_bSaveInProgress = true;
		manager.GetSaves(manager.GetCurrentMissionResource(), new SaveGameObtainCallback(OnAutosaveTargetObtained));
	}

	//------------------------------------------------------------------------------------------------
	//! SaveGameObtainCallback delegate for AutoSave() - arity must match the delegate.
	//! Overwrites the latest AUTO save of the current playthrough when one exists,
	//! otherwise creates the first.
	//! \param success Whether the save-list query itself succeeded.
	//! \param saves Save points found for the current mission, oldest first.
	protected void OnAutosaveTargetObtained(bool success, array<SaveGame> saves)
	{
		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
		{
			EL_Debug.Error("Persistence", "autosave aborted: SaveGameManager disappeared before the save could start");
			NotifySaveFinished(false);
			return;
		}

		SaveGame latestAuto;
		if (success && saves)
		{
			int playthrough = manager.GetCurrentPlaythroughNumber();
			ref array<ref EL_SaveInfo> infos = new array<ref EL_SaveInfo>();
			foreach (SaveGame save : saves)
			{
				if (save)
					infos.Insert(EL_SaveInfo.Create(save.GetType(), save.GetPlaythroughNumber()));
			}

			int index = EL_SaveSelection.FindLatestAutoSaveIndex(infos, playthrough);
			if (index >= 0)
				latestAuto = saves[index];
		}

		ESaveGameRequestFlags flags;
		if (RplSession.Mode() == RplMode.None)
			flags = ESaveGameRequestFlags.BLOCKING;

		if (latestAuto)
		{
			manager.RequestSavePointOverwrite(latestAuto, flags, new SaveGameOperationCallback(OnSavePointCompleted));
		}
		else
		{
			manager.RequestSavePoint(ESaveGameType.AUTO, string.Empty, flags, new SaveGameOperationCallback(OnSavePointCompleted));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Requests a player- or admin-initiated save point. Result is reported through
	//! GetOnSaveFinished(), never assumed.
	void SaveGame()
	{
		RequestSavePoint(ESaveGameType.MANUAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the game mode's OnGameEnd invoker: requests the shutdown save, once per
	//! session. The request is BLOCKING so it completes before session teardown.
	protected void OnGameEnd()
	{
		if (m_bShutdownSaveRequested)
			return;

		if (!m_bGameStarted)
		{
			EL_Debug.Info("Persistence", "shutdown save skipped: the game never started");
			return;
		}

		m_bShutdownSaveRequested = true;
		EL_Debug.Info("Persistence", "session ending, requesting shutdown save");
		RequestSavePoint(ESaveGameType.SHUTDOWN);
	}

	//------------------------------------------------------------------------------------------------
	//! Continues from the most recent save point, BY RESTARTING THE SESSION.
	//!
	//! This is the production continue path and one of two ways saved data comes back.
	//! LoadLatestSave() asks the engine to take the session through a game-state
	//! transition into the save: everything comes back, including world entities, and the
	//! current session ends. ReapplyLatestSaveData() is the no-transition alternative.
	//!
	//! Asynchronous in two stages because the SaveGameManager offers no synchronous way to
	//! name a save: ask for the mission's save list, then load its last entry (vanilla's
	//! own idiom, SCR_ScenarioUICommon.c). Poll IsLoadInProgress() for completion and read
	//! GetLastLoadDiagnostic() to find out why one never got that far.
	void LoadLatestSave()
	{
		m_sLastLoadDiagnostic = "";

		if (!Replication.IsServer())
		{
			m_sLastLoadDiagnostic = "only the authority can load a save point";
			EL_Debug.Warn("Persistence", m_sLastLoadDiagnostic);
			return;
		}

		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
		{
			m_sLastLoadDiagnostic = "there is no SaveGameManager on this machine";
			EL_Debug.Error("Persistence", m_sLastLoadDiagnostic);
			return;
		}

		if (manager.IsBusy())
		{
			m_sLastLoadDiagnostic = "the save system is busy with another operation";
			EL_Debug.Warn("Persistence", m_sLastLoadDiagnostic);
			return;
		}

		m_bLoadInProgress = true;
		manager.GetSaves(manager.GetCurrentMissionResource(), new SaveGameObtainCallback(OnLatestSaveObtained));
	}

	//------------------------------------------------------------------------------------------------
	//! SaveGameObtainCallback delegate for LoadLatestSave().
	//! \param success Whether the save-list query itself succeeded.
	//! \param saves Save points found for the current mission, oldest first.
	protected void OnLatestSaveObtained(bool success, array<SaveGame> saves)
	{
		bool found = false;
		if (success && saves)
			found = saves.Count() > 0;

		m_bHasSaveGame = found;
		m_bSaveCacheSeeded = true;

		if (!found)
		{
			m_bLoadInProgress = false;
			m_sLastLoadDiagnostic = "no save point exists for this mission";
			EL_Debug.Warn("Persistence", m_sLastLoadDiagnostic);
			return;
		}

		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
		{
			m_bLoadInProgress = false;
			m_sLastLoadDiagnostic = "the SaveGameManager disappeared before the load could start";
			EL_Debug.Error("Persistence", m_sLastLoadDiagnostic);
			return;
		}

		// transition: true - the engine takes the session through a game-state transition
		// into the saved world. m_bLoadInProgress stays true on purpose: a successful load
		// tears this component down with the world, which is the caller's completion signal.
		EL_Debug.Info("Persistence", "loading the latest save point");
		manager.Load(saves[saves.Count() - 1], true);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-reads the persisted record for the game mode entity and applies it to the LIVE
	//! session. No world transition: the persistence system fetches the stored data for an
	//! instance that already exists and re-runs its serializers' Deserialize over it
	//! (the vanilla idiom, SCR_SpawnLogic.c).
	//!
	//! This is the in-session re-apply path: every serializer reached must be idempotent
	//! on a live session, which the EL serializers are (see EL_PersistenceComponentSerializer).
	//! Poll IsReapplyInProgress() for completion and read GetLastReapplyDiagnostic() after.
	void ReapplyLatestSaveData()
	{
		RequestReapply(GetOwner(), "the game-mode entity is not tracked, so it has no stored record - check the Persistence component on the game-mode prefab");
	}

	//------------------------------------------------------------------------------------------------
	//! Re-reads the persisted record for ONE live tracked entity and applies it to that
	//! entity. Same mechanism and contract as ReapplyLatestSaveData().
	//! \param entity The tracked entity to re-read.
	void ReapplyEntitySaveData(IEntity entity)
	{
		RequestReapply(entity, "that entity is not tracked, so it has no stored record");
	}

	//------------------------------------------------------------------------------------------------
	//! Shared body of both re-applications.
	//! \param instance The tracked instance to re-read.
	//! \param untrackedDiagnostic What to report when the instance carries no stored record.
	protected void RequestReapply(IEntity instance, string untrackedDiagnostic)
	{
		m_sLastReapplyDiagnostic = "";

		if (!Replication.IsServer())
		{
			m_sLastReapplyDiagnostic = "only the authority can read persisted data";
			EL_Debug.Warn("Persistence", m_sLastReapplyDiagnostic);
			return;
		}

		if (!m_PersistenceSystem)
		{
			m_sLastReapplyDiagnostic = "there is no persistence system in this world";
			EL_Debug.Error("Persistence", m_sLastReapplyDiagnostic);
			return;
		}

		if (m_PersistenceSystem.GetState() != EPersistenceSystemState.ACTIVE)
		{
			m_sLastReapplyDiagnostic = "the persistence system is not active yet";
			EL_Debug.Warn("Persistence", m_sLastReapplyDiagnostic);
			return;
		}

		if (!instance)
		{
			m_sLastReapplyDiagnostic = "there is no instance to re-apply saved data to";
			EL_Debug.Error("Persistence", m_sLastReapplyDiagnostic);
			return;
		}

		if (!m_PersistenceSystem.IsTracked(instance))
		{
			m_sLastReapplyDiagnostic = untrackedDiagnostic;
			EL_Debug.Error("Persistence", m_sLastReapplyDiagnostic);
			return;
		}

		PersistenceLoadRequest request();
		request.Instances = {instance};

		m_bReapplyInProgress = true;
		m_PersistenceSystem.RequestLoad(request, new PersistenceResultCallback(OnReapplyResult));
	}

	//------------------------------------------------------------------------------------------------
	//! PersistenceResultCallback delegate for RequestReapply() - arity must match the
	//! delegate exactly (PersistenceSystem.RequestLoad, SCR_SpawnLogic.c is the model).
	//! \param statusCode OK when the instance's stored data was found and applied.
	//! \param result The instance the result belongs to.
	//! \param isLast True on the final result of the request.
	//! \param context Unused - no context was passed to the request.
	protected void OnReapplyResult(EPersistenceStatusCode statusCode, Managed result, bool isLast, Managed context)
	{
		if (statusCode == EPersistenceStatusCode.OK)
		{
			m_iCompletedReapplies += 1;
			EL_Debug.Info("Persistence", "saved data re-applied to the live session");
		}
		else
		{
			m_sLastReapplyDiagnostic = string.Format("the persistence system answered %1 when re-reading a stored record",
				typename.EnumToString(EPersistenceStatusCode, statusCode));
			EL_Debug.Error("Persistence", m_sLastReapplyDiagnostic);
		}

		if (isLast)
			m_bReapplyInProgress = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes every save point of the current mission. Asynchronous.
	void WipeSave()
	{
		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
		{
			EL_Debug.Error("Persistence", "cannot wipe saves - no SaveGameManager");
			return;
		}

		EL_Debug.Info("Persistence", "wiping every save point of the current mission");
		manager.Purge(manager.GetCurrentMissionResource(), -1, new SaveGameOperationCallback(OnPurgeCompleted));
	}

	//------------------------------------------------------------------------------------------------
	//! SaveGameOperationCallback delegate for WipeSave().
	//! \param success Whether the purge completed.
	protected void OnPurgeCompleted(bool success)
	{
		if (success)
			EL_Debug.Info("Persistence", "save points wiped");
		else
			EL_Debug.Error("Persistence", "save wipe FAILED");

		RefreshSaveCache();
	}

	//------------------------------------------------------------------------------------------------
	//! The common gate in front of every save request. Says why when the answer is no.
	//! \return True when a save request may be made right now.
	protected bool PassesSaveGates()
	{
		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
		{
			EL_Debug.Error("Persistence", "save rejected: no SaveGameManager");
			return false;
		}

		if (!manager.IsSavingAllowed())
		{
			EL_Debug.Warn("Persistence", "save rejected: saving is disabled or disallowed");
			return false;
		}

		if (!m_PersistenceSystem || m_PersistenceSystem.GetState() != EPersistenceSystemState.ACTIVE)
		{
			EL_Debug.Warn("Persistence", "save rejected: the persistence system is not active");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The single save path behind SaveGame() / OnGameEnd() / AutoSave().
	//! \param saveType Which kind of save point to ask for.
	protected void RequestSavePoint(ESaveGameType saveType)
	{
		if (!PassesSaveGates())
		{
			NotifySaveFinished(false);
			return;
		}

		SaveGameManager manager = GetGame().GetSaveGameManager();

		// Vanilla gates a blocking save to outside a replication session, where a hitch
		// costs nobody else. The shutdown save is the exception: the session is ending
		// either way, and an async request would be torn down before it completes.
		ESaveGameRequestFlags flags;
		if (RplSession.Mode() == RplMode.None || saveType == ESaveGameType.SHUTDOWN)
			flags = ESaveGameRequestFlags.BLOCKING;

		m_bSaveInProgress = true;
		EL_Debug.Info("Persistence", string.Format("requesting save point (%1)", typename.EnumToString(ESaveGameType, saveType)));
		manager.RequestSavePoint(saveType, string.Empty, flags, new SaveGameOperationCallback(OnSavePointCompleted));
	}

	//------------------------------------------------------------------------------------------------
	//! SaveGameOperationCallback delegate - arity must match the delegate.
	//! \param success Whether the save point was committed.
	protected void OnSavePointCompleted(bool success)
	{
		if (success)
		{
			m_bHasSaveGame = true;
			m_bSaveCacheSeeded = true;
			m_iCompletedSaves += 1;
			EL_Debug.Info("Persistence", "save point created");
		}
		else
		{
			EL_Debug.Error("Persistence", "save point FAILED");
		}

		NotifySaveFinished(success);
	}

	//------------------------------------------------------------------------------------------------
	//! Clears the in-flight flag and tells subscribers what actually happened.
	//! \param success Outcome to report.
	protected void NotifySaveFinished(bool success)
	{
		m_bSaveInProgress = false;
		m_OnSaveFinished.Invoke(success);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-asks the SaveGameManager whether this mission has any save points. Asynchronous.
	void RefreshSaveCache()
	{
		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
			return;

		manager.GetSaves(manager.GetCurrentMissionResource(), new SaveGameObtainCallback(OnSavesObtained));
	}

	//------------------------------------------------------------------------------------------------
	//! SaveGameObtainCallback delegate - arity must match the delegate.
	//! \param success Whether the query itself succeeded.
	//! \param saves Save points found for the mission filter that was passed.
	protected void OnSavesObtained(bool success, array<SaveGame> saves)
	{
		bool found = false;
		if (success && saves)
			found = saves.Count() > 0;

		m_bHasSaveGame = found;
		m_bSaveCacheSeeded = true;
		EL_Debug.Info("Persistence", string.Format("save scan complete, save present: %1", m_bHasSaveGame.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence system state changed. There is no OnAfterLoad invoker; reaching ACTIVE
	//! is the "data is in" signal, which is how vanilla itself detects it.
	//! \param oldState State being left.
	//! \param newState State being entered.
	protected void OnPersistenceStateChanged(EPersistenceSystemState oldState, EPersistenceSystemState newState)
	{
		EL_Debug.Info("Persistence", string.Format("system state %1 -> %2",
			typename.EnumToString(EPersistenceSystemState, oldState),
			typename.EnumToString(EPersistenceSystemState, newState)));

		if (newState != EPersistenceSystemState.ACTIVE)
			return;

		if (IsPlayingLoadedSave())
		{
			m_bHasSaveGame = true;
			m_bSaveCacheSeeded = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Called before any save (including engine-initiated ones), on the authority.
	//! \param saveType Kind of save being taken.
	protected void OnBeforeSave(ESaveGameType saveType)
	{
		EL_Debug.Info("Persistence", string.Format("preparing to save (%1)", typename.EnumToString(ESaveGameType, saveType)));
	}

	//------------------------------------------------------------------------------------------------
	//! Called after any save completes, on the authority. Fires for engine-initiated saves
	//! too, which is why the cache is kept current here as well as in the request callback.
	//! \param saveType Kind of save that was taken.
	//! \param success Whether it succeeded.
	protected void OnAfterSave(ESaveGameType saveType, bool success)
	{
		if (success)
		{
			m_bHasSaveGame = true;
			m_bSaveCacheSeeded = true;
			EL_Debug.Info("Persistence", string.Format("save completed (%1)", typename.EnumToString(ESaveGameType, saveType)));
		}
		else
		{
			EL_Debug.Error("Persistence", string.Format("save failed (%1)", typename.EnumToString(ESaveGameType, saveType)));
		}
	}
}