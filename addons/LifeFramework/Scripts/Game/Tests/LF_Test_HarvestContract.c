// red-proof: Observed 2026-11-19 with `--no-build --red LF_Test_HarvestContract`: `RED PROOF OK (all 1 listed test(s) fail with the hook on)`, the 95 % cut and the boot load skipped (no bonus, no XP, "the deposit of a kit out at the restart is back in the bank (expected 300, got 0)").
//! harvest/harvest-contract (#3297): the farm board's harvest contract. A ripe field is posted once; a second taker
//! is refused while it is held; a rental kit costs a $300 deposit and spawns a vehicle with a reaper hitched; when the
//! field is cut to 95 % through the H4 server API the taker is paid the $40 bonus and FARMER XP exactly once; the
//! kit's return pays the deposit back; a kit that is kept past the timeout is deleted and its deposit stays with the
//! farm. A taker with no reaper and no cash is refused before anything moves. A deposit out at a restart is paid
//! into the bank once on the next boot (the "harvest_deposits" section). The test deletes what it spawns.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HarvestContract : LF_Test
{
	protected static const ResourceName FIELD = "{5FF806585DB556B3}Prefabs/Harvest/LF_WheatField.et";
	protected static const vector SPOT_A = "160 0 140";
	protected static const vector SPOT_B = "260 0 140";
	protected static const vector SPOT_C = "360 0 140";
	protected static const vector BOARD = "160 0 100";
	protected static const int DEPOSIT_CASH = 300;
	protected static const string TAKER = "test-contract-taker";
	protected static const string STRANGER = "test-contract-stranger";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "harvest/harvest-contract";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_HarvestContract.ResetStatics();
		LF_OwnedImplements.ResetStatics();
		IEntity entityA = SpawnAt(FIELD, SPOT_A);
		IEntity entityB = SpawnAt(FIELD, SPOT_B);
		IEntity entityC = SpawnAt(FIELD, SPOT_C);
		LF_WheatField fieldA = LF_WheatField.Cast(entityA);
		LF_WheatField fieldB = LF_WheatField.Cast(entityB);
		LF_WheatField fieldC = LF_WheatField.Cast(entityC);
		IEntity taker = LF_TestCharacterFixture.Spawn(ctx);
		IEntity stranger = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(fieldA, "the wheat field prefab spawns");
		ctx.NotNull(fieldB, "a second wheat field spawns");
		if (fieldA && fieldB && taker && stranger)
			Check(ctx, fieldA, fieldB, taker, stranger);

		ctx.NotNull(fieldC, "a third wheat field spawns");
		if (fieldC && taker)
			CheckRestartRefund(ctx, fieldC, taker);

		LF_Utils.SetUidForTests(null, "");
		LF_HarvestContract.ResetStatics();
		LF_OwnedImplements.ResetStatics();
		if (taker)
			LF_TestCharacterFixture.Delete(taker);

		if (stranger)
			LF_TestCharacterFixture.Delete(stranger);

		if (fieldA)
			fieldA.ClearLocal();

		if (fieldB)
			fieldB.ClearLocal();

		if (entityA)
			SCR_EntityHelper.DeleteEntityAndChildren(entityA);

		if (entityB)
			SCR_EntityHelper.DeleteEntityAndChildren(entityB);

		if (fieldC)
			fieldC.ClearLocal();

		if (entityC)
			SCR_EntityHelper.DeleteEntityAndChildren(entityC);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, LF_WheatField fieldA, LF_WheatField fieldB, IEntity taker, IEntity stranger)
	{
		LF_Utils.SetUidForTests(taker, TAKER);
		ctx.Equal(500, LF_MoneyUtils.AddCash(taker, 500), "the taker is seeded with $500");
		ctx.True(LF_HarvestContract.KitPrefab(false) == LF_VehicleCatalog.UAZ469, "without the DLC the kit is a UAZ469");
		ctx.True(LF_HarvestContract.KitPrefab(true) == LF_VehicleDlcGate.Z7211, "with the DLC the kit is the Z7211 tractor");

		// Post: one contract per ripe field.
		ctx.True(LF_HarvestContract.Post(fieldA) == LF_EHarvestContract.OK, "the board posts a contract for the ripe field");
		ctx.True(LF_HarvestContract.Post(fieldA) == LF_EHarvestContract.ALREADY_POSTED, "a field gets one contract only");
		ctx.True(LF_HarvestContract.StateOf(fieldA) == LF_EHarvestContractState.OPEN, "a posted contract is open");

		// Take with a rental kit: deposit, vehicle, reaper.
		IEntity kit;
		ctx.True(LF_HarvestContract.Take(fieldA, taker, true, false, BOARD, "0 0 0", kit) == LF_EHarvestContract.OK, "the taker takes the contract with a rental kit");
		ctx.Equal(200, LF_MoneyUtils.GetCash(taker), "the $300 deposit is taken");
		ctx.NotNull(kit, "the kit vehicle spawned");
		ctx.True(LF_ImplementHitch.FindHitched(kit) != null, "the kit tows a reaper");
		ctx.False(LF_VehicleWorldSave.IsSaved(kit), "the kit is a job vehicle, kept out of the world save");
		ctx.True(LF_HarvestContract.TakerOf(fieldA) == TAKER, "the field's contract names its taker");

		// A second taker is refused and loses nothing.
		LF_Utils.SetUidForTests(stranger, STRANGER);
		ctx.Equal(400, LF_MoneyUtils.AddCash(stranger, 400), "the stranger is seeded with $400");
		IEntity none;
		ctx.True(LF_HarvestContract.Take(fieldA, stranger, true, false, BOARD, "0 0 0", none) == LF_EHarvestContract.ALREADY_TAKEN, "a second taker on the same field is refused");
		ctx.Equal(400, LF_MoneyUtils.GetCash(stranger), "the refused taker paid nothing");
		ctx.True(none == null, "the refused taker got no kit");

		// The contract stays open until the field is cut to the line.
		LF_Utils.SetUidForTests(taker, TAKER);
		ctx.True(LF_HarvestContract.Settle(fieldA, System.GetUnixTime()) == LF_EHarvestContract.NOT_DONE, "an uncut field does not complete the contract");
		int cells = fieldA.CellCount();
		int line = LF_HarvestContract.COMPLETE_PERCENT * cells / 100;
		array<int> cut = {};
		for (int i = 0; i < line - 1; i++)
		{
			cut.Insert(i);
		}

		fieldA.CutCells(cut);
		ctx.True(LF_HarvestContract.Settle(fieldA, System.GetUnixTime()) == LF_EHarvestContract.NOT_DONE, "a field just under the line does not complete the contract");

		// Cut to 95 % through the H4 server API: the bonus and the XP.
		int xpBefore = LF_JobLevels.XpOf(TAKER, LF_JobLevels.JOB_FARMER);
		cut.Clear();
		for (int j = line - 1; j < line + 1; j++)
		{
			cut.Insert(j);
		}

		if (!RedProof())
			fieldA.CutCells(cut);

		ctx.True(LF_HarvestContract.CutPercent(fieldA) >= LF_HarvestContract.COMPLETE_PERCENT, "the field is cut to the line");
		ctx.True(LF_HarvestContract.Settle(fieldA, System.GetUnixTime()) == LF_EHarvestContract.PAID, "the contract pays at 95 %");
		ctx.Equal(240, LF_MoneyUtils.GetCash(taker), "the $40 bonus is paid on top of what is left");
		int kg = LF_WheatFieldRules.YieldKg(fieldA.CutCount());
		int expectedXp = kg / 100 * LF_HarvestContract.XP_PER_100KG;
		ctx.True(expectedXp > 0, "the cut field is worth FARMER XP");
		ctx.Equal(expectedXp, LF_JobLevels.XpOf(TAKER, LF_JobLevels.JOB_FARMER) - xpBefore, "FARMER XP is paid per 100 kg cut");
		ctx.True(LF_HarvestContract.StateOf(fieldA) == LF_EHarvestContractState.DONE, "the contract is done");
		ctx.True(LF_HarvestContract.Settle(fieldA, System.GetUnixTime()) == LF_EHarvestContract.NOT_DONE, "a done contract pays nothing a second time");
		ctx.Equal(240, LF_MoneyUtils.GetCash(taker), "no second bonus");

		// The kit goes back to the board: far away refuses, at the board refunds the deposit.
		int refunded;
		LF_Utils.SetUidForTests(stranger, STRANGER);
		ctx.True(LF_HarvestContract.ReturnKit(fieldA, stranger, BOARD, refunded) == LF_EHarvestContract.NOT_TAKER, "someone else cannot return the kit");
		LF_Utils.SetUidForTests(taker, TAKER);
		ctx.True(LF_HarvestContract.ReturnKit(fieldA, taker, BOARD + "80 0 0", refunded) == LF_EHarvestContract.KIT_FAR, "a kit away from the board is not taken back");
		ctx.Equal(240, LF_MoneyUtils.GetCash(taker), "the refused return refunded nothing");
		ctx.True(LF_HarvestContract.ReturnKit(fieldA, taker, kit.GetOrigin(), refunded) == LF_EHarvestContract.OK, "the kit is returned");
		ctx.Equal(300, refunded, "the deposit comes back");
		ctx.Equal(540, LF_MoneyUtils.GetCash(taker), "the refund is in the taker's cash");
		ctx.True(LF_HarvestContract.KitOf(fieldA) == null, "the returned kit is gone");
		ctx.False(LF_HarvestContract.IsPosted(fieldA), "the finished contract is closed");
		ctx.True(LF_HarvestContract.Post(fieldA) == LF_EHarvestContract.NOT_RIPE, "a field cut to the line has nothing left to post");

		CheckRefusalsAndTimeout(ctx, fieldB, taker, stranger);
	}

	//------------------------------------------------------------------------------------------------
	//! A restart with a kit out: the save carries the open deposit, the next boot pays it into the taker's bank once,
	//! and a load while the deposit is still live in this session pays nothing.
	protected void CheckRestartRefund(LF_TestContext ctx, LF_WheatField field, IEntity taker)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_HarvestContractSavePayload.NAME);
		ctx.NotNull(section, "harvest_deposits is a registered save section");
		if (!section)
			return;

		LF_Utils.SetUidForTests(taker, TAKER);
		LF_BankAccount bank = LF_ATMManager.GetOrCreate(TAKER);
		int bankBefore = bank.GetBalance();
		LF_MoneyUtils.AddCash(taker, DEPOSIT_CASH);
		int cashBefore = LF_MoneyUtils.GetCash(taker);
		ctx.True(LF_HarvestContract.Post(field) == LF_EHarvestContract.OK, "the third field is posted");
		IEntity kit;
		ctx.True(LF_HarvestContract.Take(field, taker, true, false, BOARD, "0 0 0", kit) == LF_EHarvestContract.OK, "the taker rents a kit for the third field");
		ctx.Equal(cashBefore - LF_HarvestContract.DEPOSIT, LF_MoneyUtils.GetCash(taker), "the deposit is taken");
		if (kit)
			ctx.False(LF_VehicleWorldSave.IsSaved(LF_ImplementHitch.FindHitched(kit)), "the kit's reaper is kept out of the world save too");

		JsonSaveContext save = new JsonSaveContext(false);
		section.WriteSection(save);
		string saved = save.SaveToString();

		// A load while the deposit is live in this session (a re-apply) pays nothing.
		JsonLoadContext live = new JsonLoadContext(false);
		live.LoadFromString(saved);
		section.ReadSection(live, 1);
		ctx.Equal(bankBefore, bank.GetBalance(), "a re-apply in the session that took the deposit refunds nothing");

		// Restart: the session and the kit are gone.
		LF_HarvestContract.ResetStatics();
		if (kit)
			SCR_EntityHelper.DeleteEntityAndChildren(kit);

		JsonLoadContext boot = new JsonLoadContext(false);
		ctx.True(boot.LoadFromString(saved), "the save parses");
		// RedProof(): the boot load is skipped, so the refund never arrives (`--red`).
		if (!RedProof())
			section.ReadSection(boot, 1);

		ctx.Equal(bankBefore + LF_HarvestContract.DEPOSIT, bank.GetBalance(), "the deposit of a kit out at the restart is back in the bank");
		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		section.ReadSection(again, 1);
		ctx.Equal(bankBefore + LF_HarvestContract.DEPOSIT, bank.GetBalance(), "a second load refunds nothing twice");
		ctx.Equal(0, LF_HarvestContract.ExportDeposits().Count(), "the next save holds no deposit");
		bank.SetBalance(bankBefore);
	}

	//------------------------------------------------------------------------------------------------
	//! The second field: a taker with no reaper or no cash is refused, and an abandoned kit keeps its deposit.
	protected void CheckRefusalsAndTimeout(LF_TestContext ctx, LF_WheatField fieldB, IEntity taker, IEntity stranger)
	{
		ctx.True(LF_HarvestContract.Post(fieldB) == LF_EHarvestContract.OK, "the second field is posted");
		IEntity none;
		LF_Utils.SetUidForTests(stranger, STRANGER);
		ctx.True(LF_HarvestContract.Take(fieldB, stranger, false, false, BOARD, "0 0 0", none) == LF_EHarvestContract.NO_REAPER, "the own-reaper route needs a reaper on the owned list");
		LF_MoneyUtils.RemoveAmount(stranger, 400);
		ctx.True(LF_HarvestContract.Take(fieldB, stranger, true, false, BOARD, "0 0 0", none) == LF_EHarvestContract.NO_CASH, "a taker who cannot pay the deposit is refused");
		ctx.True(LF_HarvestContract.StateOf(fieldB) == LF_EHarvestContractState.OPEN, "the refusals left the contract open");

		IEntity kit;
		LF_Utils.SetUidForTests(taker, TAKER);
		ctx.True(LF_HarvestContract.Take(fieldB, taker, true, false, BOARD, "0 0 0", kit) == LF_EHarvestContract.OK, "the taker takes the second contract");
		ctx.Equal(240, LF_MoneyUtils.GetCash(taker), "a second deposit is taken");
		int now = System.GetUnixTime();
		ctx.True(LF_HarvestContract.Settle(fieldB, now + LF_HarvestContract.TIMEOUT_SECONDS - 60) == LF_EHarvestContract.NOT_DONE, "inside the timeout the contract keeps running");
		ctx.NotNull(LF_HarvestContract.KitOf(fieldB), "the kit is still out inside the timeout");

		ctx.True(LF_HarvestContract.Settle(fieldB, now + LF_HarvestContract.TIMEOUT_SECONDS + 60) == LF_EHarvestContract.FORFEIT, "past the timeout the contract lapses");
		ctx.Equal(240, LF_MoneyUtils.GetCash(taker), "an abandoned kit keeps the deposit");
		ctx.True(LF_HarvestContract.KitOf(fieldB) == null, "the abandoned kit is taken away");
		ctx.False(LF_HarvestContract.IsPosted(fieldB), "the lapsed contract is gone");
		ctx.True(LF_HarvestContract.Post(fieldB) == LF_EHarvestContract.OK, "the field can be posted again");

		// A taker who finishes and never returns the kit does not hold the field for ever.
		ctx.Equal(100, LF_MoneyUtils.AddCash(taker, 100), "the taker is topped up for another deposit");
		ctx.True(LF_HarvestContract.Take(fieldB, taker, true, false, BOARD, "0 0 0", kit) == LF_EHarvestContract.OK, "the taker takes the field again");
		array<int> all = {};
		for (int i = 0; i < fieldB.CellCount(); i++)
		{
			all.Insert(i);
		}

		fieldB.CutCells(all);
		ctx.True(LF_HarvestContract.Settle(fieldB, now) == LF_EHarvestContract.PAID, "the second run is paid");
		ctx.Equal(80, LF_MoneyUtils.GetCash(taker), "the second bonus is paid and the deposit still held");
		ctx.True(LF_HarvestContract.Settle(fieldB, now + LF_HarvestContract.TIMEOUT_SECONDS + 60) == LF_EHarvestContract.FORFEIT, "a done contract whose kit stays out lapses");
		ctx.Equal(80, LF_MoneyUtils.GetCash(taker), "the unreturned kit keeps the deposit");
		ctx.True(LF_HarvestContract.KitOf(fieldB) == null, "the unreturned kit is taken away");
		ctx.False(LF_HarvestContract.IsPosted(fieldB), "the field is free of the finished contract");
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAt(ResourceName prefab, vector at)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		return LF_Utils.SpawnEntityPrefab(prefab, at);
	}
}
