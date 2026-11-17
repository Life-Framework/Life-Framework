//------------------------------------------------------------------------------------------------
//! Game Master for testers in development worlds (#617, epic #145 Moderation).
//!
//! Vanilla gives the EDIT (Game Master) mode only to players holding the administrator role
//! (EEditorModeFlag.ADMIN on the EDIT prefab in the base-game EditorManagerCore.conf). A hosted
//! or dedicated playtest has no admin password, so nobody is admin and the editor never opens.
//! When the game mode opts into dev tools (GameMode_Roleplay_Debug.et, and the mission header
//! does not close them), every player's editor manager gets EDIT on the BASE access layer, so a
//! tester can open Game Master, fly, spawn and move things. The production game mode never
//! installs this: there only administrators get Game Master, as in the base game.
//! SERVER only. Nothing persists.
//------------------------------------------------------------------------------------------------
class LF_DevGameMaster
{
	static const string FEATURE = "DevGameMaster";

	// BASE is rewritten by the core right after it raises Event_OnEditorManagerCreatedServer, so the
	// grant waits a short moment and re-resolves the manager by player id.
	static const int GRANT_DELAY_MS = 500;

	protected static bool s_bInstalled;

	//------------------------------------------------------------------------------------------------
	//! True while the editor-manager handler is installed on this world's core.
	static bool IsInstalled()
	{
		return s_bInstalled;
	}

	//------------------------------------------------------------------------------------------------
	//! World end: take the handler off the editor core and forget the install. SCR_EditorManagerCore
	//! outlives the world, so a handler left on it would keep granting EDIT in a production world
	//! loaded after a dev-tools world in the same process (#2415).
	static void ResetStatics()
	{
		if (s_bInstalled)
		{
			SCR_EditorManagerCore core = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
			if (core)
				core.Event_OnEditorManagerCreatedServer.Remove(OnManagerCreated);
		}

		s_bInstalled = false;
	}

	//------------------------------------------------------------------------------------------------
	//! A grant needs this install and the current game mode's dev-tools gate, read again at grant
	//! time: a handler or a delayed call that outlived its world must not grant in the next one.
	//! \param installed The install flag.
	//! \param devToolsEnabled The game mode's gate (LF_GameModeRoleplay.AreDevToolsEnabled).
	static bool MayGrant(bool installed, bool devToolsEnabled)
	{
		return installed && devToolsEnabled;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: grant EDIT to every editor manager, present and future. Safe to call twice.
	//! \param enabled The game mode's dev-tools gate (LF_GameModeRoleplay.AreDevToolsEnabled).
	static void Install(bool enabled)
	{
		if (!enabled || s_bInstalled || !Replication.IsServer())
			return;

		SCR_EditorManagerCore core = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
		if (!core)
		{
			LF_Debug.Error(FEATURE, "no SCR_EditorManagerCore; Game Master stays admin-only");
			return;
		}

		s_bInstalled = true;
		core.Event_OnEditorManagerCreatedServer.Insert(OnManagerCreated);

		array<SCR_EditorManagerEntity> existing = {};
		core.GetEditorEntities(existing);
		foreach (SCR_EditorManagerEntity manager : existing)
		{
			OnManagerCreated(manager);
		}

		LF_Debug.Info(FEATURE, "dev Game Master on: every player may open EDIT mode");
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnManagerCreated(SCR_EditorManagerEntity manager)
	{
		if (!manager)
			return;

		GetGame().GetCallqueue().CallLater(Grant, GRANT_DELAY_MS, false, manager.GetPlayerID());
	}

	//------------------------------------------------------------------------------------------------
	protected static void Grant(int playerId)
	{
		bool devTools;
		LF_GameModeRoleplay gameMode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (gameMode)
			devTools = gameMode.AreDevToolsEnabled();

		if (!MayGrant(s_bInstalled, devTools))
		{
			LF_Debug.Warn(FEATURE, string.Format("player %1 not granted EDIT: dev tools are off in this world", playerId));
			return;
		}

		SCR_EditorManagerCore core = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
		if (!core)
			return;

		SCR_EditorManagerEntity manager = core.GetEditorManager(playerId);
		if (!manager)
			return;

		manager.AddEditorModes(EEditorModeAccess.BASE, EEditorMode.EDIT);
		LF_Debug.Info(FEATURE, string.Format("player %1 granted EDIT (modes %2)", playerId, manager.GetEditorModes()));
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_DevGameMaster's statics (registered at script load; Core/LF_StaticReset.c).
class LF_DevGameMaster_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_DevGameMaster_StaticResetHook());

	override string Owner()
	{
		return "LF_DevGameMaster";
	}

	override void Apply()
	{
		LF_DevGameMaster.ResetStatics();
	}
}
