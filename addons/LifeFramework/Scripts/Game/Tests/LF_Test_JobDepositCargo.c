// red-proof: #2481. `--red` skips ApplyDeposits, so the refund and 'the restart takes the carried cargo back' fail; on the unfixed code a restart refunded the deposit and the snapshot kept the parcel.
// Observed 2026-11-18 `--red LF_Test_JobDepositCargo,LF_Test_JobDepositRestart`: RED-OK jobs/deposit-cargo (29 assertions), RED PROOF OK. Restart proof: `tools/cli wt e2e fix-2481`.
//! A restart refunds a held cargo deposit and takes that run's cargo out of the player's saved carried
//! snapshot, so the player never keeps both (#2481). A cargo no longer carried is just refunded; a stack
//! loses one; a fuel run takes a fuel can, not a parcel; a row whose job is unknown looks for either kind.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_JobDepositCargo : LF_Test
{
	protected static const string COURIER = "job-cargo-courier";
	protected static const string DROPPER = "job-cargo-dropper";
	protected static const string STACKER = "job-cargo-stacker";
	protected static const string TANKER = "job-cargo-tanker";
	protected static const string LEGACY = "job-cargo-legacy";
	//! Any other carried gear: the strip must leave it alone.
	protected static const ResourceName OTHER_GEAR = LF_Bank.DRILL_PREFAB;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/deposit-cargo";
	}

	//------------------------------------------------------------------------------------------------
	protected void Carry(string owner, ResourceName prefab, int quantity)
	{
		LF_StorageSnapshotRecord record = LF_StorageSnapshotRecord.Create(owner, LF_StorageSnapshot.SLOT_CARRIED);
		record.m_aItems.Insert(LF_StorageItemRecord.Create(OTHER_GEAR, 1, -1, 0, 0));
		if (!prefab.IsEmpty())
			record.m_aItems.Insert(LF_StorageItemRecord.Create(prefab, quantity, -1, 0, 1));

		LF_StorageSnapshot.Set(record);
	}

	//------------------------------------------------------------------------------------------------
	protected int Held(string owner, ResourceName prefab)
	{
		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Get(owner, LF_StorageSnapshot.SLOT_CARRIED);
		if (!record)
			return -1;

		return record.CountPrefab(prefab);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ATMManager atm = LF_ATMManager.GetInstance();
		array<string> uids = { COURIER, DROPPER, STACKER, TANKER, LEGACY };
		foreach (string uid : uids)
		{
			atm.CreateAccount(uid);
			ctx.True(atm.Deposit(uid, 500), uid + " banks 500");
			LF_CivilianJob.ForgetCareer(uid);
		}

		ResourceName parcel = LF_CivilianJob.PrefabFor(LF_EJobType.PARCEL);
		ResourceName fuel = LF_CivilianJob.FUEL_PREFAB;
		Carry(COURIER, parcel, 1);
		Carry(DROPPER, "", 0);
		Carry(STACKER, parcel, 2);
		Carry(TANKER, parcel, 1);
		LF_StorageSnapshotRecord tankerRecord = LF_StorageSnapshot.Get(TANKER, LF_StorageSnapshot.SLOT_CARRIED);
		tankerRecord.m_aItems.Insert(LF_StorageItemRecord.Create(fuel, 1, -1, 0, 2));
		Carry(LEGACY, fuel, 1);

		ctx.Equal(1, Held(COURIER, parcel), "the courier carries the parcel before the restart");

		array<ref LF_JobRunRow> rows = {};
		rows.Insert(LF_JobRunRow.Create(COURIER, 150, LF_EJobType.PARCEL));
		rows.Insert(LF_JobRunRow.Create(DROPPER, 150, LF_EJobType.PARCEL));
		rows.Insert(LF_JobRunRow.Create(STACKER, 150, LF_EJobType.PARCEL));
		rows.Insert(LF_JobRunRow.Create(TANKER, 90, LF_EJobType.FUEL_DELIVERY));
		rows.Insert(LF_JobRunRow.Create(LEGACY, 90, LF_EJobType.UNEMPLOYED));

		int refunded = 0;
		if (!RedProof())
			refunded = LF_CivilianJob.ApplyDeposits(rows);

		ctx.Equal(5, refunded, "every ended run is refunded");

		ctx.Equal(650, atm.GetAccount(COURIER).GetBalance(), "the courier gets the deposit back");
		ctx.Equal(0, Held(COURIER, parcel), "the restart takes the carried parcel back: the deposit and the cargo are never both kept");
		ctx.Equal(1, Held(COURIER, OTHER_GEAR), "the rest of the courier's gear stays");

		ctx.Equal(650, atm.GetAccount(DROPPER).GetBalance(), "a run whose cargo is no longer carried is refunded");
		ctx.Equal(1, Held(DROPPER, OTHER_GEAR), "nothing else is taken from a courier who dropped the cargo");

		ctx.Equal(650, atm.GetAccount(STACKER).GetBalance(), "the stack owner is refunded once");
		ctx.Equal(1, Held(STACKER, parcel), "a stack of two loses one, the run's own");

		ctx.Equal(590, atm.GetAccount(TANKER).GetBalance(), "the fuel run is refunded its own deposit");
		ctx.Equal(1, Held(TANKER, parcel), "a fuel run leaves a parcel the player carries");
		ctx.Equal(0, Held(TANKER, fuel), "a fuel run takes the fuel can");

		ctx.Equal(590, atm.GetAccount(LEGACY).GetBalance(), "a run whose job is unknown is refunded");
		ctx.Equal(0, Held(LEGACY, fuel), "a row with no known job still finds the fuel can");

		foreach (string cleanup : uids)
		{
			LF_StorageSnapshot.Clear(cleanup, LF_StorageSnapshot.SLOT_CARRIED);
			LF_CivilianJob.ForgetCareer(cleanup);
		}
	}
}
