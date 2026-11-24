// Harness verb `casualty roll [kind]` (#4853) and the stand name `Casualty` (#4853). A played medic hour
// cannot wait one roll interval (1200 s on Everon) for a casualty, so the verb rolls one now: it skips the
// interval and the open cap, still needs a medic on duty, and answers with the new casualty's caller uid,
// row kind, position and expiry. `stand Casualty` (or `stand casualty:<n>`) walks the character to that
// body itself, so the revive hold ("Treat the patient") is in reach; a place entity only gets near it.
// `casualty ambulance <Entity> [metres]` sets the ambulance the medic drew at the clinic car pool that many
// metres (default 4) beside the named entity (a casualty, a bed): the harness cannot drive, so a played carry
// parks the car where the drive would have ended.
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
//! Client side of `casualty check`: what this client reads of the nearest other character, and what the revive offer says.
modded class LF_CasualtyHarness
{
	protected static IEntity s_pNear;
	protected static IEntity s_pSelf;

	protected static bool CollectNear(IEntity entity)
	{
		if (entity != s_pSelf && SCR_ChimeraCharacter.Cast(entity))
			s_pNear = entity;

		return true;
	}

	static string CheckNearest()
	{
		s_pSelf = LF_Utils.GetLocalCharacter();
		s_pNear = null;
		if (!s_pSelf || !GetGame().GetWorld())
			return "no_character";

		GetGame().GetWorld().QueryEntitiesBySphere(s_pSelf.GetOrigin(), 6, CollectNear, null, EQueryEntitiesFlags.ALL);
		if (!s_pNear)
			return "no other character within 6 m";

		bool onDuty = LF_MedicDuty.IsOnDutyState(LF_AccountReplication.GetState(s_pSelf));
		LF_EReviveRefusal refusal = LF_Revive.Check(s_pSelf, s_pNear, onDuty);
		// casualtyFlag is the replicated flag on the body (#4848); labelled is what the Revive action shows: "Treat the patient" when 1.
		bool flag;
		SCR_ChimeraCharacter nearCharacter = SCR_ChimeraCharacter.Cast(s_pNear);
		if (nearCharacter)
			flag = nearCharacter.LF_IsCasualty();
		return string.Format("dist=%1 downed=%2 onDuty=%3 medicEnabled=%4 refusal=%5 casualtyFlag=%6 labelled=%7", vector.Distance(s_pSelf.GetOrigin(), s_pNear.GetOrigin()), LF_Downed.IsDowned(s_pNear), onDuty, LF_Systems.IsEnabled("Medic"), typename.EnumToString(LF_EReviveRefusal, refusal), flag, LF_CasualtyTreat.LooksLikeCasualty(s_pNear));
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "casualty")
			return super.Handle(cmd);

		if (cmd.Arg1 == "check")
		{
			SCR_ChimeraCharacter me = LF_Utils.GetLocalCharacter();
			if (me)
				me.LF_AskCasualtyCheck(cmd.Id, LF_CasualtyHarness.CheckNearest());
			else
				LF_AgentHarness.Answer(cmd.Id, "error", "no_character");
			return true;
		}

		if (cmd.Arg1 != "roll" && cmd.Arg1 != "ambulance")
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "casualty roll [kind] | casualty ambulance <Entity> [metres]");
			return true;
		}

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_character");
			return true;
		}

		// The server answers with the casualty it rolled, or the refusal.
		if (cmd.Arg1 == "ambulance")
			character.LF_AskCasualtyAmbulance(cmd.Id, cmd.Arg2, cmd.Arg3);
		else
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
	void LF_AskCasualtyCheck(int commandId, string clientSide)
	{
		if (Replication.IsServer())
		{
			RpcAsk_LF_CasualtyCheck(commandId, clientSide);
			return;
		}

		Rpc(RpcAsk_LF_CasualtyCheck, commandId, clientSide);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_CasualtyCheck(int commandId, string clientSide)
	{
		IEntity near = LF_CasualtyHarness.Named("Casualty");
		string server = "no casualty";
		if (near)
		{
			bool stateDuty = LF_MedicDuty.IsOnDutyState(LF_AccountReplication.GetState(this));
			bool accountDuty;
			LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
			if (accounts)
				accountDuty = LF_MedicDuty.IsOnDuty(accounts.GetAccount(LF_Utils.GetPlayerUID(this)));

			LF_EReviveRefusal refusal = LF_Revive.Check(this, near, stateDuty);
			server = string.Format("dist=%1 downed=%2 stateDuty=%3 accountDuty=%4 refusal=%5", vector.Distance(GetOrigin(), near.GetOrigin()), LF_Downed.IsDowned(near), stateDuty, accountDuty, typename.EnumToString(LF_EReviveRefusal, refusal));
		}

		LF_HarnessReply(commandId, "ok", "CLIENT " + clientSide + " | SERVER " + server);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskCasualtyAmbulance(int commandId, string entityName, string metres)
	{
		if (Replication.IsServer())
		{
			LF_ApplyCasualtyAmbulance(commandId, entityName, metres, true);
			return;
		}

		Rpc(RpcAsk_LF_CasualtyAmbulance, commandId, entityName, metres);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_CasualtyAmbulance(int commandId, string entityName, string metres)
	{
		LF_ApplyCasualtyAmbulance(commandId, entityName, metres, LF_DevGate.CheckHarnessRequest(this, "casualty ambulance"));
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ApplyCasualtyAmbulance(int commandId, string entityName, string metres, bool gateOpen)
	{
		if (!gateOpen)
		{
			LF_HarnessReply(commandId, "error", LF_AgentHarness.REFUSED_DEV_GATE);
			return;
		}

		IEntity car = LF_MedicCarPool.CarOf(LF_Utils.GetPlayerUID(this));
		IEntity anchor = LF_AgentHarness.ResolveNamed(entityName);
		if (!car || !anchor)
		{
			LF_HarnessReply(commandId, "error", "no_ambulance_or_entity (draw one at the car pool first)");
			return;
		}

		float distance = 4;
		if (!metres.IsEmpty())
			distance = metres.ToFloat();

		vector previous = car.GetOrigin();
		vector transform[4];
		anchor.GetWorldTransform(transform);
		vector spot = transform[3] + transform[0] * distance;
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]) + 0.5;
		transform[3] = spot;
		car.SetWorldTransform(transform);
		car.Update();
		// A replicated entity moved far on the server stays where it was for the clients unless its node is told.
		RplComponent rpl = RplComponent.Cast(car.FindComponent(RplComponent));
		if (rpl)
			rpl.ForceNodeMovement(previous);

		Physics physics = car.GetPhysics();
		if (physics)
		{
			physics.SetVelocity(vector.Zero);
			physics.SetAngularVelocity(vector.Zero);
			physics.SetActive(ActiveState.ACTIVE);
		}

		LF_Debug.Info(LF_CasualtyCalls.FEATURE, string.Format("harness: the medic's ambulance set %1 m beside %2 at %3", distance, entityName, spot));
		LF_HarnessReply(commandId, "ok", string.Format("ambulance at %1", spot));
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
