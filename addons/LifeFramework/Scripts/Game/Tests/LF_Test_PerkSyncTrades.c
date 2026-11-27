// red-proof: with `RedProof()` on the trade list is not asked, so every trade reads "not a counter trade" and the true checks fail.

//! level/perk-sync-trades: which job a level rise re-syncs the perk counter for (LF_PerkSync.IsCounterTrade):
//! the gathering and craft trades, police and medic, but never civilian, unemployed or an empty job.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PerkSyncTrades : LF_Test
{
	override string GetName()
	{
		return "level/perk-sync-trades";
	}

	override void Run(LF_TestContext ctx)
	{
		bool miner = false;
		bool lumberjack = false;
		bool farmer = false;
		bool police = false;
		bool medic = false;
		bool civilian = true;
		bool empty = true;
		bool lowerCase = true;
		bool unknown = true;
		if (!RedProof())
		{
			miner = LF_PerkSync.IsCounterTrade("MINER");
			lumberjack = LF_PerkSync.IsCounterTrade("LUMBERJACK");
			farmer = LF_PerkSync.IsCounterTrade("FARMER");
			police = LF_PerkSync.IsCounterTrade(LF_PerkPick.JOB_POLICE);
			medic = LF_PerkSync.IsCounterTrade(LF_PerkPick.JOB_MEDIC);
			civilian = LF_PerkSync.IsCounterTrade(LF_JobLevels.JOB_CIVILIAN);
			empty = LF_PerkSync.IsCounterTrade("");
			lowerCase = LF_PerkSync.IsCounterTrade("miner");
			unknown = LF_PerkSync.IsCounterTrade("ASTRONAUT");
		}

		ctx.True(miner, "mining is a counter trade");
		ctx.True(lumberjack, "lumber is a counter trade");
		ctx.True(farmer, "farming is a counter trade");
		ctx.True(police, "police is a counter trade");
		ctx.True(medic, "medic is a counter trade");
		ctx.False(civilian, "the civilian job is not a counter trade");
		ctx.False(empty, "no job is not a counter trade");
		ctx.False(lowerCase, "job ids are matched exactly, case included");
		ctx.False(unknown, "an unknown job is not a counter trade");
	}
}
