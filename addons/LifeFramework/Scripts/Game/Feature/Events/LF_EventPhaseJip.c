//------------------------------------------------------------------------------------------------
//! Late joiners and the running event phases (#2106). LF_EventShell announces each phase change by a
//! broadcast RPC, which a player who joins mid-event never heard. The game mode's JIP snapshot now
//! carries the running phases (a format version, a count, then id and phase pairs), and RplLoad
//! fills LF_EventPhaseBoard with them, so a late joiner's board matches everyone else's at once.
//!
//! Only running events are sent; an idle or cooling event has no phase on the board either.
class LF_EventPhaseJip
{
	static const int FORMAT_VERSION = 1;
	static const int MAX_EVENTS = 256;	// lf-fixed: a sanity cap on a corrupt count, far above any config

	//------------------------------------------------------------------------------------------------
	//! Server: the running phases of the events in `eventIds`, as flat id, phase pairs.
	static array<string> Collect(array<string> eventIds)
	{
		array<string> pairs = {};
		if (!eventIds)
			return pairs;

		foreach (string id : eventIds)
		{
			string phase = LF_EventShell.PhaseOf(id);
			if (id.IsEmpty() || phase.IsEmpty())
				continue;
			pairs.Insert(id);
			pairs.Insert(phase);
		}
		return pairs;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the ids of the configured events (the shell runs the same config, LF_EventShell.LoadConfig).
	static array<string> ConfiguredIds()
	{
		array<string> ids = {};
		LF_EventsConfig config = LF_EventShell.LoadConfig();
		if (!config || !config.m_aEvents)
			return ids;

		foreach (LF_EventDef def : config.m_aEvents)
		{
			if (def)
				ids.Insert(def.m_sEventId);
		}
		return ids;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: puts the joined pairs on the board. \return the number of events applied.
	static int Apply(array<string> pairs)
	{
		if (!pairs)
			return 0;

		int applied;
		for (int i = 0; i + 1 < pairs.Count(); i += 2)
		{
			LF_EventPhaseBoard.OnAnnounced(pairs[i], pairs[i + 1], "jip");
			applied++;
		}
		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, harness only (dev gate checked by the caller): runs the event `harness_jip`, which starts
	//! in `prep` and holds in `hot` (its condition wants 1000 players), so a late joiner can be tested.
	static string HarnessStart()
	{
		if (!Replication.IsServer() || !GetGame() || !GetGame().GetWorld())
			return "not_server";

		LF_EventShell.Unregister(HARNESS_HOLD);
		LF_EventShell.RegisterCondition(HARNESS_HOLD, LF_EventPrecondition.MakePlayers(LF_EEventConditionKind.PLAYERS_ONLINE, 1000));
		array<string> holdModules = {HARNESS_HOLD};
		array<ref LF_EventPhase> phases = {};
		phases.Insert(LF_EventPhase.Make("prep", 0, 0));
		phases.Insert(LF_EventPhase.Make("hot", 0, 0, holdModules));
		LF_EventsConfig config = new LF_EventsConfig();
		config.m_aEvents = {};
		config.m_aEvents.Insert(LF_EventDef.Make(HARNESS_EVENT, phases, 300));
		LF_EventShell.Use(config);

		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		LF_EventShell.Tick(now);
		LF_EventShell.Tick(now + 1);
		return HARNESS_EVENT + "=" + LF_EventShell.PhaseOf(HARNESS_EVENT);
	}

	static const string HARNESS_EVENT = "harness_jip";
	static const string HARNESS_HOLD = "harness_jip_hold";

	//------------------------------------------------------------------------------------------------
	static void Write(notnull ScriptBitWriter writer, notnull array<string> pairs)
	{
		int count = pairs.Count() / 2;
		writer.WriteInt(FORMAT_VERSION);
		writer.WriteInt(count);
		for (int i = 0; i < count * 2; i++)
			writer.WriteString(pairs[i]);
	}

	//------------------------------------------------------------------------------------------------
	//! \return false when the stream is short or corrupt (the snapshot is then refused).
	static bool Read(notnull ScriptBitReader reader, notnull array<string> pairs)
	{
		int version;
		int count;
		if (!reader.ReadInt(version) || !reader.ReadInt(count))
			return false;

		if (count < 0 || count > MAX_EVENTS)
		{
			LF_Debug.Error(LF_EventShell.FEATURE, string.Format("jip: the snapshot claims %1 running events; refused", count));
			return false;
		}

		// Read whatever the format so the stream stays aligned for what follows.
		for (int i = 0; i < count * 2; i++)
		{
			string value;
			if (!reader.ReadString(value))
				return false;
			pairs.Insert(value);
		}

		if (version != FORMAT_VERSION)
		{
			LF_Debug.Error(LF_EventShell.FEATURE, string.Format("jip: running phases dropped: the server sends format %1, this build reads %2", version, FORMAT_VERSION));
			pairs.Clear();
		}
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The running phases ride in the game mode's JIP snapshot after whatever the chain below writes.
//! Positional: RplLoad reads exactly what RplSave writes, in this order, after super.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	override bool RplSave(ScriptBitWriter writer)
	{
		if (!super.RplSave(writer))
			return false;

		array<string> ids = LF_EventPhaseJip.ConfiguredIds();
		if (LF_EventShell.PhaseOf(LF_EventPhaseJip.HARNESS_EVENT) != "")
			ids.Insert(LF_EventPhaseJip.HARNESS_EVENT);
		array<string> pairs = LF_EventPhaseJip.Collect(ids);
		LF_EventPhaseJip.Write(writer, pairs);
		if (!pairs.IsEmpty())
			LF_Debug.Info(LF_EventShell.FEATURE, string.Format("jip: snapshot carries %1 running event(s)", pairs.Count() / 2));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		if (!super.RplLoad(reader))
			return false;

		array<string> pairs = {};
		if (!LF_EventPhaseJip.Read(reader, pairs))
		{
			LF_Debug.Error(LF_EventShell.FEATURE, "jip: the join snapshot carries no running phases; the board waits for the next change");
			return false;
		}

		LF_EventPhaseJip.Apply(pairs);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Harness `eventjip start` asks the server (dev gate) to run the harness event; `eventjip board <id>`
//! reads this client's LF_EventPhaseBoard. The late-join proof: start on client 1, join client 2, read.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "eventjip")
			return super.Handle(cmd);

		if (cmd.Arg1 == "board")
		{
			string id = cmd.Arg2;
			if (id.IsEmpty())
				id = LF_EventPhaseJip.HARNESS_EVENT;
			LF_AgentHarness.Answer(cmd.Id, "ok", id + "=" + LF_EventPhaseBoard.PhaseOf(id));
			return true;
		}

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (cmd.Arg1 != "start" || !character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: eventjip start|board [id] (start needs a character)");
			return true;
		}

		character.LF_AskHarnessEventJip(cmd.Id);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskHarnessEventJip(int commandId)
	{
		if (Replication.IsServer())
		{
			LF_HarnessReply(commandId, "ok", LF_EventPhaseJip.HarnessStart());
			return;
		}

		Rpc(RpcAsk_LF_HarnessEventJip, commandId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_HarnessEventJip(int commandId)
	{
		if (!LF_DevGate.CheckHarnessRequest(this, "eventjip"))
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		LF_HarnessReply(commandId, "ok", LF_EventPhaseJip.HarnessStart());
	}
}
