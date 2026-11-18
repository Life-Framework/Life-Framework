// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_JailWorkBeginRange`: RED PROOF OK; the hook puts the spot beside the worker (the unguarded Begin of main): `- a refused start runs no work animation (expected 0, got 1)`, `- a refused start leaves the prisoner free (not busy) (expected 0, got 2)`. Green on the fixed code.
// tier: WORLD (spawns two characters)
//! #2580 (#2417 item 26): LF_JailWork.Begin refused nobody for distance, only Finish checked
//! LF_WorkAnim.MAX_RANGE, so a prisoner across the cell could start a task. A worker beyond range
//! starts no task; one inside range starts one.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_JailWorkBeginRange : LF_Test
{
	override string GetName()
	{
		return "jail/work-begin-range";
	}

	override void Run(LF_TestContext ctx)
	{
		string officerUid = "work-range-officer";
		string prisonerUid = "work-range-prisoner";

		LF_Jail.Configure(60, 300, 1, 5);
		LF_Jail.SetCellOrigin("48 0 64");
		LF_JailWork.ResetStatics();
		LF_WorkAnim.ResetStatics();
		LF_WorkAnim.SetSilent(true);

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		officer.SetPoliceRank(LF_PoliceRanks.SENIOR);
		LF_PlayerAccount prisoner = LF_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(3);
		ctx.True(LF_Jail.Arrest(officerUid, prisonerUid, null), "the prisoner is serving");

		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		IEntity spot = LF_TestCharacterFixture.Spawn(ctx);
		if (!worker || !spot)
		{
			LF_TestCharacterFixture.Delete(worker);
			LF_TestCharacterFixture.Delete(spot);
			LF_WorkAnim.ResetStatics();
			LF_JailWork.ResetStatics();
			return;
		}

		// The spot stands well beyond reach of the worker.
		spot.SetOrigin(worker.GetOrigin() + Vector(LF_WorkAnim.MAX_RANGE + 20, 0, 0));
		if (RedProof())
			spot.SetOrigin(worker.GetOrigin());

		int startsBefore = LF_WorkAnim.GetStartCount();
		bool far = LF_JailWork.BeginFor(prisonerUid, spot, worker, "scrub", 30, 45, 3600, LF_EWorkAnimKind.PICK);
		ctx.False(far, "a start from beyond range is refused");
		ctx.Equal(startsBefore, LF_WorkAnim.GetStartCount(), "a refused start runs no work animation");
		ctx.Equal(LF_EJailWorkCheck.OK, LF_JailWork.Check(prisonerUid, "scrub", 30, 45, 100), "a refused start leaves the prisoner free (not busy)");

		// Now the spot is next to the worker.
		spot.SetOrigin(worker.GetOrigin() + Vector(1, 0, 0));
		bool near = LF_JailWork.BeginFor(prisonerUid, spot, worker, "scrub", 30, 45, 3600, LF_EWorkAnimKind.PICK);
		ctx.True(near, "a start within range begins a task");
		ctx.Equal(startsBefore + 1, LF_WorkAnim.GetStartCount(), "the task runs the work animation once");
		ctx.Equal(LF_EJailWorkCheck.BUSY, LF_JailWork.Check(prisonerUid, "scrub", 30, 45, 100), "the prisoner is busy with the task");

		GetGame().GetCallqueue().Remove(LF_JailWork.Finish);
		LF_WorkAnim.ResetStatics();
		LF_JailWork.ResetStatics();
		LF_TestCharacterFixture.Delete(worker);
		LF_TestCharacterFixture.Delete(spot);
	}
}
