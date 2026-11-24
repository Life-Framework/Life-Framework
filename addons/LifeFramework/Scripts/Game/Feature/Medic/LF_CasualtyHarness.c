// Harness verb `casualty roll [kind]` (#4853) and the stand name `Casualty` (#4853). A played medic hour
// cannot wait one roll interval (1200 s on Everon) for a casualty, so the verb rolls one now: it skips the
// interval and the open cap, still needs a medic on duty, and answers with the new casualty's caller uid,
// row kind, position and expiry. `stand Casualty` (or `stand casualty:<n>`) walks the character to that
// body itself, so the revive hold ("Treat the patient") is in reach; a place entity only gets near it.
//
// A harness build only: the verb and its RPC are compiled out without -scrDefine LF_HARNESS. The name
// lookup stays in every build (LF_AgentHarness.ResolveNamed calls it; the suite tests it).

//------------------------------------------------------------------------------------------------
class LF_CasualtyHarness
{
	static const string STAND_NAME = "Casualty";

	//------------------------------------------------------------------------------------------------
	//! Server: the body a harness name stands for. "Casualty" is the newest open casualty, "casualty:<n>"
	//! one by its caller uid. null on a client (it cannot read the server's list) and for any other name.
	static IEntity Named(string entityName)
	{
		if (!Replication.IsServer() || LF_CasualtyCalls.Open().IsEmpty())
			return null;

		bool newest = entityName == STAND_NAME;
		if (!newest && !entityName.StartsWith(LF_CasualtyCalls.CALLER_PREFIX))
			return null;

		array<ref LF_Casualty> open = LF_CasualtyCalls.Open();
		for (int i = open.Count() - 1; i >= 0; i--)
		{
			if (!newest && open[i].m_sCallerUid != entityName)
				continue;

			IEntity body = LF_CasualtyCalls.Body(open[i]);
			if (body)
				return body;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The reply text of a roll: uid, summary key, position and seconds left, for the proof to read.
	static string Describe(notnull LF_Casualty casualty, int nowMs)
	{
		int left = (casualty.m_iExpiresMs - nowMs) / 1000;
		return string.Format("%1 '%2' at %3 expires_in=%4", casualty.m_sCallerUid, casualty.m_sSummary, casualty.m_vPosition, left);
	}
}

#ifdef LF_HARNESS
//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "casualty")
			return super.Handle(cmd);

		if (cmd.Arg1 != "roll")
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "casualty roll [kind]");
			return true;
		}

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_character");
			return true;
		}

		// The server answers with the casualty it rolled, or the refusal.
		character.LF_AskCasualtyRoll(cmd.Id, cmd.Arg2);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	void LF_AskCasualtyRoll(int commandId, string kind)
	{
		if (Replication.IsServer())
		{
			LF_ApplyCasualtyRoll(commandId, kind, true);
			return;
		}

		Rpc(RpcAsk_LF_CasualtyRoll, commandId, kind);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_CasualtyRoll(int commandId, string kind)
	{
		// Compiled only in a LF_HARNESS build, where this check is open; the dev gate stays the one door.
		LF_ApplyCasualtyRoll(commandId, kind, LF_DevGate.CheckHarnessRequest(this, "casualty roll"));
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ApplyCasualtyRoll(int commandId, string kind, bool gateOpen)
	{
		if (!gateOpen)
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		int nowMs = System.GetTickCount();
		LF_Casualty casualty = LF_CasualtyCalls.RollNow(nowMs, kind);
		if (!casualty)
		{
			LF_HarnessReply(commandId, "error", "no_casualty_rolled (generator off, no medic on duty or no place of that kind; see the CasualtyCall log)");
			return;
		}

		LF_HarnessReply(commandId, "ok", LF_CasualtyHarness.Describe(casualty, nowMs));
	}
}
#endif
