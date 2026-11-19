// red-proof: 2026-11-19 `tools\cli test --no-build --red LF_Test_CrewRob`: RED PROOF OK. With the hook on the holder's TakeMoney is skipped, so `the third member is paid a share (expected 50, got 0)`, `the job is over (expected 0, got 1)`. Green without the hook.
//! A crew robbery of a till (#3404). Three fixture characters start a job, the holder holds the
//! till for 30 s and takes the money: each of the three is paid a share, once. The holder steps
//! off the spot and the take pauses. A lone player at a crew till is refused.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CrewRob : LF_Test
{
	override string GetName()
	{
		return "crime/crew-rob";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_CrewRobMember Member(LF_TestContext ctx, IEntity body, string uid)
	{
		ctx.NotNull(LF_PlayerAccountManager.GetOrCreate(uid), "a civilian account exists for " + uid);
		return LF_CrewRobMember.Make(body, uid);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ATMManager.Reset();
		LF_PlayerAccountManager.Reset();
		LF_CrewRob.ResetStatics();

		ctx.True(Replication.IsServer(), "crew robbery test runs on the server");

		IEntity holder = LF_TestCharacterFixture.Spawn(ctx);
		IEntity second = LF_TestCharacterFixture.Spawn(ctx);
		IEntity third = LF_TestCharacterFixture.Spawn(ctx);
		if (!holder || !second || !third)
		{
			LF_TestCharacterFixture.Delete(holder);
			LF_TestCharacterFixture.Delete(second);
			LF_TestCharacterFixture.Delete(third);
			return;
		}

		LF_RobAction till = new LF_RobAction();
		vector spot = holder.GetOrigin();
		const int POLICE = 99; // the till's officer minimum is the till's own business, not this test's

		array<ref LF_CrewRobMember> crew = {};
		crew.Insert(Member(ctx, holder, "test-crew-holder"));
		crew.Insert(Member(ctx, second, "test-crew-second"));
		crew.Insert(Member(ctx, third, "test-crew-third"));

		// A lone player at a crew till is refused, and nothing starts.
		array<ref LF_CrewRobMember> lone = {};
		lone.Insert(crew[0]);
		string loneRefusal = LF_CrewRob.Start(till, spot, lone, POLICE);
		ctx.EqualStr(LF_CrewRob.REFUSED_CREW, loneRefusal, "a lone player is refused");
		ctx.Equal(0, LF_CrewRob.ActiveCount(), "a refused start leaves no job");

		// Three start: one job, the crime booked on each, a second start at the same till is refused.
		string started = LF_CrewRob.Start(till, spot, crew, POLICE);
		ctx.EqualStr("", started, "a crew of three starts the job");
		ctx.Equal(1, LF_CrewRob.ActiveCount(), "one job runs");
		LF_PlayerAccount holderAccount = LF_PlayerAccountManager.GetOrCreate("test-crew-holder");
		if (holderAccount)
			ctx.Equal(1, holderAccount.GetWantedLevel(), "the starter is wanted");

		ctx.EqualStr(LF_CrewRob.REFUSED_BUSY, LF_CrewRob.Start(till, spot, crew, POLICE), "a second job at the same till is refused");

		LF_CrewRobJob job = LF_CrewRob.JobOf(till);
		ctx.NotNull(job, "the till has its job");
		if (!job)
		{
			Cleanup(holder, second, third);
			return;
		}

		// Nobody holds yet: the clock runs but the take does not grow.
		LF_CrewRob.Step(job, 10);
		ctx.Equal(0, job.Take(), "the take does not grow before the till is held");

		// Only the holder holds the till.
		ctx.EqualStr(LF_CrewRob.REFUSED_NOT_HOLDER, LF_CrewRob.Hold(till, crew[1]), "a lookout cannot hold the till");
		ctx.EqualStr("", LF_CrewRob.Hold(till, crew[0]), "the holder holds the till");

		// The holder holds 30 s on the spot: the take grows at the configured rate, up to the cap.
		LF_CrewRobConfig config = LF_CrewRobConfig.Get();
		int expected = LF_CrewRobRules.Take(30, config.m_iTakePerSecond, config.m_iTakeCap);
		LF_CrewRob.Step(job, 30);
		ctx.Equal(expected, job.Take(), "30 s on the spot grows the take");
		ctx.True(expected > 0, "the take is more than nothing");

		// The holder steps off: the take pauses.
		holder.SetOrigin(spot + "0 0 100");
		LF_CrewRob.Step(job, 20);
		ctx.Equal(expected, job.Take(), "off the spot the take pauses");
		holder.SetOrigin(spot);
		LF_CrewRob.Step(job, 0);

		// A lookout cannot take the money; the take is still there.
		ctx.EqualStr(LF_CrewRob.REFUSED_NOT_HOLDER, LF_CrewRob.TakeMoney(till, crew[2]), "a lookout cannot take the money");
		ctx.Equal(1, LF_CrewRob.ActiveCount(), "the job still runs after a refused take");

		// The holder takes: three payouts, once.
		if (!RedProof())
			ctx.EqualStr("", LF_CrewRob.TakeMoney(till, crew[0]), "the holder takes the money");

		array<int> shares = {};
		LF_CrewRobRules.Split(expected, 3, shares);
		ctx.Equal(expected, LF_CrewRob.LastPaid(), "the whole take is paid");
		ctx.Equal(shares[0], LF_MoneyUtils.GetCash(holder), "the holder is paid a share");
		ctx.Equal(shares[1], LF_MoneyUtils.GetCash(second), "the second member is paid a share");
		ctx.Equal(shares[2], LF_MoneyUtils.GetCash(third), "the third member is paid a share");
		ctx.Equal(0, LF_CrewRob.ActiveCount(), "the job is over");

		// Once: a second take pays nothing more.
		ctx.EqualStr(LF_CrewRob.REFUSED_NO_JOB, LF_CrewRob.TakeMoney(till, crew[0]), "a finished job takes no second payout");
		ctx.Equal(shares[0], LF_MoneyUtils.GetCash(holder), "no second payout reached the holder");

		// The lookout's warning is once per approach.
		ctx.True(LF_CrewRob.WarnLookouts(true, false), "an officer arriving warns the lookouts");
		ctx.False(LF_CrewRob.WarnLookouts(true, true), "an officer who stays does not warn again");
		ctx.False(LF_CrewRob.WarnLookouts(false, false), "no officer, no warning");

		Cleanup(holder, second, third);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity a, IEntity b, IEntity c)
	{
		LF_CrewRob.ResetStatics();
		LF_TestCharacterFixture.Delete(a);
		LF_TestCharacterFixture.Delete(b);
		LF_TestCharacterFixture.Delete(c);
	}
}
