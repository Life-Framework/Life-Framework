//------------------------------------------------------------------------------------------------
//! Starts an event scene from a fixture conf in a session (#2173). LF_EventOverlay reads only the
//! core conf and the pack overlays, so an event that ships off (`m_bEnabled 0`) could not run in a
//! proof without a pack patch. `Start` reloads that list, lays the fixture (root class
//! LF_EventsConfig, the same overlay rules as a pack's) over it, hands the result to the shell and the
//! module loader, and optionally forces the hour the shell's conditions read. `Stop` puts the
//! files' own list back. Harness only: the verb below is dev gated on the server.
class LF_EventSceneHarness
{
	static const string FEATURE = "Events";
	static const string REFUSED_NO_CONF = "refused: no fixture conf named";
	static const string REFUSED_NOT_LOADED = "refused: the fixture conf does not load";
	static const string REFUSED_WRONG_ROOT = "refused: the fixture is not an LF_EventsConfig";

	protected static ResourceName s_sFixture;

	//------------------------------------------------------------------------------------------------
	//! Server: applies `fixture` over the event list and runs it. `hour` >= 0 forces the shell's hour.
	//! \return "ok: ..." or one of the REFUSED_* lines (logged).
	static string Start(ResourceName fixture, float hour = -1)
	{
		if (fixture.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "eventscene: " + REFUSED_NO_CONF);
			return REFUSED_NO_CONF;
		}

		Resource resource = BaseContainerTools.LoadContainer(fixture);
		if (!resource || !resource.IsValid() || !resource.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("eventscene: fixture %1 does not load; nothing changed", fixture));
			return REFUSED_NOT_LOADED;
		}

		BaseContainer root = resource.GetResource().ToBaseContainer();
		if (!root || root.GetClassName() != "LF_EventsConfig")
		{
			LF_Debug.Error(FEATURE, string.Format("eventscene: fixture %1 is not an LF_EventsConfig; nothing changed", fixture));
			return REFUSED_WRONG_ROOT;
		}

		LF_EventOverlay.Reload();
		LF_EventsConfig config = LF_EventOverlay.Get();
		if (!config)
			return "refused: the event config does not load";

		int changed = LF_EventOverlay.ApplyOverlay(config, root, fixture);
		LF_EventShell.Use(config);
		int modules = LF_EventModuleLoader.RegisterFrom(config);
		if (hour >= 0)
			LF_EventShell.ForceHour(hour);
		LF_EventShell.StartIfServer();
		s_sFixture = fixture;

		string line = string.Format("fixture %1 applied: %2 event(s) changed, %3 running, %4 module(s), hour %5", fixture, changed, LF_EventShell.RunCount(), modules, hour);
		LF_Debug.Info(FEATURE, "eventscene: " + line);
		return "ok: " + line;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: back to the files' own event list and the real clock. \return a one-line summary.
	static string Stop()
	{
		LF_EventOverlay.Reload();
		LF_EventsConfig config = LF_EventOverlay.Get();
		LF_EventShell.Use(config);
		LF_EventModuleLoader.RegisterFrom(config);
		LF_EventShell.ForceHour(-1);
		s_sFixture = "";
		LF_Debug.Info(FEATURE, string.Format("eventscene: fixture dropped, %1 event(s) running", LF_EventShell.RunCount()));
		return string.Format("ok: fixture dropped, %1 running", LF_EventShell.RunCount());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the running scene's crew and walk, and `eventId`'s phase when one is named.
	static string Status(string eventId = "")
	{
		string phase = "";
		if (!eventId.IsEmpty())
			phase = string.Format(" event %1 phase '%2';", eventId, LF_EventShell.PhaseOf(eventId));

		string head = string.Format("fixture '%1';%2", s_sFixture, phase);
		LF_EventSceneCrew crew = LF_EventSceneSpawn.Running();
		if (!crew)
			return head + " no scene running";

		string line = string.Format("%1 scene %2: %3 spawned", head, crew.m_sSceneId, crew.m_aPeople.Count());
		LF_EventSceneWalk walk = LF_EventSceneWalk.Active();
		if (!walk || walk.m_Crew != crew)
			return line + ", not walking";

		int walking = walk.m_aWalkers.Count() - walk.CountArrived() - walk.CountTimedOut();
		return string.Format("%1, walking %2, arrived %3, timed out %4, %5 s", line, walking, walk.CountArrived(), walk.CountTimedOut(), walk.m_fElapsed);
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName Fixture()
	{
		return s_sFixture;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_sFixture = "";
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_EventSceneHarness's statics (Core/LF_StaticReset.c).
class LF_EventSceneHarness_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_EventSceneHarness_StaticResetHook());

	override string Owner()
	{
		return "LF_EventSceneHarness";
	}

	override void Apply()
	{
		LF_EventSceneHarness.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Harness `eventscene start <{GUID}fixture.conf> [hour]` | `eventscene status [eventId]` |
//! `eventscene stop`. The server runs each under the dev gate (as `eventjip` does).
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "eventscene")
			return super.Handle(cmd);

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		bool known = cmd.Arg1 == "start" || cmd.Arg1 == "status" || cmd.Arg1 == "stop";
		if (!known || !character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: eventscene start <{GUID}fixture.conf> [hour] | status [eventId] | stop (needs a character)");
			return true;
		}

		float hour = -1;
		if (cmd.Arg1 == "start" && !cmd.Arg3.IsEmpty())
			hour = cmd.Arg3.ToFloat();

		character.LF_AskHarnessEventScene(cmd.Id, cmd.Arg1, cmd.Arg2, hour);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskHarnessEventScene(int commandId, string action, string arg, float hour)
	{
		if (Replication.IsServer())
		{
			LF_HarnessReplyEventScene(commandId, action, arg, hour);
			return;
		}

		Rpc(RpcAsk_LF_HarnessEventScene, commandId, action, arg, hour);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessEventScene(int commandId, string action, string arg, float hour)
	{
		if (!LF_DevGate.CheckHarnessRequest(this, "eventscene"))
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		LF_HarnessReplyEventScene(commandId, action, arg, hour);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_HarnessReplyEventScene(int commandId, string action, string arg, float hour)
	{
		string detail;
		if (action == "start")
			detail = LF_EventSceneHarness.Start(arg, hour);
		else if (action == "stop")
			detail = LF_EventSceneHarness.Stop();
		else
			detail = LF_EventSceneHarness.Status(arg);

		string status = "ok";
		if (detail.StartsWith("refused"))
			status = "error";
		LF_HarnessReply(commandId, status, detail);
	}
}
