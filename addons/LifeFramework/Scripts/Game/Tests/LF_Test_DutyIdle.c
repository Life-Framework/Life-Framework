// red-proof: review fix (#3132) 2026-11-18: 'an idle officer pauses although the trial sweep settles the clock each minute' failed on the old clock-start window (tmp/logs/test-20261118-171059.log) and passes with the last-active window. Original: observed 2026-11-18 with `test --no-build --red LF_Test_DutyIdle` (RED PROOF OK, log tmp/logs/test-20261118-170122.log): with the hook on the sampler never runs, so 'idle seconds past the window are not paid' (expected 460, got 660), the medic case (expected 400, got 600) and 'a fixture body that stands 310 s pauses the clock' fail. Green with the sampler.
//! police/duty-idle (#3120): an on-duty officer or medic who stands still past dutyIdleSeconds has
//! the duty clock paused, so the idle time is not paid; moving resumes it; 0 turns the pause off.
//! The rule is driven with explicit times (10 s samples); the WORLD part reads a fixture
//! character's pose, so a real body that moves counts as active and one that stands does not.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_DutyIdle : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/duty-idle";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(300, LF_DutyIdle.IdleSeconds(), "the shipped idle window is 300 s");

		// 600 s on duty, still from second 100: paid for 100 + 300 s.
		LF_PlayerAccount officer = OnDuty("duty-idle-officer", LF_Faction.POLICE);
		LF_DutyIdleWatch watch = new LF_DutyIdleWatch();
		Shift(officer, watch, 100, 600, 300);
		ctx.True(!officer.IsDutyClockRunning(), "an officer idle past the window has the clock paused");

		// Moving again resumes the clock: 60 more seconds count.
		Sample(officer, watch, 600000, Vector(500, 0, 0), 300);
		ctx.True(officer.IsDutyClockRunning(), "moving again resumes the duty clock");
		LF_PoliceRanks.OnDutyChangedAt(officer, false, 660000);
		ctx.Equal(460, LF_DutyPay.TakeShiftPay(officer, 1), "idle seconds past the window are not paid, active ones are");

		// A medic is paused the same way.
		LF_PlayerAccount medic = OnDuty("duty-idle-medic", LF_Faction.MEDIC);
		LF_DutyIdleWatch medicWatch = new LF_DutyIdleWatch();
		Shift(medic, medicWatch, 100, 600, 300);
		LF_PoliceRanks.OnDutyChangedAt(medic, false, 600000);
		ctx.Equal(400, medic.TakeUnpaidDutySeconds(), "a medic idle from second 100 is paid 100 + 300 s");

		// The setting at 0 pays all 600 s.
		LF_PlayerAccount off = OnDuty("duty-idle-off", LF_Faction.POLICE);
		LF_DutyIdleWatch offWatch = new LF_DutyIdleWatch();
		Shift(off, offWatch, 100, 600, 0);
		LF_PoliceRanks.OnDutyChangedAt(off, false, 600000);
		ctx.Equal(600, off.TakeUnpaidDutySeconds(), "dutyIdleSeconds 0 pays every second");

		// Other settles restart the clock (the trial sweep settles every officer each minute): an
		// officer idle from second 100 is still paused, at most one sample late.
		LF_PlayerAccount swept = OnDuty("duty-idle-swept", LF_Faction.POLICE);
		LF_DutyIdleWatch sweptWatch = new LF_DutyIdleWatch();
		for (int t = 0; t < 600; t += 10)
		{
			if (t > 0 && t % 60 == 0)
				LF_PoliceRanks.SettleDutyAt(swept, true, t * 1000);
			Sample(swept, sweptWatch, t * 1000, Vector(LF_Utils.MinInt(t, 100) * 3, 0, 0), 300);
		}
		ctx.True(!swept.IsDutyClockRunning(), "an idle officer pauses although the trial sweep settles the clock each minute");
		LF_PoliceRanks.OnDutyChangedAt(swept, false, 600000);
		int sweptPaid = swept.TakeUnpaidDutySeconds();
		ctx.True(sweptPaid <= 420, string.Format("an officer idle from second 100 under minute settles is paid at most 400 s plus one sample, got %1", sweptPaid));

		// Turning the view counts as active (a player at a desk or in a parked car looking out).
		ctx.True(LF_DutyIdle.IsActive(vector.Zero, vector.Zero, "1 0 0", "0 0 1"), "turning the view counts as active");
		ctx.True(!LF_DutyIdle.IsActive(vector.Zero, "1 0 0", "1 0 0", "1 0 0"), "1 m and the same view is idle");

		WorldCase(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! A real body: standing still reads idle, a 5 m move reads active.
	protected void WorldCase(LF_TestContext ctx)
	{
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		vector pos;
		vector aim;
		LF_DutyIdle.ReadPose(body, pos, aim);
		ctx.True(pos != vector.Zero, "the pose of a fixture character reads its position");

		LF_PlayerAccount account = OnDuty("duty-idle-body", LF_Faction.POLICE);
		LF_DutyIdleWatch watch = new LF_DutyIdleWatch();
		Sample(account, watch, 0, pos, 300);
		vector still;
		LF_DutyIdle.ReadPose(body, still, aim);
		Sample(account, watch, 310000, still, 300);
		ctx.True(!account.IsDutyClockRunning(), "a fixture body that stands 310 s pauses the clock");

		body.SetOrigin(pos + "5 0 0");
		vector moved;
		LF_DutyIdle.ReadPose(body, moved, aim);
		Sample(account, watch, 320000, moved, 300);
		ctx.True(account.IsDutyClockRunning(), "a fixture body moved 5 m resumes the clock");

		LF_PoliceRanks.OnDutyChangedAt(account, false, 320000);
		account.TakeUnpaidDutySeconds();
		LF_TestCharacterFixture.Delete(body);
	}

	//------------------------------------------------------------------------------------------------
	//! On duty at 0 s; samples every 10 s to endSeconds (exclusive), moving 1 m per sample until
	//! stillFrom, then standing.
	protected void Shift(LF_PlayerAccount account, LF_DutyIdleWatch watch, int stillFrom, int endSeconds, int idleSeconds)
	{
		for (int t = 0; t < endSeconds; t += 10)
		{
			int x = LF_Utils.MinInt(t, stillFrom) * 3;
			Sample(account, watch, t * 1000, Vector(x, 0, 0), idleSeconds);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Sample(LF_PlayerAccount account, LF_DutyIdleWatch watch, int nowMs, vector position, int idleSeconds)
	{
		// The act the assertions guard: with the hook on, the sampler never runs.
		if (!RedProof())
			LF_DutyIdle.SampleAt(account, watch, nowMs, position, "1 0 0", idleSeconds);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlayerAccount OnDuty(string uid, LF_Faction role)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		account.SetFaction(role);
		if (role == LF_Faction.POLICE)
			account.SetPoliceRank(LF_PoliceRanks.PATROL);
		account.SetOnDuty(false);
		account.StopDutyClock();
		account.TakeUnpaidDutySeconds();
		account.SetOnDuty(true);
		LF_PoliceRanks.OnDutyChangedAt(account, true, 0);
		return account;
	}
}
