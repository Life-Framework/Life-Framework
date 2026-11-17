// red-proof: 2026-11-17 `tools\cli test --no-build --red LF_Test_ChopShopCollect,LF_Test_RobNoRoom,LF_Test_JailEscortDrop,LF_Test_JailSelfArrest,LF_Test_JailWorkAbandon`: RED PROOF OK, all 5 failed. The hook skips the timer on a gone worker: `- after rejoining the prisoner may work again (expected 0, got 2)`. Green on the fixed code.
// tier: WORLD (needs a cell origin, like jail/work)
//! A prisoner who logs out mid-task is not left busy (crime review 2026-11-17). The work timer
//! returned on a missing worker before clearing the busy mark, so after rejoining every task
//! was refused as BUSY until the world reset.
class LF_Test_JailWorkAbandon : LF_Test
{
	override string GetName()
	{
		return "jail/work-abandon";
	}

	override void Run(LF_TestContext ctx)
	{
		string officerUid = "work-abandon-officer";
		string prisonerUid = "work-abandon-prisoner";

		LF_Jail.Configure(60, 300, 1, 5);
		LF_Jail.SetCellOrigin("48 0 64");
		LF_JailWork.ResetStatics();

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		officer.SetPoliceRank(LF_PoliceRanks.SENIOR);
		LF_PlayerAccount prisoner = LF_PlayerAccountManager.GetOrCreate(prisonerUid);
		prisoner.SetWantedLevel(3);
		ctx.True(LF_Jail.Arrest(officerUid, prisonerUid, null), "the prisoner is serving");

		LF_JailWork.Reserve(prisonerUid);
		ctx.Equal(LF_EJailWorkCheck.BUSY, LF_JailWork.Check(prisonerUid, "scrub", 30, 45, 100), "a prisoner mid-task is busy");

		// The timer fires after the prisoner logged out: no worker entity.
		if (!RedProof())
			LF_JailWork.Finish(prisonerUid, null, "0 0 0", "scrub", 30, 45);

		ctx.Equal(LF_EJailWorkCheck.OK, LF_JailWork.Check(prisonerUid, "scrub", 30, 45, 100), "after rejoining the prisoner may work again");

		LF_JailWork.ResetStatics();
	}
}
