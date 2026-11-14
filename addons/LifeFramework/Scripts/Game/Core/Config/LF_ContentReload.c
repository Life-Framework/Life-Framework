//------------------------------------------------------------------------------------------------
//! One thing `reload <what>` can read again (#970). A reloader is a class named for what it reloads,
//! `LF_ContentReloader_<Name>`, found by name the way a dialogue hook is: there is no list to append
//! to, a feature that can reload adds its own class in its own file (the terms layer, #958, adds
//! `LF_ContentReloader_Terms`). A reloader runs on the server, changes only server state, logs what
//! changed and returns a short summary; LF_ContentReload sends the result to the clients once.
class LF_ContentReloader : Managed
{
	static const string CLASS_PREFIX = "LF_ContentReloader_";

	//------------------------------------------------------------------------------------------------
	//! Reads the content again.
	//! \return What changed, one short line.
	string Run()
	{
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this reloader needs the dev tools opened by the game mode or a harness build. A reloader
	//! that reads a file only a developer writes says yes, so a live server never reads it.
	bool NeedsDevTools()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The reloader for a name, or null (logged).
	static LF_ContentReloader Find(string name)
	{
		if (name.IsEmpty())
			return null;

		// The verb is lower case (`reload dialogue`); the class is LF_ContentReloader_Dialogue.
		string head = name.Substring(0, 1);
		head.ToUpper();
		string className = CLASS_PREFIX + head + name.Substring(1, name.Length() - 1);
		typename type = className.ToType();
		if (!type || !type.IsInherited(LF_ContentReloader))
		{
			LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("reload: nothing reloads '%1' (no class %2)", name, className));
			return null;
		}

		return LF_ContentReloader.Cast(type.Spawn());
	}
}

//------------------------------------------------------------------------------------------------
//! `reload config`: the owner's config.json is read again, the features re-price and the new values
//! stream to the clients (what /configreload does).
class LF_ContentReloader_Config : LF_ContentReloader
{
	override string Run()
	{
		LF_ConfigService.Boot();
		LF_ConfigStream.ApplyToFeatures();
		return string.Format("config.json read again (owner layer %1)", LF_ConfigService.HasOwnerLayer());
	}
}

//------------------------------------------------------------------------------------------------
//! `reload dialogue`: the dev caption file is read again (LF_DialogueOverrides). Dev tools only.
class LF_ContentReloader_Dialogue : LF_ContentReloader
{
	override string Run()
	{
		return LF_DialogueOverrides.ReloadFromFile();
	}

	override bool NeedsDevTools()
	{
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The dev reload (#970): `reload config`, `reload dialogue` or a comma list, asked by a client,
//! decided and run by the server. The client sends only the names.
class LF_ContentReload
{
	static const int MAX_ASK_LENGTH = 64; // lf-fixed: a request is a few short names

	//------------------------------------------------------------------------------------------------
	//! The decision, pure so the suite can test every cell.
	//! \param[in] needsDevTools The reloader reads a developer's file.
	//! \param[in] devTools The game mode opens dev tools, or this is a harness build.
	//! \param[in] allowed The requester passes LF_ConfigReload.IsAllowed (harness build, dev tools or an administrator).
	static bool Permits(bool needsDevTools, bool devTools, bool allowed)
	{
		if (!allowed)
			return false;

		if (needsDevTools && !devTools)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when this server opened dev tools (a dev world, or a harness build). An
	//! administrator alone does not pass: a live server never reads a developer's file.
	static bool DevToolsOpen()
	{
		if (LF_DevGate.HarnessBuild())
			return true;

		LF_GameModeRoleplay gameMode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		return gameMode && gameMode.AreDevToolsEnabled();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one reload request. Runs each name, then sends the result to the clients once.
	//! \param[in] playerId The asking player.
	//! \param[in] what A name or a comma list: config, dialogue.
	//! \return The log line: what each reloader changed, or why one was refused.
	static string ServerRun(int playerId, string what)
	{
		if (!Replication.IsServer())
			return "not the server";

		bool allowed = LF_ConfigReload.IsAllowed(playerId);
		bool devTools = DevToolsOpen();
		if (!allowed)
		{
			LF_Debug.Warn(LF_ConfigService.FEATURE, string.Format("reload '%1' refused for player %2: administrators only", what, playerId));
			return "refused";
		}

		// A client sends the names; never parse more than the few that exist.
		if (what.Length() > MAX_ASK_LENGTH)
		{
			LF_Debug.Warn(LF_ConfigService.FEATURE, string.Format("reload refused for player %1: the request is %2 characters", playerId, what.Length()));
			return "refused";
		}

		array<string> names = {};
		what.Split(",", names, true);
		if (names.IsEmpty())
		{
			LF_Debug.Warn(LF_ConfigService.FEATURE, "reload: say what to reload (config, dialogue)");
			return "nothing asked";
		}

		string result;
		foreach (string asked : names)
		{
			string name = asked.Trim();
			LF_ContentReloader reloader = LF_ContentReloader.Find(name);
			if (!reloader)
			{
				result += name + ": unknown; ";
				continue;
			}

			if (!Permits(reloader.NeedsDevTools(), devTools, allowed))
			{
				LF_Debug.Warn(LF_ConfigService.FEATURE, string.Format("reload '%1' refused for player %2: it reads a developer's file and dev tools are off in this game mode", name, playerId));
				result += name + ": refused (dev tools off); ";
				continue;
			}

			string summary = reloader.Run();
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("reload %1 by player %2: %3", name, playerId, summary));
			result += name + ": " + summary + "; ";
		}

		LF_GameModeRoleplay gameMode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.LF_PublishConfigStream();
		return result;
	}
};

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Asks the server to reload what a dev changed. The player owns their controller, so the RPC is allowed.
	void LF_AskContentReload(string what)
	{
		if (Replication.IsServer())
			RpcAsk_LF_ContentReload(what);
		else
			Rpc(RpcAsk_LF_ContentReload, what);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_ContentReload(string what)
	{
		LF_ContentReload.ServerRun(GetPlayerId(), what);
	}
};
