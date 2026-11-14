// red-proof: Observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_DisconnectJail`: `if (false)` in place of the ImprisonOnReturn call in LF_DisconnectRules.Apply failed `- the cuffed logout is jailed (expected 'jail', got 'fine')`; `if (false)` in place of `!online && clockOffline` in LF_Jail.TickOnline failed `- jailClockOffline: the away prisoner serves time (expected 30, got 50)`. Restored; green.
// tier: WORLD (needs DebugWorld's placed cell to restore after the stand-in cell)
//! Disconnect rules (#691) on the server path with no body: a cuffed logout starts a paused
//! sentence and adds the charge; the rejoin serves it; the fine rule takes the fine from the bank;
//! jailClockOffline lets an offline prisoner serve time; a flagged logout writes an incident row.
class LF_Test_DisconnectJail : LF_Test
{
	protected static const string JAILED = "disconnect-jail-uid";
	protected static const string FINED = "disconnect-fine-uid";
	protected static const string AWAY = "disconnect-away-uid";

	override string GetName()
	{
		return "disconnect/jail";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_JailRecord> before = LF_Jail.ExportAll();
		LF_Jail.Configure(60, 600, 1, 5);
		LF_Jail.SetCellOrigin("48 0 64");
		LF_IncidentLog.Install();
		LF_ConfigService.Reset();
		LF_DisconnectConfig.Reload();

		// Jail rule: wanted 2 + 1 for leaving custody is 180 s at 60 s a level; the 300 s floor wins.
		LF_PlayerAccount jailed = LF_PlayerAccountManager.GetOrCreate(JAILED);
		jailed.SetWantedLevel(2);
		LF_DisconnectDecision d = LF_DisconnectRules.Apply(Cuffed(JAILED, 2), null);
		ctx.EqualStr(LF_DisconnectConfig.RULE_JAIL, d.m_sCustody, "the cuffed logout is jailed");
		ctx.Equal(3, jailed.GetWantedLevel(), "leaving custody adds one wanted level");
		ctx.True(LF_Jail.IsServing(JAILED), "the sentence starts at logout");
		ctx.EqualFloat(300, LF_Jail.GetRemaining(JAILED), 0.01, "the sentence is the 300 s floor");
		array<string> nobody = {};
		LF_Jail.TickOnline(100, nobody);
		ctx.EqualFloat(300, LF_Jail.GetRemaining(JAILED), 0.01, "away, the sentence waits");
		array<string> back = {JAILED};
		LF_Jail.TickOnline(30, back);
		ctx.EqualFloat(270, LF_Jail.GetRemaining(JAILED), 0.01, "the rejoin serves it");

		array<ref LF_IncidentRecord> rows = LF_IncidentLog.Query(vector.Zero, -1, 0, -1, LF_IncidentConfig.KIND_LOGOUT);
		bool logged;
		foreach (LF_IncidentRecord row : rows)
		{
			if (row.m_sSubjectUid == JAILED && row.m_iAmount == 300 && row.m_sHow.Contains("cuffed"))
				logged = true;
		}
		ctx.True(logged, "the incident log has the logout with the state and the sentence");

		// A second logout while serving: no second sentence.
		d = LF_DisconnectRules.Apply(Cuffed(JAILED, 3), null);
		ctx.True(d.m_bKeepSentence, "a serving prisoner keeps the sentence");
		ctx.EqualFloat(270, LF_Jail.GetRemaining(JAILED), 0.01, "and gets no new one");

		// Fine rule.
		LF_DisconnectConfig fineConfig = LF_DisconnectConfig.CreateDefault();
		fineConfig.m_sCuffedRule = LF_DisconnectConfig.RULE_FINE;
		fineConfig.m_iCuffedFine = 400;
		fineConfig.m_bCuffedFineAllowsDebt = false;
		LF_DisconnectConfig.Use(fineConfig);
		LF_ATMManager.GetOrCreate(FINED).SetBalance(250);
		LF_PlayerAccountManager.GetOrCreate(FINED).SetWantedLevel(0);
		d = LF_DisconnectRules.Apply(Cuffed(FINED, 0), null);
		ctx.EqualStr(LF_DisconnectConfig.RULE_FINE, d.m_sCustody, "the fine rule fines");
		ctx.Equal(0, LF_ATMManager.GetOrCreate(FINED).GetBalance(), "without debt the fine takes what the bank holds");
		ctx.False(LF_Jail.IsServing(FINED), "a fine is no sentence");
		ctx.Equal(1, LF_PlayerAccountManager.GetOrCreate(FINED).GetWantedLevel(), "the fine rule adds the charge");

		// Offline clock.
		fineConfig.m_bJailClockOffline = true;
		ctx.True(LF_Jail.ImprisonOnReturn(AWAY, 50, 1), "a sentence on return starts");
		LF_Jail.TickOnline(20, nobody);
		ctx.EqualFloat(30, LF_Jail.GetRemaining(AWAY), 0.01, "jailClockOffline: the away prisoner serves time");
		LF_Jail.TickOnline(40, nobody);
		ctx.False(LF_Jail.IsServing(AWAY), "and is released away");

		LF_DisconnectConfig.Reload();
		LF_Jail.ApplyAll(before);
		LF_Jail.SyncPlacedCell();
	}

	protected static LF_DisconnectState Cuffed(string uid, int wanted)
	{
		LF_DisconnectState state = new LF_DisconnectState();
		state.m_sPlayerUid = uid;
		state.m_bCuffed = true;
		state.m_iWanted = wanted;
		state.m_bServing = LF_Jail.IsServing(uid);
		state.m_bCellPlaced = LF_Jail.HasCell();
		return state;
	}
}
