// red-proof: expected wanted 0 after Record and ran `tools\cli test --no-build --only EL_Test_Murder`
// (2026-11-08). The suite reported
// `[ELTEST]   - the murder raises wanted once by the configured amount (expected 0, got 3)`.
// Restored the expected value to 3.

// tier: LOGIC
class EL_Test_Murder : EL_Test
{
	override string GetName()
	{
		return "crime/murder";
	}

	override void Run(EL_TestContext ctx)
	{
		// The rule: who counts as a murderer.
		ctx.EqualStr("", LF_Murder.SkipReason(1, 2, true, false, false, 0, true), "a player killing another player is murder");
		ctx.EqualStr("", LF_Murder.SkipReason(1, 0, true, false, false, 0, true), "a player killing an NPC is murder");
		ctx.EqualStr(LF_Murder.SKIP_SELF, LF_Murder.SkipReason(1, 1, true, false, false, 0, true), "a self-kill by the same player id is not murder");
		ctx.EqualStr(LF_Murder.SKIP_SELF, LF_Murder.SkipReason(1, 0, true, true, false, 0, true), "a body that killed itself is not murder");
		ctx.EqualStr(LF_Murder.SKIP_NO_KILLER, LF_Murder.SkipReason(0, 2, true, false, false, 0, true), "a death with no player behind it is not murder");
		ctx.EqualStr(LF_Murder.SKIP_NO_KILLER, LF_Murder.SkipReason(-1, 0, true, false, false, 0, true), "an NPC killing an NPC is not murder");
		ctx.EqualStr(LF_Murder.SKIP_NOT_CHARACTER, LF_Murder.SkipReason(1, 0, false, false, false, 0, true), "a destroyed vehicle is not murder");
		ctx.EqualStr(LF_Murder.SKIP_OFFICER_ON_WANTED, LF_Murder.SkipReason(1, 2, true, false, true, 2, true), "an on-duty officer killing a wanted suspect is exempt");
		ctx.EqualStr("", LF_Murder.SkipReason(1, 2, true, false, true, 0, true), "an on-duty officer killing someone not wanted is murder");
		ctx.EqualStr("", LF_Murder.SkipReason(1, 2, true, false, true, 2, false), "with the exemption off an officer kill is murder");
		ctx.EqualStr("", LF_Murder.SkipReason(1, 2, true, false, false, 2, true), "a civilian killing a wanted player is still murder");

		// The config defaults (LF_Police.conf).
		ctx.Equal(3, LF_Murder.WantedAmount(), "a murder adds 3 wanted levels");
		ctx.True(LF_Murder.OfficerExempt(), "the officer exemption is on by default");

		// The record: wanted once, one report with the murder offence, no name on it.
		string killerUid = "logic-murder-killer";
		EL_PlayerAccount killer = EL_PlayerAccountManager.GetOrCreate(killerUid);
		ctx.NotNull(killer, "killer account exists");
		if (!killer)
			return;

		killer.SetWantedLevel(0);
		int recordedBefore = LF_Murder.GetRecordedCount();
		int reportsBefore = LF_WantedReports.CountFor(killerUid);

		ctx.True(LF_Murder.Record(killerUid, null, "npc"), "a murder is recorded");
		ctx.Equal(3, killer.GetWantedLevel(), "the murder raises wanted once by the configured amount");
		ctx.Equal(recordedBefore + 1, LF_Murder.GetRecordedCount(), "one murder counted");
		ctx.Equal(reportsBefore + 1, LF_WantedReports.CountFor(killerUid), "one crime report filed");

		array<ref LF_WantedReportRecord> reports = LF_WantedReports.GetFor(killerUid);
		if (reports && reports.Count() > 0)
		{
			LF_WantedReportRecord report = reports.Get(reports.Count() - 1);
			ctx.EqualStr(LF_WantedReports.OFFENCE_MURDER, report.m_sOffence, "the report's offence is murder");
			ctx.False(report.m_bIdentified, "the report does not name the killer");
		}

		ctx.False(LF_Murder.Record("", null, "npc"), "a killer with no uid is refused");
		ctx.Equal(recordedBefore + 1, LF_Murder.GetRecordedCount(), "a refused record does not count");

		// A death that is not murder through the server entry leaves wanted alone.
		ctx.False(LF_Murder.OnCharacterKilled(null, null, 0, 0), "a death with no victim and no killer is not murder");
		ctx.Equal(3, killer.GetWantedLevel(), "a non-murder death does not raise wanted");

		killer.SetWantedLevel(0);
	}
}
