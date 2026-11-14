// red-proof: TODO
// tier: LOGIC
//! Disconnect rules (#691), the pure decision per state and the owner "disconnect" section:
//! cuffed jails (or fines, or releases), escorted counts as cuffed, jail falls back to the fine
//! with no cell, downed dies (not when stunned), the warrant and the sentence stay.
class LF_Test_DisconnectRules : LF_Test
{
	override string GetName()
	{
		return "disconnect/rules";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_DisconnectConfig config = LF_DisconnectConfig.CreateDefault();

		// Cuffed, jail rule, a cell placed: jail with the leaving-custody charge.
		LF_DisconnectState cuffed = State(true, false, false, false, 0, false, true);
		LF_DisconnectDecision d = LF_DisconnectRules.Decide(cuffed, config);
		ctx.EqualStr(LF_DisconnectConfig.RULE_JAIL, d.m_sCustody, "cuffed at logout is jail by default");
		ctx.Equal(1, d.m_iWantedAdd, "leaving custody adds the wanted charge");
		ctx.Equal(300, d.m_iMinJailSeconds, "the jail rule carries the shortest sentence");
		ctx.True(d.m_bKeepWarrant, "the added charge is a warrant that stays");
		ctx.False(d.m_bDownedDies, "a standing cuffed player does not die");

		// No cell: the fine instead, said so.
		cuffed.m_bCellPlaced = false;
		d = LF_DisconnectRules.Decide(cuffed, config);
		ctx.EqualStr(LF_DisconnectConfig.RULE_FINE, d.m_sCustody, "with no cell the jail rule falls back to the fine");
		ctx.True(d.m_bJailFellBack, "the fallback is recorded");
		ctx.Equal(1000, d.m_iFine, "the fallback fine is the configured one");

		// Fine rule.
		config.m_sCuffedRule = LF_DisconnectConfig.RULE_FINE;
		cuffed.m_bCellPlaced = true;
		d = LF_DisconnectRules.Decide(cuffed, config);
		ctx.EqualStr(LF_DisconnectConfig.RULE_FINE, d.m_sCustody, "the fine rule fines");
		ctx.False(d.m_bJailFellBack, "a chosen fine is no fallback");
		ctx.Equal(1, d.m_iWantedAdd, "the fine rule also adds the charge");

		// Release rule: no penalty.
		config.m_sCuffedRule = LF_DisconnectConfig.RULE_RELEASE;
		d = LF_DisconnectRules.Decide(cuffed, config);
		ctx.EqualStr(LF_DisconnectConfig.RULE_RELEASE, d.m_sCustody, "the release rule releases");
		ctx.Equal(0, d.m_iWantedAdd, "release adds no charge");

		// Escorted on foot counts as cuffed, unless the owner says not.
		config.m_sCuffedRule = LF_DisconnectConfig.RULE_JAIL;
		LF_DisconnectState escorted = State(false, true, false, false, 2, false, true);
		d = LF_DisconnectRules.Decide(escorted, config);
		ctx.EqualStr(LF_DisconnectConfig.RULE_JAIL, d.m_sCustody, "escorted counts as cuffed");
		config.m_bEscortedCountsAsCuffed = false;
		d = LF_DisconnectRules.Decide(escorted, config);
		ctx.EqualStr("", d.m_sCustody, "escorted alone is no custody when the owner turns it off");
		ctx.True(d.m_bKeepWarrant, "the escorted wanted player keeps the warrant");
		config.m_bEscortedCountsAsCuffed = true;

		// A serving prisoner who is somehow cuffed keeps the sentence and gets no second one.
		LF_DisconnectState jailed = State(true, false, false, false, 1, true, true);
		d = LF_DisconnectRules.Decide(jailed, config);
		ctx.EqualStr("", d.m_sCustody, "a serving prisoner gets no custody rule");
		ctx.True(d.m_bKeepSentence, "the sentence stays");

		// Downed: dies, not when stunned, not when the owner turns it off.
		LF_DisconnectState downed = State(false, false, true, false, 0, false, true);
		ctx.True(LF_DisconnectRules.Decide(downed, config).m_bDownedDies, "downed at logout dies");
		downed.m_bStunned = true;
		ctx.False(LF_DisconnectRules.Decide(downed, config).m_bDownedDies, "a stunned player never dies of a logout");
		downed.m_bStunned = false;
		config.m_bDownedDies = false;
		ctx.False(LF_DisconnectRules.Decide(downed, config).m_bDownedDies, "downedDies off: the downed logout lives");
		config.m_bDownedDies = true;

		// Wanted only: the warrant stays, plus the configured add.
		LF_DisconnectState wanted = State(false, false, false, false, 3, false, true);
		d = LF_DisconnectRules.Decide(wanted, config);
		ctx.True(d.m_bKeepWarrant, "a wanted logout keeps the warrant");
		ctx.Equal(0, d.m_iWantedAdd, "no add by default");
		config.m_iWantedLogoutAdd = 2;
		ctx.Equal(2, LF_DisconnectRules.Decide(wanted, config).m_iWantedAdd, "wantedLogoutAdd raises it");

		// Clean: nothing flagged.
		ctx.False(LF_DisconnectRules.Decide(State(false, false, false, false, 0, false, true), config).IsFlagged(), "a clean logout is not flagged");

		// The sentence for leaving custody is never below the floor.
		ctx.Equal(300, LF_DisconnectRules.JailSeconds(60, 300), "the floor lifts a short sentence");
		ctx.Equal(900, LF_DisconnectRules.JailSeconds(900, 300), "a longer wanted sentence stays");

		// The owner section.
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"disconnect\":{\"cuffedRule\":\"FINE\",\"cuffedFine\":250,\"downedDies\":false,\"jailClockOffline\":true}}", "test");
		LF_DisconnectConfig.Reload();
		LF_DisconnectConfig owner = LF_DisconnectConfig.Get();
		ctx.EqualStr(LF_DisconnectConfig.RULE_FINE, owner.m_sCuffedRule, "the owner's cuffed rule applies");
		ctx.Equal(250, owner.m_iCuffedFine, "the owner's fine applies");
		ctx.False(LF_DisconnectRules.DownedDies(), "the owner's downedDies applies");
		ctx.True(LF_DisconnectRules.JailClockOffline(), "the owner's jail clock applies");

		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"disconnect\":{\"cuffedRule\":\"vanish\"}}", "test");
		LF_DisconnectConfig.Reload();
		ctx.EqualStr(LF_DisconnectConfig.RULE_JAIL, LF_DisconnectConfig.Get().m_sCuffedRule, "an unknown rule keeps the shipped jail");

		LF_ConfigService.Reset();
		LF_DisconnectConfig.Reload();
		ctx.EqualStr(LF_DisconnectConfig.RULE_JAIL, LF_DisconnectConfig.Get().m_sCuffedRule, "the shipped rule is jail");
		ctx.True(LF_DisconnectRules.DownedDies(), "the shipped downed logout dies");
		ctx.False(LF_DisconnectRules.JailClockOffline(), "the shipped sentence counts online time only");
	}

	protected static LF_DisconnectState State(bool cuffed, bool escorted, bool downed, bool stunned, int wanted, bool serving, bool cell)
	{
		LF_DisconnectState state = new LF_DisconnectState();
		state.m_sPlayerUid = "disconnect-rules-uid";
		state.m_bCuffed = cuffed;
		state.m_bEscorted = escorted;
		state.m_bDowned = downed;
		state.m_bStunned = stunned;
		state.m_iWanted = wanted;
		state.m_bServing = serving;
		state.m_bCellPlaced = cell;
		return state;
	}
}
