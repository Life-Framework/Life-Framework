// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_HospitalRespawn`:
// respawn/bill-once: not removing the uid in LF_HospitalRespawn.TakePendingWake failed it with
// `- a reconnect after waking writes no second bill`.
// respawn/bed-pick: making IsOccupied always false failed it with
// `- an occupied bed is skipped (expected 1, got 0)`.
// respawn/ward: before the Hospital_Bed_* entities were placed it failed with
// `- DebugWorld has a ward of at least three beds (found 0)`. All restored; green.

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class LF_Test_HospitalBillOnce : LF_Test
{
	override string GetName()
	{
		return "respawn/bill-once";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ATMManager.Reset();
		LF_HospitalRespawn.Reset();
		LF_TreatmentBills.ResetStatics();
		string uid = "test-hospital-once";
		LF_ATMManager.GetOrCreate(uid).Deposit(500);

		ctx.True(LF_HospitalRespawn.BillOnWake(uid, 150, "d") == null, "a spawn with no death writes no bill");
		ctx.Equal(0, LF_TreatmentBills.CountOpen(uid), "no death, no open bill");

		LF_HospitalRespawn.MarkDied(uid);
		LF_HospitalRespawn.MarkDied(uid);
		ctx.True(LF_HospitalRespawn.HasPendingWake(uid), "a death leaves a pending hospital wake");
		LF_TreatmentBillRecord bill = LF_HospitalRespawn.BillOnWake(uid, 150, "10/11/1989 08:00");
		ctx.NotNull(bill, "the wake after a death writes the bill");
		if (bill)
		{
			ctx.Equal(150, bill.m_iAmount, "the bill carries the configured amount");
			ctx.EqualStr("", bill.m_sMedicUid, "a wake bill has no medic");
			ctx.Equal(0, bill.m_iShare, "a wake bill pays no medic share");
		}

		ctx.Equal(500, LF_ATMManager.GetOrCreate(uid).GetBalance(), "the bank is never touched by the bill");
		ctx.Equal(1, LF_TreatmentBills.CountOpen(uid), "two death reports before one wake write one bill");

		// Reconnect after waking, or a second respawn request: the wake was consumed.
		ctx.False(LF_HospitalRespawn.HasPendingWake(uid), "waking consumes the pending wake");
		ctx.True(LF_HospitalRespawn.BillOnWake(uid, 150, "d") == null, "a reconnect after waking writes no second bill");
		ctx.Equal(1, LF_TreatmentBills.CountOpen(uid), "one bill per death");

		LF_HospitalRespawn.MarkDied(uid);
		ctx.NotNull(LF_HospitalRespawn.BillOnWake(uid, 150, "d"), "the next death bills again");
		ctx.Equal(2, LF_TreatmentBills.CountOpen(uid), "second death, second bill");

		ctx.True(LF_HospitalRespawn.BillOnWake("", 150, "d") == null, "an empty uid is refused");

		LF_HospitalRespawn.MarkDied("test-hospital-off");
		ctx.True(LF_HospitalRespawn.BillOnWake("test-hospital-off", 0, "d") == null, "a zero bill writes nothing");
		ctx.False(LF_HospitalRespawn.HasPendingWake("test-hospital-off"), "the wake is still consumed");

		// A saved debt on the bank still loads as debt.
		LF_ATMManager.Reset();
		array<ref LF_BankAccountRecord> records = new array<ref LF_BankAccountRecord>();
		records.Insert(LF_BankAccountRecord.Create("test-hospital-saved", -100));
		LF_ATMManager.ApplyAll(records);
		ctx.Equal(-100, LF_ATMManager.GetOrCreate("test-hospital-saved").GetBalance(), "a saved debt loads as debt");

		LF_HospitalRespawn.Reset();
		LF_TreatmentBills.ResetStatics();
		LF_ATMManager.Reset();
	}
}

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class LF_Test_HospitalBedPick : LF_Test
{
	override string GetName()
	{
		return "respawn/bed-pick";
	}

	override void Run(LF_TestContext ctx)
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
class LF_Test_HospitalWard : LF_Test
{
	override string GetName()
	{
		return "respawn/ward";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_RespawnConfig config = LF_RespawnConfig.Get();
		ctx.True(config != null, "Configs/Spawning/LF_Respawn.conf resolves");
		if (config)
		{
			ctx.True(config.m_bWakeInHospital, "the flagship wakes players in hospital");
			ctx.True(config.m_iBillAmount > 0, "the flagship bill is set");
		}

		array<vector> beds = {};
		LF_HospitalBedComponent.GetBedPositions(beds);
		ctx.True(beds.Count() >= 3, string.Format("DebugWorld has a ward of at least three beds (found %1)", beds.Count()));

		IEntity bill = LF_Utils.SpawnEntityPrefab(LF_HospitalBill.Prefab(), "0 0 0");
		ctx.True(bill != null, "the hospital bill prefab spawns");
		if (!bill)
			return;

		LF_HospitalBillComponent paper = LF_HospitalBillComponent.Cast(bill.FindComponent(LF_HospitalBillComponent));
		ctx.True(paper != null, "the bill carries LF_HospitalBillComponent");
		if (paper)
		{
			ctx.False(paper.IsIssued(), "a fresh bill is blank");
			LF_TreatmentBillRecord record = new LF_TreatmentBillRecord();
			record.m_iNumber = 7;
			record.m_iAmount = 150;
			record.m_sIssued = "12/05/1989 08:30";
			ctx.True(paper.Print(record, 15), "the server prints the bill");
			ctx.True(paper.IsIssued(), "a printed bill is no longer blank");
			ctx.Equal(150, paper.GetAmount(), "the bill shows the amount");
			ctx.False(paper.IsPaid(), "a fresh bill is unpaid");
			ctx.EqualStr("12/05/1989 08:30", paper.GetDate(), "the bill shows the date");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(bill);
	}
}
