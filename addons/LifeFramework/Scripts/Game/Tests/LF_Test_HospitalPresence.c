// red-proof: 2026-11-23 `tools/cli test --no-build --red LF_Test_HospitalPresence` printed `RED PROOF OK`: with RedProof() on the presence and lock refusals are skipped, so "0 medics and 1 officer refuses", the bank-lock and cooldown refusals fail.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HospitalPresence : LF_Test
{
	protected static const string CABINET_ID = "logic-hospital-presence-cabinet";
	protected static const string ROBBER_UID = "test-hospital-presence-robber";
	protected static const int NOW = 5000;

	override string GetName()
	{
		return "crime/hospital-presence";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_BigJobLock.ResetStatics();
		LF_HospitalPresence.ResetStatics();
		LF_BigJobLock.SetClockForTests(NOW);

		CheckPresence(ctx);
		CheckBankLock(ctx);
		CheckOpenTakesLock(ctx);

		LF_BigJobLock.ResetStatics();
		LF_HospitalPresence.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected string Refusal(int medics, int police)
	{
		LF_HospitalPresence.SetCountsForTests(medics, police);
		string why;
		if (RedProof())
			return string.Empty;

		return LF_HospitalPresence.RefusalKey(NOW, why);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPresence(LF_TestContext ctx)
	{
		ctx.EqualStr(LF_HospitalPresence.KEY_TOO_FEW, Refusal(0, 1), "0 medics and 1 officer refuses");
		ctx.EqualStr(LF_HospitalPresence.KEY_TOO_FEW, Refusal(0, 0), "nobody on duty refuses");
		ctx.EqualStr("", Refusal(1, 0), "1 medic allows");
		ctx.EqualStr("", Refusal(0, 2), "2 officers allow");
		ctx.False(LF_Terms.Text(LF_HospitalPresence.KEY_TOO_FEW) == LF_HospitalPresence.KEY_TOO_FEW, "the refusal has words in the string table");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBankLock(LF_TestContext ctx)
	{
		ctx.True(LF_BigJobLock.TryBegin("bank", "test-crew", LF_BigJobLock.COOLDOWN_BANK, NOW) == LF_EBigJobLock.OK, "the bank takes the big-job lock");
		ctx.EqualStr(LF_BigJobLock.RefusalKey(LF_EBigJobLock.BUSY), Refusal(1, 2), "with the lock held by bank the cabinet refuses");
		LF_BigJobLock.ResetStatics();
		LF_BigJobLock.SetClockForTests(NOW);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckOpenTakesLock(LF_TestContext ctx)
	{
		LF_Breach.Configure(CABINET_ID, 0, "");
		// No loot lines: this test is about the gate, and spawned loot would outlive it.
		array<ResourceName> noLoot = {};
		LF_Hospital.ConfigureCabinet(CABINET_ID, noLoot);

		LF_HospitalPresence.SetCountsForTests(0, 1);
		bool refused = true;
		if (!RedProof())
			refused = !LF_HospitalPresence.Breach(CABINET_ID, ROBBER_UID, null);
		ctx.True(refused, "the cabinet breach refuses with 0 medics and 1 officer");
		ctx.False(LF_Breach.IsOpen(CABINET_ID), "a refused cabinet stays shut");
		ctx.Equal(0, LF_Breach.GetPoliceNotifyCount(CABINET_ID), "a refused cabinet does not alert");

		LF_HospitalPresence.SetCountsForTests(1, 0);
		ctx.True(LF_HospitalPresence.Breach(CABINET_ID, ROBBER_UID, null), "with 1 medic the cabinet breach goes ahead");
		ctx.True(LF_Breach.IsOpen(CABINET_ID), "the cabinet is open");
		ctx.EqualStr(LF_HospitalPresence.JOB_KEY, LF_BigJobLock.RunningJob(NOW), "the opened cabinet holds the big-job lock as pharmacy");
		ctx.True(LF_BigJobLock.TryBegin("bank", "test-crew", LF_BigJobLock.COOLDOWN_BANK, NOW) == LF_EBigJobLock.BUSY, "the bank is refused while the pharmacy runs");

		LF_HospitalPresence.EndJob();
		ctx.EqualStr("", LF_BigJobLock.RunningJob(NOW), "the pharmacy job's end frees the lock");
		ctx.Equal(LF_BigJobLock.COOLDOWN_PHARMACY, LF_BigJobLock.CooldownLeft(LF_HospitalPresence.JOB_KEY, NOW), "the pharmacy cooldown is 2 h");
		ctx.Equal(7200, LF_HospitalPresenceConfig.Get().m_iCooldownSeconds, "the cooldown comes from the config (2 h)");
		ctx.EqualStr(LF_BigJobLock.RefusalKey(LF_EBigJobLock.COOLDOWN), Refusal(1, 2), "during its cooldown the cabinet refuses");
	}
}
