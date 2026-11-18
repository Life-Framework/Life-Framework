//------------------------------------------------------------------------------------------------
//! A job offer the player's standing does not reach is greyed on a dedicated client too (#2915,
//! docs/architecture/reputation.md section 4). Standing lives on the server, but the owner's own tiers
//! reach the client (LF_ReputationSync.ClientTier, #2704), so the client reads the gate row and its
//! own tier and greys the offer with the reason in words before the player presses it.
//!
//! Only the client branch lives here: a host and a server already read the points in the action.
//! Before the first tiers arrive the offer shows; the server decides again on use.
//------------------------------------------------------------------------------------------------
class LF_ReputationOfferGrey
{
	//------------------------------------------------------------------------------------------------
	//! Client: the refusal for a gate id from the tiers this client last received.
	//! \param[out] tierWord The `#` key of the gate's minimum tier word; empty when none or not shown.
	//! \return "" when the gate is open for this client (no row, a faction not in the config, no
	//! tiers received yet, or the tier reaches the minimum), else the faction's refusal `#` key.
	static string ClientRefusal(string gateId, out string tierWord)
	{
		tierWord = string.Empty;

		LF_ReputationGateDef gate = LF_Reputation.FindGate(gateId);
		if (!gate)
			return string.Empty;

		LF_ReputationFactionDef def = LF_Reputation.FactionDef(gate.m_sFaction);
		if (!def)
			return string.Empty;

		int tier;
		if (!LF_ReputationSync.ClientTier(gate.m_sFaction, tier))
			return string.Empty;

		if (tier >= gate.m_iMinTier)
			return string.Empty;

		if (def.m_bShown)
			tierWord = LF_Reputation.TierWordKey(def, gate.m_iMinTier);

		return LF_Reputation.RefusalKey(gate.m_sFaction);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when this client's tiers reach the gate (or are not known yet).
	static bool ClientPasses(string gateId)
	{
		string tierWord;
		return ClientRefusal(gateId, tierWord).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The greyed reason as player text, empty when the gate is open for this client.
	static string ClientText(string gateId)
	{
		string tierWord;
		string refusal = ClientRefusal(gateId, tierWord);
		if (refusal.IsEmpty())
			return string.Empty;

		return LF_Reputation.RefusalText(refusal, tierWord);
	}
}

//------------------------------------------------------------------------------------------------
//! The sign-up counter greys on a dedicated client when the town does not know the player yet.
modded class LF_JobSignUpAction
{
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		if (Replication.IsServer())
			return true;

		string text = LF_ReputationOfferGrey.ClientText(LF_JobStandingGate.GateId("job", m_eJob));
		if (text.IsEmpty())
			return true;

		SetCannotPerformReason(text);
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The run start greys on a dedicated client the same way, after the sign-up check.
modded class LF_JobStartRunAction
{
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		if (Replication.IsServer())
			return true;

		string text = LF_ReputationOfferGrey.ClientText(LF_JobStandingGate.GateId("run", m_eJob));
		if (text.IsEmpty())
			return true;

		SetCannotPerformReason(text);
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Session proof verb `standing <town> <police> [gate]` (runs on the client): feeds this client the tiers the
//! owner RPC would carry, so a joined client's greyed offer can be looked at without earning the loss.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "standing")
			return super.Handle(cmd);

		if (cmd.Arg1.IsEmpty() || cmd.Arg2.IsEmpty())
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: standing <town tier> <police tier> [gate id]");
			return true;
		}

		LF_ReputationSync.ReceiveOnClient(LF_ReputationSync.Pack(cmd.Arg1.ToInt(), cmd.Arg2.ToInt(), 0));

		// Optional gate id: a town gate at tier 1 on this client's config only (a level-1 counter such as
		// job:PARCEL, so the picture shows the standing words and not the job-level words).
		LF_ReputationConfig config = LF_ReputationConfig.Get();
		if (!cmd.Arg3.IsEmpty() && config)
		{
			if (!config.m_aGates)
				config.m_aGates = {};

			config.m_aGates.Insert(LF_ReputationGateDef.Make(cmd.Arg3, LF_Reputation.FACTION_TOWN, 1));
		}

		LF_AgentHarness.Answer(cmd.Id, "ok", "client tiers set");
		return true;
	}
}
