// red-proof: 2026-11-17, `tools\cli test --no-build --red LF_Test_JobRunRules,LF_Test_JobRunCargoWorld`: RED-OK. RedProof() skips the
//   BeginRun of the stash loop, so the deposit is not taken, no run opens, and every later check fails.
//   Unfixed code (#2408): BeginRun did not exist; the start action gave the cargo with no deposit and Dropoff paid at any distance.

//------------------------------------------------------------------------------------------------
// tier: WORLD
//! jobs/run-cargo-deposit (#2408): a real character starts, abandons and delivers runs at the placed drop. A run
//! takes the cargo's deposit from the player's cash (a player short of it gets no run and no cargo), an abandoned
//! run keeps it, and a delivery pays the run and returns it. A drop too close to the start, or reached too fast,
//! or not a placed drop at all, pays nothing and leaves the run, the cargo and the deposit where they were.
class LF_Test_JobRunCargoWorld : LF_Test
{
	protected static const string UID = "job-run-cargo-world";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/run-cargo-deposit";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity drop = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.JOB_DROP);
		IEntity depot = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.JOB_DEPOT);
		ctx.NotNull(drop, "Annex_Job_Drop is placed");
		ctx.NotNull(depot, "Annex_Job_Depot is placed");
		if (!drop || !depot)
			return;

		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		if (!user)
			return;

		LF_JobsConfig.Reload();
		LF_CivilianJob.ForgetCareer(UID);
		RunStash(ctx, user, drop);
		RunDrops(ctx, user, drop, depot);
		LF_CivilianJob.ForgetCareer(UID);
		LF_TestCharacterFixture.Delete(user);
	}

	//------------------------------------------------------------------------------------------------
	//! Start, abandon, start, abandon: every loop costs the deposit and the cargo is worth no more than it.
	protected void RunStash(LF_TestContext ctx, IEntity user, IEntity drop)
	{
		ResourceName parcel = LF_CivilianJob.ParcelPrefab();
		int deposit = LF_CivilianJob.DepositFor(LF_EJobType.PARCEL);
		ctx.True(deposit > 0, "a parcel is held against a deposit");
		ctx.True(LF_CivilianJob.SignUp(UID, LF_EJobType.PARCEL), "the courier signs up");
		user.SetOrigin(drop.GetOrigin() + "2 0 0");

		int cash = LF_MoneyUtils.GetCash(user);
		if (cash > 0)
			LF_MoneyUtils.RemoveCash(user, cash);

		ctx.Equal(0, LF_MoneyUtils.GetCash(user), "the courier starts with no cash");
		int asked;
		ctx.True(LF_CivilianJob.BeginRun(user, UID, LF_EJobType.PARCEL, 100, asked) == LF_ERunStart.NO_DEPOSIT, "a courier short of the deposit gets no run");
		ctx.Equal(deposit, asked, "the refusal names the deposit");
		ctx.False(LF_CivilianJob.HasActiveRun(UID), "no run is left active");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(user, parcel), "and no parcel is handed over");

		LF_MoneyUtils.GiveCash(user, deposit * 3);
		ctx.Equal(deposit * 3, LF_MoneyUtils.GetCash(user), "the courier is given three deposits");

		for (int loop = 1; loop <= 2; loop++)
		{
			LF_ERunStart started = LF_ERunStart.REFUSED;
			if (!RedProof())
				started = LF_CivilianJob.BeginRun(user, UID, LF_EJobType.PARCEL, 100, asked);

			ctx.True(started == LF_ERunStart.STARTED, string.Format("loop %1: the run starts", loop));
			ctx.Equal(deposit * (3 - loop), LF_MoneyUtils.GetCash(user), string.Format("loop %1: the deposit is taken", loop));
			ctx.Equal(deposit, LF_CivilianJob.GetDeposit(UID), string.Format("loop %1: the run holds it", loop));
			ctx.Equal(1, LF_InventoryUtils.GetAmount(user, parcel), string.Format("loop %1: the parcel is in hand", loop));
			ctx.True(LF_CivilianJob.AbandonRun(UID), string.Format("loop %1: the run is abandoned", loop));
			ctx.Equal(0, LF_CivilianJob.GetDeposit(UID), string.Format("loop %1: the deposit stays with the job", loop));
			ctx.Equal(deposit * (3 - loop), LF_MoneyUtils.GetCash(user), string.Format("loop %1: abandoning gives no cash back", loop));

			// The stash: the parcel goes into a car or a bag, and the hands are free for the next run.
			ctx.Equal(1, LF_InventoryUtils.RemoveAmount(user, parcel, 1), string.Format("loop %1: the parcel is stashed", loop));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDrops(LF_TestContext ctx, IEntity user, IEntity drop, IEntity depot)
	{
		ResourceName parcel = LF_CivilianJob.ParcelPrefab();
		int deposit = LF_CivilianJob.DepositFor(LF_EJobType.PARCEL);
		LF_MoneyUtils.GiveCash(user, deposit * 2);
		int cash = LF_MoneyUtils.GetCash(user);
		int held = LF_InventoryUtils.GetAmount(user, parcel);
		int asked;
		string refusal;

		// A run that starts beside the drop pays nothing there, and nothing moves.
		user.SetOrigin(drop.GetOrigin() + "2 0 0");
		ctx.True(LF_CivilianJob.BeginRun(user, UID, LF_EJobType.PARCEL, 1000, asked) == LF_ERunStart.STARTED, "a run starts beside the drop");
		cash = cash - deposit;
		held = held + 1;
		ctx.Equal(0, LF_CivilianJob.Dropoff(user, UID, drop, 5000, refusal), "a drop beside the start pays nothing");
		ctx.EqualStr(LF_CivilianJob.KEY_TOO_CLOSE, refusal, "and says it is too close");
		ctx.True(LF_CivilianJob.HasActiveRun(UID), "the run stays open");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(user), "no cash moved");
		ctx.Equal(held, LF_InventoryUtils.GetAmount(user, parcel), "the parcel stays in hand");
		ctx.Equal(deposit, LF_CivilianJob.GetDeposit(UID), "the deposit is still held");

		// An entity that is not a drop pays nothing.
		ctx.Equal(0, LF_CivilianJob.Dropoff(user, UID, depot, 5000, refusal), "the depot counter is no drop");
		ctx.EqualStr(LF_CivilianJob.KEY_NOT_DROP, refusal, "and says so");
		ctx.True(LF_CivilianJob.AbandonRun(UID), "the run ends");
		ctx.Equal(1, LF_InventoryUtils.RemoveAmount(user, parcel, 1), "the parcel is put away so the hands are free");
		held = held - 1;

		// A run that starts far away and reaches the drop at once pays nothing.
		user.SetOrigin(drop.GetOrigin() + "400 0 0");
		ctx.True(LF_CivilianJob.BeginRun(user, UID, LF_EJobType.PARCEL, 2000, asked) == LF_ERunStart.STARTED, "a run starts 400 m out");
		cash = cash - deposit;
		held = held + 1;
		ctx.Equal(0, LF_CivilianJob.Dropoff(user, UID, drop, 2001, refusal), "a far drop reached in a second pays nothing");
		ctx.EqualStr(LF_CivilianJob.KEY_TOO_FAST, refusal, "and says it is too fast");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(user), "no cash moved");
		ctx.Equal(held, LF_InventoryUtils.GetAmount(user, parcel), "the parcel is still in hand");

		// The same run, delivered after the time it needs, pays the run and returns the deposit.
		int paid = LF_CivilianJob.Dropoff(user, UID, drop, 2100, refusal);

		ctx.Equal(LF_CivilianJob.PAY_PARCEL, paid, "the delivery pays the parcel run");
		ctx.EqualStr("", refusal, "with no refusal");
		ctx.Equal(cash + deposit + LF_CivilianJob.PAY_PARCEL, LF_MoneyUtils.GetCash(user), "the pay and the deposit are handed over");
		ctx.Equal(held - 1, LF_InventoryUtils.GetAmount(user, parcel), "the delivered parcel is taken");
		ctx.False(LF_CivilianJob.HasActiveRun(UID), "the run is over");
		ctx.Equal(0, LF_CivilianJob.GetDeposit(UID), "the deposit is not held twice");
		ctx.Equal(0, LF_CivilianJob.Dropoff(user, UID, drop, 2200, refusal), "a second drop pays nothing");
	}
}
