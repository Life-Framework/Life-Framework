// red-proof: with `RedProof()` on the alias table is not asked, so no id is rewritten and the "renamed job", "renamed quest",
// "15 links", "refused loop" and "table size" checks fail.

//! persist/save-ids-records: renamed ids loading as the new ones (LF_SaveIds). A job sign-up and a turned-in quest list are
//! rewritten through the alias table before the feature applies them, an id nothing names loads as itself, ids match
//! exactly outside prefabs (case counts), a chain of 15 links is followed to its end while one of 16 is refused and loads
//! unchanged (a loop guard, never a half-followed chain), and a missing list, row or id never crashes a load.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_SaveIdsRecords : LF_Test
{
	override string GetName()
	{
		return "persist/save-ids-records";
	}

	//------------------------------------------------------------------------------------------------
	protected void AddChain(string kind, string prefix, int links)
	{
		for (int i = 0; i < links; i++)
		{
			LF_SaveIds.Add(kind, prefix + i.ToString(), prefix + (i + 1).ToString());
		}
	}

	override void Run(LF_TestContext ctx)
	{
		int tableSize = -1;
		string renamedJob = "-";
		string untouchedJob = "-";
		string wrongCaseJob = "-";
		string emptyJob = "-";
		string renamedSignUp = "-";
		string keptSignUp = "-";
		string renamedQuest = "-";
		string keptQuest = "-";
		string questOfJobKind = "-";
		string fifteenLinks = "-";
		string sixteenLinks = "-";
		int tableAfterChains = -1;
		int signUpCount = -1;
		int questCount = -1;
		bool survived = false;
		if (!RedProof())
		{
			LF_SaveIds.ResetStatics();
			LF_SaveIds.Add(LF_SaveIds.KIND_JOB, "COURIER", "DELIVERY");
			LF_SaveIds.Add(LF_SaveIds.KIND_QUEST, "old_quest", "new_quest");
			tableSize = LF_SaveIds.Count();
			renamedJob = LF_SaveIds.Resolve(LF_SaveIds.KIND_JOB, "COURIER");
			untouchedJob = LF_SaveIds.Resolve(LF_SaveIds.KIND_JOB, "FARMER");
			wrongCaseJob = LF_SaveIds.Resolve(LF_SaveIds.KIND_JOB, "courier");
			emptyJob = LF_SaveIds.Resolve(LF_SaveIds.KIND_JOB, "");
			questOfJobKind = LF_SaveIds.Resolve(LF_SaveIds.KIND_JOB, "old_quest");

			array<ref LF_JobSignUpRecord> signUps = {};
			signUps.Insert(LF_JobSignUpRecord.Create("uid-1", "COURIER"));
			signUps.Insert(null);
			signUps.Insert(LF_JobSignUpRecord.Create("uid-2", "FARMER"));
			LF_SaveIds.ResolveSignUps(signUps);
			renamedSignUp = signUps[0].m_sJob;
			keptSignUp = signUps[2].m_sJob;
			signUpCount = signUps.Count();
			LF_SaveIds.ResolveSignUps(null);

			array<ref LF_ProgressionRecord> progress = {};
			LF_ProgressionRecord withQuests = new LF_ProgressionRecord();
			withQuests.m_aTurnedInQuests = {"old_quest", "other_quest"};
			progress.Insert(withQuests);
			progress.Insert(null);
			LF_ProgressionRecord noList = new LF_ProgressionRecord();
			noList.m_aTurnedInQuests = null;
			progress.Insert(noList);
			LF_SaveIds.ResolveProgression(progress);
			renamedQuest = withQuests.m_aTurnedInQuests[0];
			keptQuest = withQuests.m_aTurnedInQuests[1];
			questCount = withQuests.m_aTurnedInQuests.Count();
			LF_SaveIds.ResolveProgression(null);

			AddChain(LF_SaveIds.KIND_PLACE, "p15_", 15);
			AddChain(LF_SaveIds.KIND_PLACE, "p16_", 16);
			fifteenLinks = LF_SaveIds.Resolve(LF_SaveIds.KIND_PLACE, "p15_0");
			sixteenLinks = LF_SaveIds.Resolve(LF_SaveIds.KIND_PLACE, "p16_0");
			tableAfterChains = LF_SaveIds.Count();
			survived = true;
			LF_SaveIds.ResetStatics();
		}

		ctx.Equal(2, tableSize, "the shipped table is empty, so only the two added aliases are in it");
		ctx.EqualStr("DELIVERY", renamedJob, "a renamed job id loads as the new one");
		ctx.EqualStr("FARMER", untouchedJob, "a job nothing renamed loads as itself");
		ctx.EqualStr("courier", wrongCaseJob, "ids match exactly outside prefabs: a different case is another id");
		ctx.EqualStr("", emptyJob, "an empty id stays empty");
		ctx.EqualStr("old_quest", questOfJobKind, "a quest alias does not rename a job of the same text");
		ctx.EqualStr("DELIVERY", renamedSignUp, "a saved sign-up for a renamed job is rewritten");
		ctx.EqualStr("FARMER", keptSignUp, "a sign-up for an unrenamed job is kept");
		ctx.Equal(3, signUpCount, "the sign-up list keeps its rows (a null row is skipped, not removed)");
		ctx.EqualStr("new_quest", renamedQuest, "a turned-in quest that was renamed loads under the new id");
		ctx.EqualStr("other_quest", keptQuest, "a turned-in quest nothing renamed is kept");
		ctx.Equal(2, questCount, "the turned-in list keeps its length");
		ctx.EqualStr("p15_15", fifteenLinks, "a chain of 15 links is followed to its end");
		ctx.EqualStr("p16_0", sixteenLinks, "a chain of 16 links is refused and the id loads unchanged");
		ctx.Equal(33, tableAfterChains, "both chains' rows are in the table (31 chain rows plus the two aliases)");
		ctx.True(survived, "a null list, a null row and a missing list never stop a load");
	}
}
