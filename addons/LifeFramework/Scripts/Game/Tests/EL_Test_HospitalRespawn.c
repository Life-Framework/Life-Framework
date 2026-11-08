// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only EL_Test_HospitalRespawn`:
// respawn/bill-once: not removing the uid in LF_HospitalRespawn.TakePendingWake failed it with
// `- a reconnect after waking sends no second bill (expected -1, got 150)`.
// respawn/bill-debt: clamping EL_BankAccount.SetBalance at 0 again failed it with
// `- a saved debt loads as debt (expected -100, got 0)`.
// respawn/bed-pick: making IsOccupied always false failed it with
// `- an occupied bed is skipped (expected 1, got 0)`.
// respawn/ward: before the Hospital_Bed_* entities were placed it failed with
// `- DebugWorld has a ward of at least three beds (found 0)`. All restored; green.

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_HospitalBillOnce : EL_Test
{
	override string GetName()
	{
		return "respawn/bill-once";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_ATMManager.Reset();
		LF_HospitalRespawn.Reset();
		string uid = "test-hospital-once";
		EL_ATMManager.GetOrCreate(uid).Deposit(500);

		ctx.Equal(-1, LF_HospitalRespawn.BillOnWake(uid, 150, true), "a spawn with no death sends no bill");
		ctx.Equal(500, EL_ATMManager.GetOrCreate(uid).GetBalance(), "first spawn leaves the bank alone");

		LF_HospitalRespawn.MarkDied(uid);
		LF_HospitalRespawn.MarkDied(uid);
		ctx.True(LF_HospitalRespawn.HasPendingWake(uid), "a death leaves a pending hospital wake");
		ctx.Equal(150, LF_HospitalRespawn.BillOnWake(uid, 150, true), "the wake after a death charges the bill");
		ctx.Equal(350, EL_ATMManager.GetOrCreate(uid).GetBalance(), "two death reports before one wake charge once");

		// Reconnect after waking, or a second respawn request: the wake was consumed.
		ctx.False(LF_HospitalRespawn.HasPendingWake(uid), "waking consumes the pending wake");
		ctx.Equal(-1, LF_HospitalRespawn.BillOnWake(uid, 150, true), "a reconnect after waking sends no second bill");
		ctx.Equal(350, EL_ATMManager.GetOrCreate(uid).GetBalance(), "the bank is charged once per death");

		LF_HospitalRespawn.MarkDied(uid);
		ctx.Equal(150, LF_HospitalRespawn.BillOnWake(uid, 150, true), "the next death bills again");
		ctx.Equal(200, EL_ATMManager.GetOrCreate(uid).GetBalance(), "second death, second bill");

		ctx.Equal(-1, LF_HospitalRespawn.BillOnWake("", 150, true), "an empty uid is refused");
		LF_HospitalRespawn.Reset();
		EL_ATMManager.Reset();
	}
}

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_HospitalBillDebt : EL_Test
{
	override string GetName()
	{
		return "respawn/bill-debt";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_BankAccount broke = EL_BankAccount.Create("test-hospital-broke");
		broke.Deposit(50);
		ctx.Equal(150, LF_HospitalRespawn.ChargeBill(broke, 150, true), "debt allowed: the whole bill is charged");
		ctx.Equal(-100, broke.GetBalance(), "a broke player goes negative by the rest of the bill");
		ctx.True(broke.IsInDebt(), "a negative balance is debt");
		ctx.False(broke.Withdraw(10), "no withdrawal while in debt");
		ctx.Equal(-100, broke.GetBalance(), "a refused withdrawal leaves the debt");

		broke.Deposit(300);
		ctx.Equal(200, broke.GetBalance(), "a later deposit pays off the debt first");
		ctx.False(broke.IsInDebt(), "the debt is cleared");

		EL_BankAccount capped = EL_BankAccount.Create("test-hospital-capped");
		capped.Deposit(50);
		ctx.Equal(50, LF_HospitalRespawn.ChargeBill(capped, 150, false), "debt off: only what the account holds is charged");
		ctx.Equal(0, capped.GetBalance(), "debt off: the balance stops at zero");

		ctx.Equal(0, LF_HospitalRespawn.ChargeBill(capped, 0, true), "a zero bill charges nothing");
		ctx.Equal(0, LF_HospitalRespawn.ChargeBill(capped, -5, true), "a negative bill charges nothing");
		ctx.Equal(0, LF_HospitalRespawn.ChargeBill(null, 150, true), "no account, no charge");

		// A debt survives the save: the record keeps the sign.
		EL_ATMManager.Reset();
		array<ref EL_BankAccountRecord> records = new array<ref EL_BankAccountRecord>();
		records.Insert(EL_BankAccountRecord.Create("test-hospital-saved", -100));
		EL_ATMManager.ApplyAll(records);
		ctx.Equal(-100, EL_ATMManager.GetOrCreate("test-hospital-saved").GetBalance(), "a saved debt loads as debt");
		EL_ATMManager.Reset();
	}
}

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_HospitalBedPick : EL_Test
{
	override string GetName()
	{
		return "respawn/bed-pick";
	}

	override void Run(EL_TestContext ctx)
	{
		array<vector> beds = {};
		beds.Insert(Vector(10, 0, 10));
		beds.Insert(Vector(12, 0, 10));
		beds.Insert(Vector(14, 0, 10));
		array<vector> nobody = {};

		ctx.Equal(-1, LF_HospitalRespawn.PickBed(new array<vector>(), nobody, 0), "no beds, no pick");
		ctx.Equal(0, LF_HospitalRespawn.PickBed(beds, nobody, 0), "an empty ward starts at the cursor");
		ctx.Equal(2, LF_HospitalRespawn.PickBed(beds, nobody, 2), "the cursor rotates through the beds");
		ctx.Equal(0, LF_HospitalRespawn.PickBed(beds, nobody, 3), "the cursor wraps");

		array<vector> occupied = {};
		occupied.Insert(Vector(10.2, 0, 10.1));
		ctx.Equal(1, LF_HospitalRespawn.PickBed(beds, occupied, 0), "an occupied bed is skipped");

		occupied.Insert(Vector(12, 0, 10));
		ctx.Equal(2, LF_HospitalRespawn.PickBed(beds, occupied, 0), "the first free bed after the cursor");

		occupied.Insert(Vector(14, 0, 10));
		ctx.Equal(1, LF_HospitalRespawn.PickBed(beds, occupied, 1), "a full ward still gives the cursor bed");

		array<vector> nearby = {};
		nearby.Insert(Vector(10, 0, 12));
		ctx.Equal(0, LF_HospitalRespawn.PickBed(beds, nearby, 0), "someone two metres away does not occupy the bed");
	}
}

//------------------------------------------------------------------------------------------------
// tier: WORLD
class EL_Test_HospitalWard : EL_Test
{
	override string GetName()
	{
		return "respawn/ward";
	}

	override void Run(EL_TestContext ctx)
	{
		LF_RespawnConfig config = LF_RespawnConfig.Get();
		ctx.True(config != null, "Configs/Spawning/LF_Respawn.conf resolves");
		if (config)
		{
			ctx.True(config.m_bWakeInHospital, "the flagship wakes players in hospital");
			ctx.True(config.m_iBillAmount > 0, "the flagship bill is set");
			ctx.True(config.m_bDebtAllowed, "the flagship allows debt");
		}

		array<vector> beds = {};
		LF_HospitalBedComponent.GetBedPositions(beds);
		ctx.True(beds.Count() >= 3, string.Format("DebugWorld has a ward of at least three beds (found %1)", beds.Count()));

		IEntity bill = EL_Utils.SpawnEntityPrefab(LF_HospitalBill.PREFAB, "0 0 0");
		ctx.True(bill != null, "the hospital bill prefab spawns");
		if (!bill)
			return;

		LF_HospitalBillComponent paper = LF_HospitalBillComponent.Cast(bill.FindComponent(LF_HospitalBillComponent));
		ctx.True(paper != null, "the bill carries LF_HospitalBillComponent");
		if (paper)
		{
			ctx.False(paper.IsIssued(), "a fresh bill is blank");
			ctx.True(paper.Issue(150, -100, "12/05/1989 08:30", 15), "the server prints the bill");
			ctx.Equal(150, paper.GetAmount(), "the bill shows the amount");
			ctx.Equal(-100, paper.GetBalanceAfter(), "the bill shows the balance after");
			ctx.EqualStr("12/05/1989 08:30", paper.GetDate(), "the bill shows the date");
			ctx.False(paper.Issue(10, 0, "x", 15), "a printed bill is not reprinted");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(bill);
	}
}
